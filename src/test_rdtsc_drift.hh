#pragma once

#include "cycles.hh"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <print>
#include <vector>

inline void run_test_rdtsc_drift(std::ofstream& csv)
{
    csv << "cycles_diff,sample_idx\n";

    auto const cps = cycles_per_second();
    if (cps <= 0)
    {
        std::println("rdtsc_drift: hw cycle counter unavailable — skipping");
        return;
    }

    using clk = std::chrono::high_resolution_clock;
    constexpr int samples = 1000;
    constexpr auto budget = std::chrono::milliseconds(1);

    std::vector<uint64_t> diffs;
    diffs.reserve(samples);

    for (int i = 0; i < samples; ++i)
    {
        auto const t_start = clk::now();
        auto const c_start = cycles_now();
        while (clk::now() - t_start < budget)
        {
            // spin
        }
        auto const c_end = cycles_now();
        diffs.push_back(c_end - c_start);
    }

    for (size_t i = 0; i < diffs.size(); ++i)
        csv << diffs[i] << "," << i << "\n";

    auto sorted = diffs;
    std::sort(sorted.begin(), sorted.end());
    auto const expected = uint64_t(cps * 1e-3);
    std::println("rdtsc_drift: {} samples — min={} p50={} max={} cycles (expected ~{} for 1 ms)", //
                 sorted.size(), sorted.front(), sorted[sorted.size() / 2], sorted.back(), expected);
    std::fflush(stdout);
}
