#include <cmath>
#include <limits>
#include <string>

#include <gtest/gtest.h>

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
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
  ratio_counter.increment(0, 1);
  ASSERT_EQ(ratio_counter.get_ratio(), 0);
  ratio_counter.increment(1, 2);
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
  ASSERT_LE(meter.get_one_minute_rate_seconds() - .2, .01);
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
  double percentile1 =
      Histogram::percentile(std::vector<int64_t>(arr1, arr1 + 5), p1);
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
  for (auto elem : arr2) {
    vec4.push_back(elem);
    vec5.push_back(elem);
  }
  std::sort(vec5.begin(), vec5.end());
  // The 60th percentile of snapshots containing the same data
  // in different orders should be the same, as the percentile
  // function should correctly sort the data before calculation
  ASSERT_EQ(Histogram::percentile(vec4, p5), Histogram::percentile(vec5, p5));
}

TEST(MetricsTests, HistogramStatsAreCorrectWithNumericallySmallElements) {
  int64_t arr[] = {16, 53, 104, 113, 185, 202};
  size_t sample_size = 6;
  Histogram histogram("Test Histogram", "milliseconds", sample_size);
  for (int64_t elem : arr) {
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

TEST(MetricsTests, HistogramMinMaxMeanAreCorrectWithNumericallyLargeElements) {
  size_t sample_size = 6;
  Histogram histogram("Test Histogram", "milliseconds", sample_size);
  long max_long = std::numeric_limits<long>::max();
  int64_t sum = 0;
  for (size_t i = 0; i < sample_size; ++i) {
    histogram.insert(max_long - i);
    sum += max_long - i;
  }
  long double expected_mean =
      static_cast<long double>(sum) / static_cast<long double>(sample_size);
  HistogramStats stats = histogram.compute_stats();
  ASSERT_DOUBLE_EQ(stats.mean_, expected_mean);
  ASSERT_DOUBLE_EQ(stats.max_, max_long);
  ASSERT_DOUBLE_EQ(stats.min_, max_long - sample_size + 1);
}

TEST(MetricsTests, MetricsRegistryReportingFormatCorrectness) {
  MetricsRegistry &registry = MetricsRegistry::get_metrics();
  const std::string hist1_name("Hist 1");
  const std::string hist2_name("Hist 2");
  const std::string counter1_name("Counter");
  const std::string ratio_counter1_name("Ratio Counter");

  std::shared_ptr<Histogram> hist1 =
      registry.create_histogram(hist1_name, "milliseconds", 2056);
  hist1->insert(100);
  hist1->insert(200);
  hist1->insert(300);

  std::shared_ptr<Histogram> hist2 =
      registry.create_histogram(hist2_name, "milliseconds", 2056);
  hist2->insert(30);
  hist2->insert(60);
  hist2->insert(115);

  std::shared_ptr<Counter> counter1 = registry.create_counter(counter1_name);
  counter1->increment(9);

  std::shared_ptr<RatioCounter> ratio_counter1 =
      registry.create_ratio_counter(ratio_counter1_name);
  ratio_counter1->increment(8, 40);

  // Get a metrics data report without clearing any metrics
  std::string report = registry.report_metrics();

  std::istringstream report_stream(report);
  boost::property_tree::ptree report_tree;
  boost::property_tree::read_json(report_stream, report_tree);

  boost::property_tree::ptree hists_tree =
      report_tree.get_child(get_metrics_category_name(MetricType::Histogram));
  ASSERT_EQ((int)hists_tree.size(), 2);

  boost::property_tree::ptree hist1_tree =
      hists_tree.front().second.get_child(hist1_name);
  HistogramStats hist1_stats = hist1->compute_stats();
  ASSERT_EQ(hist1_tree.get<size_t>("size"), hist1_stats.data_size_);
  ASSERT_EQ(hist1_tree.get<int64_t>("min"), hist1_stats.min_);
  ASSERT_EQ(hist1_tree.get<int64_t>("max"), hist1_stats.max_);
  ASSERT_DOUBLE_EQ(hist1_tree.get<long double>("mean"), hist1_stats.mean_);
  ASSERT_EQ(hist1_tree.get<long double>("std_dev"), hist1_stats.std_dev_);
  ASSERT_EQ(hist1_tree.get<long double>("p50"), hist1_stats.p50_);
  ASSERT_EQ(hist1_tree.get<long double>("p95"), hist1_stats.p95_);
  ASSERT_EQ(hist1_tree.get<long double>("p99"), hist1_stats.p99_);

  boost::property_tree::ptree hist2_tree =
      hists_tree.back().second.get_child(hist2_name);
  HistogramStats hist2_stats = hist2->compute_stats();
  ASSERT_EQ(hist2_tree.get<size_t>("size"), hist2_stats.data_size_);
  ASSERT_EQ(hist2_tree.get<int64_t>("min"), hist2_stats.min_);
  ASSERT_EQ(hist2_tree.get<int64_t>("max"), hist2_stats.max_);
  ASSERT_DOUBLE_EQ(hist2_tree.get<long double>("mean"), hist2_stats.mean_);
  ASSERT_DOUBLE_EQ(hist2_tree.get<long double>("std_dev"),
                   hist2_stats.std_dev_);
  ASSERT_DOUBLE_EQ(hist2_tree.get<long double>("p50"), hist2_stats.p50_);
  ASSERT_DOUBLE_EQ(hist2_tree.get<long double>("p95"), hist2_stats.p95_);
  ASSERT_DOUBLE_EQ(hist2_tree.get<long double>("p99"), hist2_stats.p99_);

  boost::property_tree::ptree counters_tree =
      report_tree.get_child(get_metrics_category_name(MetricType::Counter));
  ASSERT_EQ((int)counters_tree.size(), 1);
  boost::property_tree::ptree counter1_tree =
      counters_tree.front().second.get_child(counter1_name);
  ASSERT_EQ(counter1_tree.get<int>("count"), counter1->value());

  boost::property_tree::ptree ratio_counters_tree = report_tree.get_child(
      get_metrics_category_name(MetricType::RatioCounter));
  ASSERT_EQ((int)ratio_counters_tree.size(), 1);
  boost::property_tree::ptree ratio_counter1_tree =
      ratio_counters_tree.front().second.get_child(ratio_counter1_name);
  ASSERT_EQ(ratio_counter1_tree.get<double>("ratio"),
            ratio_counter1->get_ratio());
}

TEST(MetricsTests, MetricsRegistryReportingWithClearEnabledClearsCorrectly) {
  MetricsRegistry &registry = MetricsRegistry::get_metrics();

  std::shared_ptr<Histogram> hist =
      registry.create_histogram("Test Hist", "milliseconds", 10);
  hist->insert(8);
  hist->insert(10);
  HistogramStats stats = hist->compute_stats();
  ASSERT_EQ((int)stats.data_size_, 2);
  ASSERT_EQ(stats.min_, 8);
  ASSERT_EQ(stats.max_, 10);

  std::shared_ptr<Counter> counter = registry.create_counter("Counter", 8);
  counter->increment(6);
  ASSERT_EQ(counter->value(), 14);

  // Report metrics data and clear all metrics
  registry.report_metrics(true);

  HistogramStats new_stats = hist->compute_stats();
  ASSERT_EQ((int)new_stats.data_size_, 0);
  ASSERT_EQ(new_stats.min_, 0);
  ASSERT_EQ(new_stats.max_, 0);

  ASSERT_EQ(counter->value(), 0);
}

}  // namespace
