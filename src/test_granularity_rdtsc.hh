#pragma once

#include "bench_stats.hh"
#include "cycles.hh"
#include "methods.hh"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <print>
#include <vector>

inline void run_test_granularity_rdtsc(std::ofstream& csv)
{
    csv << "method,color,latency_ns,sample_idx\n";

    auto const cps = cycles_per_second();
    if (cps <= 0)
    {
        std::println("granularity_rdtsc: hw cycle counter unavailable on this arch — skipping");
        return;
    }
    double const inv_cps = 1.0 / cps;

    using clk = std::chrono::steady_clock;
    constexpr int target_samples = 10'000;
    constexpr double budget_seconds = 1.0;
    constexpr uint64_t max_inner = 1'000'000'000ull;

    std::vector<stats_row> all_stats;
    std::vector<double> latencies_ns;

    for (auto const& m : time_methods())
    {
        std::println("granularity_rdtsc: {}", m.name);
        std::fflush(stdout);

        latencies_ns.clear();
        latencies_ns.reserve(target_samples);

        auto const wall_start = clk::now();
        uint64_t c_prev = cycles_now();
        uint64_t t_prev = m.now();

        for (int i = 0; i < target_samples; ++i)
        {
            auto const elapsed = std::chrono::duration<double>(clk::now() - wall_start).count();
            if (elapsed >= budget_seconds)
                break;

            uint64_t t_cur = t_prev;
            uint64_t c_cur = c_prev;
            uint64_t inner = 0;
            while (t_cur == t_prev && inner < max_inner)
            {
                c_cur = cycles_now();
                t_cur = m.now();
                ++inner;
            }
            if (inner >= max_inner)
                break;

            if (t_cur < t_prev)
            {
                t_prev = t_cur;
                c_prev = c_cur;
                continue;
            }

            double const delta_s = double(c_cur - c_prev) * inv_cps;
            latencies_ns.push_back(delta_s * 1e9);
            t_prev = t_cur;
            c_prev = c_cur;
        }

        for (size_t i = 0; i < latencies_ns.size(); ++i)
            csv << m.name << "," << m.color << "," << latencies_ns[i] << "," << i << "\n";

        all_stats.push_back(compute_stats(m.name, latencies_ns));
        std::println("  {} samples", latencies_ns.size());
        std::fflush(stdout);
    }

    print_summary("granularity_rdtsc", all_stats);
}
