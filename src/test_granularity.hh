#pragma once

#include "bench_stats.hh"
#include "methods.hh"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <print>
#include <vector>

inline void run_test_granularity(std::ofstream& csv)
{
    csv << "method,color,latency_ns,sample_idx\n";

    constexpr int max_samples = 10'000;
    constexpr double max_secs = 1.0;
    constexpr uint64_t max_inner = 1'000'000'000ull;

    std::vector<stats_row> all_stats;
    std::vector<double> latencies_ns;

    for (auto const& m : time_methods())
    {
        int const target = m.target_sample_count_for(max_samples, max_secs);
        std::println("granularity: {} (target {} samples)", m.name, target);
        std::fflush(stdout);

        latencies_ns.clear();
        latencies_ns.reserve(target);

        uint64_t t_prev = m.now();

        for (int i = 0; i < target; ++i)
        {
            uint64_t t_cur = t_prev;
            uint64_t inner = 0;
            while (t_cur == t_prev && inner < max_inner)
            {
                t_cur = m.now();
                ++inner;
            }
            if (inner >= max_inner)
                break;

            // some clocks (system_clock under adjustment) can move backwards — skip
            if (t_cur < t_prev)
            {
                t_prev = t_cur;
                continue;
            }

            double const delta_s = double(t_cur - t_prev) * m.scale_to_seconds;
            latencies_ns.push_back(delta_s * 1e9);
            t_prev = t_cur;
        }

        for (size_t i = 0; i < latencies_ns.size(); ++i)
            csv << m.name << "," << m.color << "," << latencies_ns[i] << "," << i << "\n";

        all_stats.push_back(compute_stats(m.name, latencies_ns));
        std::println("  {} samples", latencies_ns.size());
        std::fflush(stdout);
    }

    print_summary("granularity", all_stats);
}
