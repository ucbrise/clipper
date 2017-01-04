#include <clipper/metrics.hpp>
#include <string>
#include <iostream>
#include <zconf.h>

using namespace clipper::metrics;

void histogram() {
  MetricsRegistry &registry = MetricsRegistry::instance();
  std::shared_ptr<Histogram> histogram = registry.create_histogram(std::string("Test Histogram"), 6);
  int64_t arr[] = {15, 20, 35, 60, 40, 50};
  for(auto entry : arr) {
    histogram->insert(entry);
  }
  usleep(15000000);
  histogram->insert(100);
  histogram->insert(100);
  usleep(15000000);
}

void counters() {
  MetricsRegistry &registry = MetricsRegistry::instance();
  std::shared_ptr<Counter> counter1 = registry.create_counter(std::string("Test Counter"), 5);
  counter1->increment(4);
  std::shared_ptr<RatioCounter> ratio_counter = registry.create_ratio_counter(std::string("TRC"));
  ratio_counter->increment(2,9);
  std::shared_ptr<Counter> counter2 = registry.create_counter(std::string("Test Counter2"));
  std::shared_ptr<RatioCounter> ratio_counter2 = registry.create_ratio_counter(std::string("TRC2"));

  usleep(15000000);

  ratio_counter2->increment(0,1);

  usleep(15000000);
}

int main() {
  histogram();
  counters();
}