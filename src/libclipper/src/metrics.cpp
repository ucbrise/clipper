
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <vector>

#include <clipper/metrics.hpp>

namespace clipper {
namespace metrics {

long get_duration_micros(
    std::chrono::time_point<std::chrono::high_resolution_clock> end,
    std::chrono::time_point<std::chrono::high_resolution_clock> start) {
  return std::chrono::duration_cast<std::chrono::microseconds>(end - start)
      .count();
}

double compute_mean(const std::vector<long>& measurements) {
  double sum = 0.0;
  for (auto m : measurements) {
    sum += m;
  }
  return sum / (double)measurements.size();
}

double compute_percentile(std::vector<long> measurements, double percentile) {
  // sort in ascending order
  std::sort(measurements.begin(), measurements.end());
  double sample_size = measurements.size();
  assert(percentile >= 0.0 && percentile <= 1.0);
  double x;
  if (percentile <= (1.0 / (sample_size + 1.0))) {
    x = 1.0;
  } else if (percentile > (1.0 / (sample_size + 1.0)) &&
             percentile < (sample_size / (sample_size + 1.0))) {
    x = percentile * (sample_size + 1.0);
  } else {
    x = sample_size;
  }
  int index = std::floor(x) - 1;
  double value = measurements[index];
  double remainder = std::fmod(x, 1.0);
  if (remainder != 0.0) {
    value += remainder * (measurements[index + 1] - measurements[index]);
  }
  return value;
}

}  // namespace metrics
}  // namespace clipper
