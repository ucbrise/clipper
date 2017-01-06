#include <clipper/metrics.hpp>
#include <string>
#include <iostream>
#include <zconf.h>
#include <cassert>
#include <cmath>

using namespace clipper::metrics;

void histogram() {
  MetricsRegistry &registry = MetricsRegistry::get_metrics();
  std::shared_ptr<Histogram> histogram = registry.create_histogram(std::string("Test Histogram"), "milliseconds", 6);
  int64_t arr[] = {15, 20, 35, 60, 40, 50};
  for(auto entry : arr) {
    histogram->insert(entry);
  }
  usleep(15000000);
  histogram->insert(100);
  histogram->insert(100);
  usleep(17000000);
}

void counters() {
  MetricsRegistry &registry = MetricsRegistry::get_metrics();
  std::shared_ptr<Counter> counter1 = registry.create_counter("Test Counter", 5);
  counter1->increment(4);
  std::shared_ptr<RatioCounter> ratio_counter = registry.create_ratio_counter("TRC");
  ratio_counter->increment(2,9);
  std::shared_ptr<Counter> counter2 = registry.create_counter("Test Counter2");
  std::shared_ptr<RatioCounter> ratio_counter2 = registry.create_ratio_counter("TRC2");

  usleep(15000000);

  ratio_counter2->increment(0,1);

  usleep(17000000);
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

void compare_percentile_functions() {
  for(int j = 0; j < 100; j++) {
    std::vector<long> long_vec;
    std::vector<int64_t> int_vec;
    for (int i = 0; i < 100; i++) {
      int64_t elem = rand() % 80;
      long_vec.push_back(elem);
      int_vec.push_back(elem);
    }

    double percentile = static_cast<double>(rand()) / static_cast<double>(RAND_MAX);

    double p1 = Histogram::percentile(int_vec, percentile);
    double p2 = compute_percentile(long_vec, percentile);

    if (p1 != p2) {
      std::cout << "Percentile functions differed!" << std::endl;
    }
  }
  std::cout << "Done" << std::endl;
}

int main() {
  //histogram();
  //counters();
  compare_percentile_functions();
}