#pragma once

#include "cycles.hh"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <span>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#if defined(__linux__) || defined(__APPLE__)
#include <sys/time.h>
#include <time.h>
#endif

#ifdef __APPLE__
#include <mach/mach_time.h>
#endif

struct TimeMethod
{
    char const* name;
    char const* color;
    uint64_t (*now)();
    double scale_to_seconds;

    // Two consecutive change observations, returning the second interval as
    // a rough granularity estimate (in seconds). Cheap: ~2× the granularity.
    double estimate_granularity_secs_rough() const
    {
        uint64_t const t0 = now();
        uint64_t t1 = t0;
        while (t1 == t0)
            t1 = now();
        uint64_t t2 = t1;
        while (t2 == t1)
            t2 = now();
        return double(t2 - t1) * scale_to_seconds;
    }

    // Pick a sample budget that won't blow past `max_secs`, using the rough
    // granularity estimate. Tests can then run a flat target_count loop with
    // no per-iteration wall-clock check (which would otherwise distort fine
    // clock measurements).
    int target_sample_count_for(int max_samples, double max_secs) const
    {
        auto const g = estimate_granularity_secs_rough();
        if (g <= 0)
            return max_samples;
        auto const by_time = static_cast<long long>(max_secs / g);
        return int(std::min<long long>(max_samples, std::max<long long>(1, by_time)));
    }
};

namespace bench_time
{

// ── C++ chrono ───────────────────────────────────────────────────────────────

template <typename Clock>
inline uint64_t chrono_now()
{
    return uint64_t(Clock::now().time_since_epoch().count());
}

template <typename Clock>
inline double chrono_scale()
{
    using period = typename Clock::period;
    return double(period::num) / double(period::den);
}

inline uint64_t now_system_clock() { return chrono_now<std::chrono::system_clock>(); }
inline uint64_t now_steady_clock() { return chrono_now<std::chrono::steady_clock>(); }
inline uint64_t now_high_res_clock() { return chrono_now<std::chrono::high_resolution_clock>(); }

// ── C standard ───────────────────────────────────────────────────────────────

inline uint64_t now_c_clock() { return uint64_t(std::clock()); }

// ── Windows ──────────────────────────────────────────────────────────────────

#ifdef _WIN32
inline uint64_t now_qpc()
{
    LARGE_INTEGER v;
    QueryPerformanceCounter(&v);
    return uint64_t(v.QuadPart);
}

inline double qpc_scale()
{
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    return 1.0 / double(f.QuadPart);
}

inline uint64_t now_filetime()
{
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    return (uint64_t(ft.dwHighDateTime) << 32) | uint64_t(ft.dwLowDateTime);
}

inline uint64_t now_filetime_precise()
{
    FILETIME ft;
    GetSystemTimePreciseAsFileTime(&ft);
    return (uint64_t(ft.dwHighDateTime) << 32) | uint64_t(ft.dwLowDateTime);
}

inline uint64_t now_tick_count() { return uint64_t(GetTickCount64()); }
#endif

// ── POSIX ────────────────────────────────────────────────────────────────────

#if defined(__linux__) || defined(__APPLE__)
template <clockid_t ID>
inline uint64_t now_clock_gettime()
{
    timespec ts;
    clock_gettime(ID, &ts);
    return uint64_t(ts.tv_sec) * 1'000'000'000ull + uint64_t(ts.tv_nsec);
}

inline uint64_t now_gettimeofday()
{
    timeval tv;
    gettimeofday(&tv, nullptr);
    return uint64_t(tv.tv_sec) * 1'000'000ull + uint64_t(tv.tv_usec);
}
#endif

// ── macOS ────────────────────────────────────────────────────────────────────

#ifdef __APPLE__
inline uint64_t now_mach_absolute_time() { return uint64_t(mach_absolute_time()); }

inline double mach_scale()
{
    mach_timebase_info_data_t info;
    mach_timebase_info(&info);
    return double(info.numer) / double(info.denom) * 1e-9;
}
#endif

// ── Cycles ───────────────────────────────────────────────────────────────────

inline uint64_t now_cycles() { return cycles_now(); }

#ifdef BENCH_TIME_HAS_RDTSCP
inline uint64_t now_rdtscp() { return rdtscp_now(); }
#endif

// ── Registry ─────────────────────────────────────────────────────────────────

inline std::vector<TimeMethod>& methods_storage()
{
    static std::vector<TimeMethod> m;
    return m;
}

} // namespace bench_time

inline std::span<TimeMethod const> time_methods()
{
    return bench_time::methods_storage();
}

inline void init_time_methods()
{
    using namespace bench_time;
    auto& m = methods_storage();
    m.clear();

    m.push_back({"chrono::system_clock", "#42a5f5", &now_system_clock, chrono_scale<std::chrono::system_clock>()});
    m.push_back({"chrono::steady_clock", "#1e88e5", &now_steady_clock, chrono_scale<std::chrono::steady_clock>()});
    m.push_back({"chrono::high_resolution_clock", "#0d47a1", &now_high_res_clock,
                 chrono_scale<std::chrono::high_resolution_clock>()});

    m.push_back({"c::clock", "#9e9e9e", &now_c_clock, 1.0 / double(CLOCKS_PER_SEC)});

#ifdef _WIN32
    m.push_back({"win::QueryPerformanceCounter", "#66bb6a", &now_qpc, qpc_scale()});
    m.push_back({"win::GetSystemTimeAsFileTime", "#ffa726", &now_filetime, 100e-9});
    m.push_back({"win::GetSystemTimePreciseAsFileTime", "#fb8c00", &now_filetime_precise, 100e-9});
    m.push_back({"win::GetTickCount64", "#ef5350", &now_tick_count, 1e-3});
#endif

#if defined(__linux__) || defined(__APPLE__)
    m.push_back({"posix::CLOCK_REALTIME", "#ffa726", &now_clock_gettime<CLOCK_REALTIME>, 1e-9});
    m.push_back({"posix::CLOCK_MONOTONIC", "#66bb6a", &now_clock_gettime<CLOCK_MONOTONIC>, 1e-9});
#ifdef __linux__
    m.push_back({"posix::CLOCK_MONOTONIC_RAW", "#43a047", &now_clock_gettime<CLOCK_MONOTONIC_RAW>, 1e-9});
#endif
    m.push_back(
        {"posix::CLOCK_PROCESS_CPUTIME_ID", "#ab47bc", &now_clock_gettime<CLOCK_PROCESS_CPUTIME_ID>, 1e-9});
    m.push_back({"posix::CLOCK_THREAD_CPUTIME_ID", "#7e57c2", &now_clock_gettime<CLOCK_THREAD_CPUTIME_ID>, 1e-9});
    m.push_back({"posix::gettimeofday", "#ef5350", &now_gettimeofday, 1e-6});
#endif

#ifdef __APPLE__
    m.push_back({"mach_absolute_time", "#26a69a", &now_mach_absolute_time, mach_scale()});
#endif

    auto const cps = cycles_per_second();
    if (cps > 0)
    {
#if defined(_M_X64) || defined(__x86_64__) || defined(_M_IX86) || defined(__i386__)
        m.push_back({"hw::rdtsc", "#ec407a", &now_cycles, 1.0 / cps});
#elif defined(_M_ARM64) || defined(__aarch64__)
        m.push_back({"hw::cntvct_el0", "#ec407a", &now_cycles, 1.0 / cps});
#endif
#ifdef BENCH_TIME_HAS_RDTSCP
        m.push_back({"hw::rdtscp", "#f06292", &now_rdtscp, 1.0 / cps});
#endif
    }
}
