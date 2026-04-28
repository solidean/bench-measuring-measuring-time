#pragma once

#include "cycles.hh"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <print>

inline void run_test_rdtsc_drift_long(std::ofstream& csv)
{
    csv << "time_s,cps\n";

    if (cycles_per_second() <= 0)
    {
        std::println("rdtsc_drift_long: hw cycle counter unavailable — skipping");
        return;
    }

    using clk = std::chrono::high_resolution_clock;
    constexpr int samples = 50'000;
    constexpr double total_secs = 60.0;
    constexpr double per_sample_secs = total_secs / samples;

    auto const test_start = clk::now();
    uint64_t c_prev = cycles_now();
    auto t_prev = test_start;

    std::println("rdtsc_drift_long: {} samples × ~{:.2f} ms each (~{:.0f} s total)", //
                 samples, per_sample_secs * 1e3, total_secs);
    std::fflush(stdout);

    for (int i = 0; i < samples; ++i)
    {
        while (std::chrono::duration<double>(clk::now() - t_prev).count() < per_sample_secs)
        {
            // pace each sample to ~per_sample_secs
        }

        auto const t_now = clk::now();
        uint64_t const c_now = cycles_now();

        double const dt = std::chrono::duration<double>(t_now - t_prev).count();
        uint64_t const dc = c_now - c_prev;
        double const cps = double(dc) / dt;
        double const elapsed = std::chrono::duration<double>(t_now - test_start).count();

        csv << elapsed << "," << cps << "\n";

        t_prev = t_now;
        c_prev = c_now;
    }

    auto const total = std::chrono::duration<double>(clk::now() - test_start).count();
    std::println("rdtsc_drift_long: done, {:.2f} s elapsed", total);
    std::fflush(stdout);
}
