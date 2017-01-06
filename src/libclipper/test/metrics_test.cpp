#include <string>
#include <cmath>

#include <gtest/gtest.h>

#include <clipper/metrics.hpp>

using namespace clipper::metrics;

namespace {

TEST(MetricsTests, CounterCorrectness) {
  Counter counter("Test Counter");
  counter.decrement(183);
  ASSERT_EQ(counter.value(), -183);
  counter.increment(7);
  ASSERT_EQ(counter.value(), -176);
  counter.increment(200);
  ASSERT_EQ(counter.value(), 24);
  counter.increment(76);
  ASSERT_EQ(counter.value(), 100);
  counter.decrement(100);
  ASSERT_EQ(counter.value(), 0);
}

TEST(MetricsTests, RatioCounterCorrectness) {
  RatioCounter ratio_counter("Test Ratio Counter");
  // The ratio's denominator is initialized to zero, so we expect a NaN ratio
  ASSERT_TRUE(std::isnan(ratio_counter.get_ratio()));
  ratio_counter.increment(0,1);
  ASSERT_EQ(ratio_counter.get_ratio(), 0);
  ratio_counter.increment(1,2);
  ASSERT_LE(std::abs(ratio_counter.get_ratio() - .33), .01);
  ratio_counter.increment(5, 10);
  ASSERT_LE(std::abs(ratio_counter.get_ratio() - .461), .001);

  RatioCounter ratio_counter_2("Test Counter", 17, 1);
  ASSERT_EQ(ratio_counter_2.get_ratio(), 17);
}

TEST(MetricsTests, MeterCorrectness) {
  std::shared_ptr<PresetClock> clock = std::make_shared<PresetClock>();
  Meter meter("Test meter", std::dynamic_pointer_cast<MeterClock>(clock));
  meter.mark(100);
  clock->set_time_micros(100000000);
  ASSERT_DOUBLE_EQ(meter.get_rate_seconds(), 1);
  clock->set_time_micros(200000000);
  ASSERT_DOUBLE_EQ(meter.get_rate_seconds(), 0.5);
  meter.mark(200);
  ASSERT_DOUBLE_EQ(meter.get_rate_seconds(), 1.5);
}

TEST(MetricsTests, EWMACorrectness) {
  std::shared_ptr<PresetClock> clock = std::make_shared<PresetClock>();
  Meter meter("Test meter", std::dynamic_pointer_cast<MeterClock>(clock));
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

TEST(MetricsTests, HistogramPercentileFunctionCorrectness) {
  int64_t arr1[] = {15, 20, 35, 40, 50};
  double p1 = 0.4;
  double percentile1 = Histogram::percentile(std::vector<int64_t>(arr1, arr1 + 5), p1);
  // The 40th percentile of the set of the data enumerated in arr1 should be 26
  ASSERT_EQ(static_cast<int>(percentile1), 26);

  std::vector<int64_t> vec2;
  vec2.push_back(15);
  double p2 = 0.4;
  double percentile2 = Histogram::percentile(vec2, p2);
  // The 40th percentile of the set of data in vec2 should be 15
  // because there's only one element
  ASSERT_EQ(static_cast<int>(percentile2), 15);

  double p3 = 0.0;
  double percentile3 = Histogram::percentile(vec2, p3);
  // The 0th percentile of the set of data in vec2 should be 15
  // because there's only one element
  ASSERT_EQ(static_cast<int>(percentile3), 15);

  double p4 = 1.0;
  double percentile4 = Histogram::percentile(vec2, p4);
  // The 100th percentile of the set of data in vec2 should be 15
  // because there's only one element
  ASSERT_EQ(static_cast<int>(percentile4), 15);

  std::vector<int64_t> vec3;
  // We expect a length error when computing a percentile
  // without snapshot data
  ASSERT_THROW(Histogram::percentile(vec3, p2), std::length_error);

  int64_t arr2[] = {67, 31, 45, 40, 39};
  double p5 = .6;
  std::vector<int64_t> vec4;
  std::vector<int64_t> vec5;
  for(auto elem : arr2) {
    vec4.push_back(elem);
    vec5.push_back(elem);
  }
  std::sort(vec5.begin(), vec5.end());
  // The 60th percentile of snapshots containing the same data
  // in different orders should be the same, as the percentile
  // function should correctly sort the data before calculation
  ASSERT_EQ(Histogram::percentile(vec4, p5), Histogram::percentile(vec5, p5));
}

TEST(MetricsTests, HistogramStatsCorrectness) {
  int64_t arr[] = {16, 53, 104, 113, 185, 202};
  size_t sample_size = 6;
  Histogram histogram("Test Histogram", "milliseconds", sample_size);
  for(int64_t elem : arr) {
    histogram.insert(elem);
  }
  HistogramStats stats = histogram.compute_stats();
  ASSERT_DOUBLE_EQ(stats.min_, 16);
  ASSERT_DOUBLE_EQ(stats.max_, 202);
  ASSERT_LE(std::abs(stats.mean_ - 112.16), .01);
  ASSERT_LE(std::abs(stats.std_dev_ - 66.06), .01);
  ASSERT_LE(std::abs(stats.p50_ - 108.5), .01);
  ASSERT_EQ(stats.p95_, 202);
  ASSERT_EQ(stats.p99_, 202);
}

} // namespace