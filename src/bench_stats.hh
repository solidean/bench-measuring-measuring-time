#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <format>
#include <numeric>
#include <print>
#include <string>
#include <string_view>
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

inline std::string fmt_count(double n)
{
    if (!std::isfinite(n) || n < 0)
        return "-";
    if (n < 1000)
        return std::format("{:.0f}", n);
    if (n < 1e6)
        return std::format("{:.2f}k", n / 1e3);
    if (n < 1e9)
        return std::format("{:.2f}M", n / 1e6);
    return std::format("{:.2f}G", n / 1e9);
}

struct stats_row
{
    char const* name;
    size_t samples;
    double v_min;
    double v_p50;
    double v_mean;
    double v_max;
};

inline stats_row compute_stats(char const* name, std::vector<double> values)
{
    if (values.empty())
        return {name, 0, 0, 0, 0, 0};
    std::sort(values.begin(), values.end());
    double const mean = std::accumulate(values.begin(), values.end(), 0.0) / double(values.size());
    return {
        name,
        values.size(),
        values.front(),
        values[values.size() / 2],
        mean,
        values.back(),
    };
}

inline void print_summary(std::string_view test_name,
                          std::vector<stats_row> const& all_stats,
                          std::string (*fmt)(double) = &fmt_ns)
{
    std::println("");
    std::println("{} summary:", test_name);
    std::println("  {:<40}  {:>8}  {:>11}  {:>11}  {:>11}  {:>11}", //
                 "method", "samples", "min", "median", "mean", "max");
    std::println("  {:-<40}  {:->8}  {:->11}  {:->11}  {:->11}  {:->11}", //
                 "", "", "", "", "", "");
    for (auto const& s : all_stats)
    {
        std::println("  {:<40}  {:>8}  {:>11}  {:>11}  {:>11}  {:>11}", //
                     s.name, s.samples,                                 //
                     fmt(s.v_min), fmt(s.v_p50), fmt(s.v_mean), fmt(s.v_max));
    }
    std::fflush(stdout);
}
