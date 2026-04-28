#pragma once

#include "bench_stats.hh"
#include "methods.hh"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <print>
#include <vector>

inline void run_test_calls_per_change(std::ofstream& csv)
{
    csv << "method,color,calls,sample_idx\n";

    constexpr int max_samples = 10'000;
    constexpr double max_secs = 1.0;
    constexpr uint64_t max_inner = 1'000'000'000ull;

    std::vector<stats_row> all_stats;
    std::vector<double> calls_per_sample;

    for (auto const& m : time_methods())
    {
        int const target = m.target_sample_count_for(max_samples, max_secs);
        std::println("calls_per_change: {} (target {} samples)", m.name, target);
        std::fflush(stdout);

        calls_per_sample.clear();
        calls_per_sample.reserve(target);

        uint64_t t_prev = m.now();

        for (int i = 0; i < target; ++i)
        {
            uint64_t t_cur = t_prev;
            uint64_t count = 0;
            while (t_cur == t_prev && count < max_inner)
            {
                t_cur = m.now();
                ++count;
            }
            if (count >= max_inner)
                break;

            if (t_cur < t_prev)
            {
                t_prev = t_cur;
                continue;
            }

            calls_per_sample.push_back(double(count));
            t_prev = t_cur;
        }

        for (size_t i = 0; i < calls_per_sample.size(); ++i)
            csv << m.name << "," << m.color << "," << uint64_t(calls_per_sample[i]) << "," << i << "\n";

        all_stats.push_back(compute_stats(m.name, calls_per_sample));
        std::println("  {} samples", calls_per_sample.size());
        std::fflush(stdout);
    }

    print_summary("calls_per_change", all_stats, &fmt_count);
}
