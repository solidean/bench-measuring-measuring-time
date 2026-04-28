#include "methods.hh"
#include "test_calls_per_change.hh"
#include "test_granularity.hh"
#include "test_granularity_rdtsc.hh"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <print>
#include <string>
#include <string_view>

// Single-threaded heavy work to nudge the CPU out of low-power states before
// any measurement. Returned value is printed so the loop can't be elided.
double warmup(double duration_seconds)
{
    using clk = std::chrono::steady_clock;
    auto const start = clk::now();
    double v = 0.0;
    long long i = 0;
    while (true)
    {
        for (int k = 0; k < 1000; ++k)
        {
            v = std::sin(v + double(i));
            ++i;
        }
        auto const elapsed = std::chrono::duration<double>(clk::now() - start).count();
        if (elapsed >= duration_seconds)
            break;
    }
    return v;
}

int main(int argc, char** argv)
{
    std::string requested = "all";
    for (int i = 1; i < argc; ++i)
    {
        std::string_view a(argv[i]);
        if (a == "--test" && i + 1 < argc)
            requested = argv[++i];
    }

    auto const start = std::chrono::steady_clock::now();

    auto const witness = warmup(0.3);
    std::println("warmup witness: {}", witness);
    std::fflush(stdout);

    init_time_methods();

    std::println("Registered {} time methods:", time_methods().size());
    for (auto const& m : time_methods())
        std::println("  {:40s} scale={:.3e} s/tick", m.name, m.scale_to_seconds);
    std::fflush(stdout);

    if (requested == "all" || requested == "granularity")
    {
        std::ofstream csv("result_granularity.csv");
        run_test_granularity(csv);
        csv.close();
        std::println("wrote {}", std::filesystem::absolute("result_granularity.csv").string());
    }

    if (requested == "all" || requested == "granularity_rdtsc")
    {
        std::ofstream csv("result_granularity_rdtsc.csv");
        run_test_granularity_rdtsc(csv);
        csv.close();
        std::println("wrote {}", std::filesystem::absolute("result_granularity_rdtsc.csv").string());
    }

    if (requested == "all" || requested == "calls_per_change")
    {
        std::ofstream csv("result_calls_per_change.csv");
        run_test_calls_per_change(csv);
        csv.close();
        std::println("wrote {}", std::filesystem::absolute("result_calls_per_change.csv").string());
    }

    auto const elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    std::println("total time: {:.2f} sec", elapsed);
}
