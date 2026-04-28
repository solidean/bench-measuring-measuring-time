#pragma once

#include "cycles.hh"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <numeric>
#include <print>
#include <vector>

// 8 unrolled cycle reads per iteration, recording the 7 pairwise deltas.
// Each read goes to its own local — no work in between besides whatever
// the compiler emits to combine EDX:EAX (rdtsc) or read cntvct_el0.
inline void run_test_rdtsc_step(std::ofstream& csv)
{
    csv << "cycles,sample_idx\n";

    if (cycles_per_second() <= 0)
    {
        std::println("rdtsc_step: hw cycle counter unavailable on this arch — skipping");
        return;
    }

    constexpr int target_deltas = 100'000;
    constexpr int per_iter = 7;
    int const iterations = (target_deltas + per_iter - 1) / per_iter;

    std::vector<uint64_t> deltas;
    deltas.reserve(size_t(iterations) * per_iter);

    for (int i = 0; i < iterations; ++i)
    {
        uint64_t const t0 = cycles_now();
        uint64_t const t1 = cycles_now();
        uint64_t const t2 = cycles_now();
        uint64_t const t3 = cycles_now();
        uint64_t const t4 = cycles_now();
        uint64_t const t5 = cycles_now();
        uint64_t const t6 = cycles_now();
        uint64_t const t7 = cycles_now();
        deltas.push_back(t1 - t0);
        deltas.push_back(t2 - t1);
        deltas.push_back(t3 - t2);
        deltas.push_back(t4 - t3);
        deltas.push_back(t5 - t4);
        deltas.push_back(t6 - t5);
        deltas.push_back(t7 - t6);
    }

    for (size_t i = 0; i < deltas.size(); ++i)
        csv << deltas[i] << "," << i << "\n";

    auto sorted = deltas;
    std::sort(sorted.begin(), sorted.end());
    auto const mean = std::accumulate(sorted.begin(), sorted.end(), 0.0) / double(sorted.size());
    std::println("rdtsc_step: {} deltas — min={} p50={} mean={:.1f} max={} cycles", //
                 sorted.size(), sorted.front(), sorted[sorted.size() / 2], mean, sorted.back());
    std::fflush(stdout);
}
