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
#include <vector>

inline void run_test_rdtsc_step_mt(std::ofstream& csv)
{
    csv << "diff,tight\n";

    if (cycles_per_second() <= 0)
    {
        std::println("rdtsc_step_mt: hw cycle counter unavailable on this arch — skipping");
        return;
    }

    unsigned T = std::thread::hardware_concurrency();
    if (T == 0)
        T = 4;

    constexpr int per_iter = 8;
    constexpr int entries_per_thread = 100'000;
    constexpr int iterations = entries_per_thread / per_iter;
    constexpr int actual_entries = iterations * per_iter;

    struct ThreadResult
    {
        std::vector<uint64_t> cycles;
        std::chrono::steady_clock::time_point start_ts;
        std::chrono::steady_clock::time_point end_ts;
    };
    std::vector<ThreadResult> results(T);
    for (auto& r : results)
        r.cycles.resize(size_t(actual_entries));

    std::atomic<int> ready_count{0};
    std::atomic<bool> go{false};

    auto worker = [&](unsigned tid)
    {
        auto& buf = results[tid].cycles;

        ready_count.fetch_add(1, std::memory_order_release);
        while (!go.load(std::memory_order_acquire))
            ; // spin

        auto const t_start = std::chrono::steady_clock::now();

        for (int i = 0; i < iterations; ++i)
        {
            uint64_t const c0 = cycles_now();
            uint64_t const c1 = cycles_now();
            uint64_t const c2 = cycles_now();
            uint64_t const c3 = cycles_now();
            uint64_t const c4 = cycles_now();
            uint64_t const c5 = cycles_now();
            uint64_t const c6 = cycles_now();
            uint64_t const c7 = cycles_now();
            int const base = i * per_iter;
            buf[base + 0] = c0;
            buf[base + 1] = c1;
            buf[base + 2] = c2;
            buf[base + 3] = c3;
            buf[base + 4] = c4;
            buf[base + 5] = c5;
            buf[base + 6] = c6;
            buf[base + 7] = c7;
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

    // shared interval
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
        std::println("rdtsc_step_mt: ERROR shared interval is empty — threads did not overlap");
        return;
    }

    auto const shared_span_s = std::chrono::duration<double>(shared_end - shared_start).count();
    std::println("rdtsc_step_mt: {} threads, shared interval = {:.3f} ms, {} entries each", //
                 T, shared_span_s * 1e3, actual_entries);

    // trim each thread to shared interval (linear interpolation between start_ts/end_ts)
    struct TrimRange
    {
        int lo;
        int hi;
    }; // inclusive on both ends
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

    // diffs
    size_t total_diffs = 0;
    for (unsigned tid = 0; tid < T; ++tid)
    {
        auto const& buf = results[tid].cycles;
        auto const lo = ranges[tid].lo;
        auto const hi = ranges[tid].hi;
        for (int k = lo; k < hi; ++k)
        {
            uint64_t const d = buf[k + 1] - buf[k];
            // every 8 entries (from original index 0) starts a new loop iteration,
            // so the diff crossing original index 7→8, 15→16, ... is a wraparound.
            bool const wraparound = (k % per_iter == per_iter - 1);
            csv << d << "," << (wraparound ? 0 : 1) << "\n";
            ++total_diffs;
        }
    }
    std::println("  wrote {} diffs", total_diffs);

    // value-occurrence frequencies across all threads (in the shared interval)
    std::unordered_map<uint64_t, uint32_t> freq;
    freq.reserve(size_t(T) * size_t(actual_entries));
    for (unsigned tid = 0; tid < T; ++tid)
    {
        auto const& buf = results[tid].cycles;
        auto const lo = ranges[tid].lo;
        auto const hi = ranges[tid].hi;
        for (int k = lo; k <= hi; ++k)
            ++freq[buf[k]];
    }

    std::map<uint32_t, size_t> freq_of_freq;
    for (auto const& [v, c] : freq)
        ++freq_of_freq[c];

    std::println("rdtsc value occurrences across {} threads (shared interval):", T);
    for (auto const& [count, n] : freq_of_freq)
        std::println("  observed {}x: {} unique values", count, n);
    std::fflush(stdout);
}
