#pragma once

#include <chrono>
#include <cstdint>

#if defined(_M_X64) || defined(__x86_64__) || defined(_M_IX86) || defined(__i386__)
#define BENCH_TIME_HAS_RDTSC 1
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <x86intrin.h>
#endif
#elif defined(_M_ARM64) || defined(__aarch64__)
#define BENCH_TIME_HAS_CNTVCT 1
#endif

inline uint64_t cycles_now()
{
#ifdef BENCH_TIME_HAS_RDTSC
    return __rdtsc();
#elif defined(BENCH_TIME_HAS_CNTVCT)
    uint64_t v;
    asm volatile("mrs %0, cntvct_el0" : "=r"(v));
    return v;
#else
    return 0;
#endif
}

inline double cycles_per_second()
{
#ifdef BENCH_TIME_HAS_CNTVCT
    uint64_t freq;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(freq));
    return double(freq);
#elif defined(BENCH_TIME_HAS_RDTSC)
    static double const cps = []() {
        using clk = std::chrono::steady_clock;
        auto const t0 = clk::now();
        auto const c0 = cycles_now();
        while (std::chrono::duration<double>(clk::now() - t0).count() < 0.05)
        {
            // spin
        }
        auto const t1 = clk::now();
        auto const c1 = cycles_now();
        return double(c1 - c0) / std::chrono::duration<double>(t1 - t0).count();
    }();
    return cps;
#else
    return 0.0;
#endif
}
