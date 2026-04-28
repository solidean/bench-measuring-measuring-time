#pragma once

#include "bench_stats.hh"
#include "cycles.hh"
#include "methods.hh"

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

    constexpr int max_samples = 10'000;
    constexpr double max_secs = 1.0;
    constexpr uint64_t max_inner = 1'000'000'000ull;

    std::vector<stats_row> all_stats;
    std::vector<double> latencies_ns;

    for (auto const& m : time_methods())
    {
        int const target = m.target_sample_count_for(max_samples, max_secs);
        std::println("granularity_rdtsc: {} (target {} samples)", m.name, target);
        std::fflush(stdout);

        latencies_ns.clear();
        latencies_ns.reserve(target);

        uint64_t c_prev = cycles_now();
        uint64_t t_prev = m.now();

        for (int i = 0; i < target; ++i)
        {
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
