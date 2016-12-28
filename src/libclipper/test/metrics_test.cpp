#include <gtest/gtest.h>

#include <clipper/metrics.hpp>

using namespace clipper;

namespace {

  TEST(MetricsTests, NotSure) {
    MetricsRegistry &registry = MetricsRegistry::instance();
    std::shared_ptr<Counter> counter = registry.create_counter("Test Counter", 5);
    counter->increment(4);
    usleep(3000000);
  }

} // namespace