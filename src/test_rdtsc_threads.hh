#pragma once

#include "cycles.hh"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <print>
#include <thread>
#include <vector>

inline void run_test_rdtsc_threads(std::ofstream& csv)
{
    csv << "thread,time_us,cycles\n";

    if (cycles_per_second() <= 0)
    {
        std::println("rdtsc_threads: hw cycle counter unavailable — skipping");
        return;
    }

    unsigned T = std::thread::hardware_concurrency();
    if (T == 0)
        T = 4;

    constexpr int samples_per_thread = 100'000;

    using clk = std::chrono::high_resolution_clock;

    struct ThreadResult
    {
        std::vector<uint64_t> buf_c;
        std::vector<clk::time_point> buf_t;
    };
    std::vector<ThreadResult> results(T);
    for (auto& r : results)
    {
        r.buf_c.resize(samples_per_thread);
        r.buf_t.resize(samples_per_thread);
    }

    std::atomic<int> ready_count{0};
    std::atomic<bool> go{false};

    auto worker = [&](unsigned tid)
    {
        auto& bc = results[tid].buf_c;
        auto& bt = results[tid].buf_t;
        ready_count.fetch_add(1, std::memory_order_release);
        while (!go.load(std::memory_order_acquire))
            ;
        for (int i = 0; i < samples_per_thread; ++i)
        {
            uint64_t const c = cycles_now();
            auto const t = clk::now();
            bc[i] = c;
            bt[i] = t;
        }
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

    // shared interval from per-thread first/last sample timestamps
    auto shared_start = results[0].buf_t.front();
    auto shared_end = results[0].buf_t.back();
    for (auto& r : results)
    {
        if (r.buf_t.front() > shared_start)
            shared_start = r.buf_t.front();
        if (r.buf_t.back() < shared_end)
            shared_end = r.buf_t.back();
    }
    if (shared_start >= shared_end)
    {
        std::println("rdtsc_threads: ERROR shared interval is empty");
        return;
    }

    auto count_in = [&](clk::time_point lo, clk::time_point hi) -> size_t
    {
        size_t total = 0;
        for (auto& r : results)
        {
            auto& bt = r.buf_t;
            auto it_lo = std::lower_bound(bt.begin(), bt.end(), lo);
            auto it_hi = std::upper_bound(bt.begin(), bt.end(), hi);
            total += size_t(it_hi - it_lo);
        }
        return total;
    };

    auto trim_start = shared_start;
    auto trim_end = shared_end;
    constexpr size_t target_max = 20'000;
    int trim_iters = 0;
    size_t total = count_in(trim_start, trim_end);
    while (total > target_max)
    {
        auto width = trim_end - trim_start;
        auto step = width / 10;
        trim_start += step;
        trim_end -= step;
        ++trim_iters;
        total = count_in(trim_start, trim_end);
    }

    auto const initial_us = std::chrono::duration<double, std::micro>(shared_end - shared_start).count();
    auto const trim_us = std::chrono::duration<double, std::micro>(trim_end - trim_start).count();
    std::println("rdtsc_threads: {} threads, shared = {:.1f} us, trimmed to {:.1f} us after {} iters, {} samples", //
                 T, initial_us, trim_us, trim_iters, total);

    for (unsigned tid = 0; tid < T; ++tid)
    {
        auto& bc = results[tid].buf_c;
        auto& bt = results[tid].buf_t;
        auto it_lo = std::lower_bound(bt.begin(), bt.end(), trim_start);
        auto it_hi = std::upper_bound(bt.begin(), bt.end(), trim_end);
        for (auto it = it_lo; it != it_hi; ++it)
        {
            size_t const idx = size_t(it - bt.begin());
            double const time_us = std::chrono::duration<double, std::micro>(*it - trim_start).count();
            csv << tid << "," << time_us << "," << bc[idx] << "\n";
        }
    }
    std::fflush(stdout);
}
