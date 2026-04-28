#pragma once

#include "cycles.hh"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <map>
#include <print>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

inline void run_test_rdtscp_step_mt(std::ofstream& csv)
{
    csv << "diff,tight\n";

#ifndef BENCH_TIME_HAS_RDTSCP
    std::println("rdtscp_step_mt: rdtscp unavailable on this arch — skipping");
    return;
#else
    unsigned T = std::thread::hardware_concurrency();
    if (T == 0)
        T = 4;

    // 4-unrolled (not 8): rdtscp needs 2 GPRs per call (cycles + aux), so 8x
    // unrolling spills on x64 where we'd want all results live simultaneously.
    constexpr int per_iter = 4;
    constexpr int entries_per_thread = 100'000;
    constexpr int iterations = entries_per_thread / per_iter;
    constexpr int actual_entries = iterations * per_iter;

    struct ThreadResult
    {
        std::vector<uint64_t> cycles;
        std::vector<uint32_t> aux; // procid signature per call
        std::chrono::steady_clock::time_point start_ts;
        std::chrono::steady_clock::time_point end_ts;
    };
    std::vector<ThreadResult> results(T);
    for (auto& r : results)
    {
        r.cycles.resize(size_t(actual_entries));
        r.aux.resize(size_t(actual_entries));
    }

    std::atomic<int> ready_count{0};
    std::atomic<bool> go{false};

    auto worker = [&](unsigned tid)
    {
        auto& buf_c = results[tid].cycles;
        auto& buf_a = results[tid].aux;

        ready_count.fetch_add(1, std::memory_order_release);
        while (!go.load(std::memory_order_acquire))
            ;

        auto const t_start = std::chrono::steady_clock::now();

        for (int i = 0; i < iterations; ++i)
        {
            unsigned int a0, a1, a2, a3;
            uint64_t const c0 = __rdtscp(&a0);
            uint64_t const c1 = __rdtscp(&a1);
            uint64_t const c2 = __rdtscp(&a2);
            uint64_t const c3 = __rdtscp(&a3);
            int const base = i * per_iter;
            buf_c[base + 0] = c0;
            buf_c[base + 1] = c1;
            buf_c[base + 2] = c2;
            buf_c[base + 3] = c3;
            buf_a[base + 0] = a0;
            buf_a[base + 1] = a1;
            buf_a[base + 2] = a2;
            buf_a[base + 3] = a3;
        }

        auto const t_end = std::chrono::steady_clock::now();
        results[tid].start_ts = t_start;
        results[tid].end_ts = t_end;
    };

    std::vector<std::thread> threads;
    threads.reserve(T);
    for (unsigned i = 0; i < T; ++i)
        threads.emplace_back(worker, i);

    while (ready_count.load(std::memory_order_acquire) < int(T))
        ;
    go.store(true, std::memory_order_release);

    for (auto& t : threads)
        t.join();

    auto shared_start = results[0].start_ts;
    auto shared_end = results[0].end_ts;
    for (auto& r : results)
    {
        if (r.start_ts > shared_start)
            shared_start = r.start_ts;
        if (r.end_ts < shared_end)
            shared_end = r.end_ts;
    }
    if (shared_start >= shared_end)
    {
        std::println("rdtscp_step_mt: ERROR shared interval is empty — threads did not overlap");
        return;
    }

    auto const shared_span_s = std::chrono::duration<double>(shared_end - shared_start).count();
    std::println("rdtscp_step_mt: {} threads, shared interval = {:.3f} ms, {} entries each", //
                 T, shared_span_s * 1e3, actual_entries);

    struct TrimRange
    {
        int lo;
        int hi;
    };
    std::vector<TrimRange> ranges(T);
    for (unsigned i = 0; i < T; ++i)
    {
        auto const& r = results[i];
        auto const span_s = std::chrono::duration<double>(r.end_ts - r.start_ts).count();
        auto const t_lo_s = std::chrono::duration<double>(shared_start - r.start_ts).count();
        auto const t_hi_s = std::chrono::duration<double>(shared_end - r.start_ts).count();
        int const lo = int(std::ceil(t_lo_s / span_s * (actual_entries - 1)));
        int const hi = int(std::floor(t_hi_s / span_s * (actual_entries - 1)));
        ranges[i].lo = std::max(0, lo);
        ranges[i].hi = std::min(actual_entries - 1, hi);
    }

    size_t total_diffs = 0;
    for (unsigned tid = 0; tid < T; ++tid)
    {
        auto const& buf = results[tid].cycles;
        auto const lo = ranges[tid].lo;
        auto const hi = ranges[tid].hi;
        for (int k = lo; k < hi; ++k)
        {
            uint64_t const d = buf[k + 1] - buf[k];
            bool const wraparound = (k % per_iter == per_iter - 1);
            csv << d << "," << (wraparound ? 0 : 1) << "\n";
            ++total_diffs;
        }
    }
    std::println("  wrote {} diffs", total_diffs);

    // (cycles, procid) pair frequency map — empirical test that the pair is unique
    struct PairHash
    {
        size_t operator()(std::pair<uint64_t, uint32_t> const& p) const noexcept
        {
            size_t h = std::hash<uint64_t>{}(p.first);
            h ^= std::hash<uint32_t>{}(p.second) + 0x9e3779b9ull + (h << 6) + (h >> 2);
            return h;
        }
    };
    std::unordered_map<std::pair<uint64_t, uint32_t>, uint32_t, PairHash> freq;
    freq.reserve(size_t(T) * size_t(actual_entries));
    for (unsigned tid = 0; tid < T; ++tid)
    {
        auto const& bc = results[tid].cycles;
        auto const& ba = results[tid].aux;
        auto const lo = ranges[tid].lo;
        auto const hi = ranges[tid].hi;
        for (int k = lo; k <= hi; ++k)
            ++freq[{bc[k], ba[k]}];
    }

    std::map<uint32_t, size_t> freq_of_freq;
    for (auto const& [pair, c] : freq)
        ++freq_of_freq[c];

    std::println("(cycles, procid) pair occurrences across {} threads (shared interval):", T);
    for (auto const& [count, n] : freq_of_freq)
        std::println("  observed {}x: {} unique pairs", count, n);
    std::fflush(stdout);
#endif
}
