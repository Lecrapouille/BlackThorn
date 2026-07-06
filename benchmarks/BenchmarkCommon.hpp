/**
 * @file BenchmarkCommon.hpp
 * @brief Shared timing helpers for benchmark executables.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#pragma once

#include <chrono>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>

namespace benchmark {

inline std::filesystem::path benchmarksRoot()
{
    namespace fs = std::filesystem;
    fs::path probe = fs::current_path();
    for (int i = 0; i < 8; ++i)
    {
        fs::path candidate = probe / "benchmarks";
        if (fs::exists(candidate / "yaml") || fs::exists(candidate / "xml"))
        {
            return candidate;
        }
        if (!probe.has_parent_path())
        {
            break;
        }
        probe = probe.parent_path();
    }
    return fs::current_path() / "benchmarks";
}

class Timer
{
public:

    void start()
    {
        m_start = Clock::now();
    }

    double elapsedMs() const
    {
        return std::chrono::duration<double, std::milli>(Clock::now() - m_start)
            .count();
    }

private:

    using Clock = std::chrono::steady_clock;
    Clock::time_point m_start = Clock::now();
};

inline void printResult(std::string const& p_name,
                        std::size_t p_iterations,
                        double p_total_ms)
{
    double const per_op_us =
        p_iterations > 0 ? (p_total_ms * 1000.0) / static_cast<double>(p_iterations)
                         : 0.0;

    std::cout << std::left << std::setw(46) << p_name << "  "
              << std::setw(10) << p_iterations << std::setw(12) << std::fixed
              << std::setprecision(3) << p_total_ms << " ms" << "  ("
              << std::setprecision(2) << per_op_us << " us/op)" << '\n';
}

inline double runTimed(std::size_t p_iterations,
                       std::function<void()> const& p_fn)
{
    Timer timer;
    timer.start();
    for (std::size_t i = 0; i < p_iterations; ++i)
    {
        p_fn();
    }
    return timer.elapsedMs();
}

} // namespace benchmark
