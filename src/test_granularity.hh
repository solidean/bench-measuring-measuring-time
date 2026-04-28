#pragma once

#include "methods.hh"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <format>
#include <fstream>
#include <numeric>
#include <print>
#include <string>
#include <vector>

inline std::string fmt_ns(double ns)
{
    if (!std::isfinite(ns) || ns <= 0)
        return "-";
    if (ns < 1e3)
        return std::format("{:.2f} ns", ns);
    if (ns < 1e6)
        return std::format("{:.2f} us", ns / 1e3);
    if (ns < 1e9)
        return std::format("{:.2f} ms", ns / 1e6);
    return std::format("{:.2f} s", ns / 1e9);
}

inline void run_test_granularity(std::ofstream& csv)
{
    csv << "method,color,latency_ns,sample_idx\n";

    using clk = std::chrono::steady_clock;
    constexpr int target_samples = 10'000;
    constexpr double budget_seconds = 1.0;
    constexpr uint64_t max_inner = 1'000'000'000ull;

    struct stats_row
    {
        char const* name;
        size_t samples;
        double min_ns;
        double p50_ns;
        double mean_ns;
        double max_ns;
    };
    std::vector<stats_row> all_stats;

    std::vector<double> latencies_ns;

    for (auto const& m : time_methods())
    {
        std::println("granularity: {}", m.name);
        std::fflush(stdout);

        latencies_ns.clear();
        latencies_ns.reserve(target_samples);

        auto const wall_start = clk::now();
        uint64_t t_prev = m.now();

        for (int i = 0; i < target_samples; ++i)
        {
            auto const elapsed = std::chrono::duration<double>(clk::now() - wall_start).count();
            if (elapsed >= budget_seconds)
                break;

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

        if (latencies_ns.empty())
        {
            all_stats.push_back({m.name, 0, 0, 0, 0, 0});
        }
        else
        {
            auto sorted = latencies_ns;
            std::sort(sorted.begin(), sorted.end());
            double const mean = std::accumulate(sorted.begin(), sorted.end(), 0.0) / double(sorted.size());
            all_stats.push_back({
                m.name,
                sorted.size(),
                sorted.front(),
                sorted[sorted.size() / 2],
                mean,
                sorted.back(),
            });
        }
        std::println("  {} samples", latencies_ns.size());
        std::fflush(stdout);
    }

    // human-readable summary
    std::println("");
    std::println("granularity summary:");
    std::println("  {:<40}  {:>8}  {:>11}  {:>11}  {:>11}  {:>11}", //
                 "method", "samples", "min", "median", "mean", "max");
    std::println("  {:-<40}  {:->8}  {:->11}  {:->11}  {:->11}  {:->11}", //
                 "", "", "", "", "", "");
    for (auto const& s : all_stats)
    {
        std::println("  {:<40}  {:>8}  {:>11}  {:>11}  {:>11}  {:>11}", //
                     s.name, s.samples,                                  //
                     fmt_ns(s.min_ns), fmt_ns(s.p50_ns), fmt_ns(s.mean_ns), fmt_ns(s.max_ns));
    }
    std::fflush(stdout);
}
