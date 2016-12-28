#include <string>

#include <gtest/gtest.h>

#include <clipper/metrics.hpp>

using namespace clipper;

namespace {

TEST(MetricsTests, EWMAConsistency) {
  std::shared_ptr<PresetClock> clock = std::make_shared<PresetClock>();
  Meter meter(std::string("Test meter"), std::dynamic_pointer_cast<MeterClock>(clock));
  meter.mark(1);
  clock->set_time_micros(5000000);
  ASSERT_LE(meter.get_one_minute_rate_seconds() - .2,  .01);
  meter.mark(2);
  clock->set_time_micros(30000000);
  ASSERT_LE(meter.get_one_minute_rate_seconds() - .15, .01);
  clock->set_time_micros(150000000);
  ASSERT_LE(meter.get_five_minute_rate_seconds() - .13, .01);
  ASSERT_LE(meter.get_fifteen_minute_rate_seconds() - .17, .01);
  clock->set_time_micros(960000000);
  ASSERT_LE(meter.get_five_minute_rate_seconds(), .01);
  ASSERT_LE(meter.get_fifteen_minute_rate_seconds() - .07, .01);
}

} // namespace