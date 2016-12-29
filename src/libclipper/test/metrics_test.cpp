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

TEST(MetricsTests, HistogramPercentile) {
  int64_t arr1[] = {15, 20, 35, 40, 50};
  double p1 = 0.4;
  double percentile1 = Histogram::percentile(std::vector<int64_t>(arr1, arr1 + 5), p1);
  ASSERT_EQ(static_cast<int>(percentile1), 26);

  std::vector<int64_t> vec2;
  vec2.push_back(15);
  double p2 = 0.4;
  double percentile2 = Histogram::percentile(vec2, p2);
  ASSERT_EQ(static_cast<int>(percentile2), 15);

  double p3 = 0.0;
  double percentile3 = Histogram::percentile(vec2, p3);
  ASSERT_EQ(static_cast<int>(percentile3), 15);

  double p4 = 1.0;
  double percentile4 = Histogram::percentile(vec2, p4);
  ASSERT_EQ(static_cast<int>(percentile4), 15);

  std::vector<int64_t> vec3;
  bool caught_exception = false;
  try {
    Histogram::percentile(vec3, p2);
  } catch(std::length_error) {
    caught_exception = true;
  }
  // If our attempt to compute a percentile with no snapshot data
  // does not throw an exception, then the test fails
  if(!caught_exception) {
    FAIL();
  }
}

} // namespace