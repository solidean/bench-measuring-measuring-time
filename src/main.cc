#include "methods.hh"
#include "test_granularity.hh"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <print>
#include <string>
#include <string_view>

int main(int argc, char** argv)
{
    init_time_methods();

    std::string requested = "all";
    for (int i = 1; i < argc; ++i)
    {
        std::string_view a(argv[i]);
        if (a == "--test" && i + 1 < argc)
            requested = argv[++i];
    }

    auto const start = std::chrono::steady_clock::now();
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

    auto const elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    std::println("total time: {:.2f} sec", elapsed);
}
