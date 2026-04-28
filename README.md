# Measuring "Measuring Time"

A benchmark that characterizes how the various clocks available to a C++ program behave on different systems (Windows, Linux, macOS-arm64).

It is *not* a "kernels × parameters sweep" benchmark. Instead we have:

- A registry of **time methods** — each is a function returning a `uint64_t` plus a scale factor that converts it to seconds.
- A growing set of independent **tests** — each defines its own measurement procedure, CSV schema, and chart.

## Methods

Cross-platform (always registered):
- `chrono::system_clock`, `chrono::steady_clock`, `chrono::high_resolution_clock`
- `c::clock` (from `<ctime>`)

Windows:
- `win::QueryPerformanceCounter`
- `win::GetSystemTimeAsFileTime`
- `win::GetSystemTimePreciseAsFileTime`
- `win::GetTickCount64`

POSIX (Linux + macOS):
- `posix::CLOCK_REALTIME`, `posix::CLOCK_MONOTONIC`
- `posix::CLOCK_PROCESS_CPUTIME_ID`, `posix::CLOCK_THREAD_CPUTIME_ID`
- `posix::gettimeofday`

Linux only: `posix::CLOCK_MONOTONIC_RAW`

macOS only: `mach_absolute_time`

Hardware cycle counters (architecture-dependent):
- x86 / x64: `hw::rdtsc` — frequency calibrated against `steady_clock` once at startup
- arm64: `hw::cntvct_el0` — frequency from `cntfrq_el0`

## Tests

- **A. Empirical granularity** — for each method, repeatedly read the clock until the value changes; record the delta. Up to 10 000 samples per method, capped at 1 s wall-clock budget. The distribution per method is plotted as a ridge histogram (`chart_granularity.svg`).
- **B. Granularity as seen by rdtsc** — same loop as A, but the recorded latency is `(cycles_after - cycles_before) / cycles_per_second` instead of the clock's own delta. Reveals the "true" wait time independent of the clock's own resolution. Output: `chart_granularity_rdtsc.svg`. Skipped on architectures without a hardware cycle counter.
- **C. Calls per change** — the inverse: count how many `now()` calls fit between successive value changes. Same 10 000 / 1 s budget. Plotted as a ridge histogram on a log-count axis. Output: `chart_calls_per_change.svg`.

Before any measurement, the binary runs ~300 ms of single-threaded `sin` work to nudge the CPU out of low-power states and prints a "warmup witness" value (so the work can't be elided).

## Requirements

- [CMake](https://cmake.org/) ≥ 3.25
- A C++23 compiler (GCC 14+, Clang 17+, MSVC 19.38+ / VS 2022 17.8+)
- Python 3.11+
- [uv](https://github.com/astral-sh/uv) *(for the shortcut scripts)*

## Quickstart

```bash
python build.py            # configure + build (release)
python run.py              # build + run + render charts
python run.py --test granularity   # run a specific test only
python run-debug.py        # build + run debug (no charts)
```

Outputs `result_*.csv` and `chart_*.svg` next to the repo root.

## Manual steps

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

./build/bin/bench-time                       # Linux / macOS
build\bin\Release\bench-time.exe             # Windows

uv run create-charts.py
```
