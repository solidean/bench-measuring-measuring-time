#pragma once

#include "cycles.hh"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <numeric>
#include <print>
#include <vector>

inline void run_test_rdtscp_step(std::ofstream& csv)
{
    csv << "cycles,sample_idx\n";

#ifndef BENCH_TIME_HAS_RDTSCP
    std::println("rdtscp_step: rdtscp unavailable on this arch — skipping");
    return;
#else
    // 4-unrolled (not 8): rdtscp needs 2 GPRs per call (cycles + aux), so 8x
    // unrolling spills on x64 where we'd want all 8 results live simultaneously.
    constexpr int target_deltas = 100'000;
    constexpr int per_iter = 3;
    int const iterations = (target_deltas + per_iter - 1) / per_iter;

    std::vector<uint64_t> deltas;
    deltas.reserve(size_t(iterations) * per_iter);

    for (int i = 0; i < iterations; ++i)
    {
        unsigned int a0, a1, a2, a3;
        uint64_t const t0 = __rdtscp(&a0);
        uint64_t const t1 = __rdtscp(&a1);
        uint64_t const t2 = __rdtscp(&a2);
        uint64_t const t3 = __rdtscp(&a3);
        deltas.push_back(t1 - t0);
        deltas.push_back(t2 - t1);
        deltas.push_back(t3 - t2);
    }

    for (size_t i = 0; i < deltas.size(); ++i)
        csv << deltas[i] << "," << i << "\n";

    auto sorted = deltas;
    std::sort(sorted.begin(), sorted.end());
    auto const mean = std::accumulate(sorted.begin(), sorted.end(), 0.0) / double(sorted.size());
    std::println("rdtscp_step: {} deltas — min={} p50={} mean={:.1f} max={} cycles", //
                 sorted.size(), sorted.front(), sorted[sorted.size() / 2], mean, sorted.back());
    std::fflush(stdout);
#endif
}
