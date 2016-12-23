#pragma once

#ifndef CLIPPER_LIB_METRICS_HPP
#define CLIPPER_LIB_METRICS_HPP

#include <math.h>
#include <cassert>
#include <chrono>
#include <iostream>
#include <vector>

namespace clipper {
namespace metrics {

long get_duration_micros(
    std::chrono::time_point<std::chrono::high_resolution_clock> end,
    std::chrono::time_point<std::chrono::high_resolution_clock> start);

double compute_mean(const std::vector<long>& measurements);

double compute_percentile(std::vector<long> measurements, double percentile);

}  // namespace metrics
}  // namespace clipper

#endif  // CLIPPER_LIB_METRICS_HPP
