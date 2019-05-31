#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <numeric>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <math.h>

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <clipper/logging.hpp>
#include <clipper/metrics.hpp>

namespace clipper {

namespace metrics {

constexpr long MICROS_PER_SECOND = 1000000;
constexpr double SECONDS_PER_MINUTE = 60;
constexpr double ONE_MINUTE = 1;
constexpr double FIVE_MINUTES = 5;
constexpr double FIFTEEN_MINUTES = 15;

/**
 * This comparison function is used to sort metrics based on their
 * type (Counter, Meter, etc) for structured logging
 */
bool compare_metrics(std::shared_ptr<Metric> first,
                     std::shared_ptr<Metric> second) {
  MetricType first_type = first->type();
  MetricType second_type = second->type();
  int diff = static_cast<int>(first_type) - static_cast<int>(second_type);
  return (diff < 0);
}

const std::string get_metrics_category_name(MetricType type) {
  switch (type) {
    case MetricType::Counter: return "counters";
    case MetricType::RatioCounter: return "ratio_counters";
    case MetricType::Meter: return "meters";
    case MetricType::Histogram: return "histograms";
    default:
      throw std::invalid_argument(std::to_string(static_cast<int>(type)) +
                                  " is unknown MetricType");
  }
}

MetricsRegistry::MetricsRegistry()
    : metrics_(std::make_shared<std::vector<std::shared_ptr<Metric>>>()),
      metrics_lock_(std::make_shared<std::mutex>()) {}

MetricsRegistry &MetricsRegistry::get_metrics() {
  // References a global singleton MetricsRegistry object.
  // This object is created if it does not already exist,
  // and it is automatically memory managed
  static MetricsRegistry instance;
  return instance;
}

const std::string MetricsRegistry::report_metrics(const bool clear) {
  std::lock_guard<std::mutex> guard(*metrics_lock_);
  if (metrics_->size() > 0) {
    // Sorts the metrics by MetricType in order to output them by category
    std::sort(metrics_->begin(), metrics_->end(), compare_metrics);
    boost::property_tree::ptree main_tree;
    boost::property_tree::ptree curr_category_tree;
    MetricType prev_type = metrics_->front()->type();
    for (int i = 0; i < (int)metrics_->size(); i++) {
      std::shared_ptr<Metric> metric = (*metrics_)[i];
      MetricType curr_type = metric->type();
      if (i > 0 && curr_type != prev_type) {
        main_tree.put_child(get_metrics_category_name(prev_type),
                            curr_category_tree);
        curr_category_tree.clear();
      }
      boost::property_tree::ptree named_tree;
      boost::property_tree::ptree report_tree = metric->report_tree();
      named_tree.put_child(metric->name(), report_tree);
      curr_category_tree.push_back(std::make_pair("", named_tree));
      if (clear) {
        metric->clear();
      }
      prev_type = curr_type;
    }
    // Tail case
    main_tree.put_child(get_metrics_category_name(prev_type),
                        curr_category_tree);
    std::ostringstream ss;
    boost::property_tree::write_json(ss, main_tree);
    return ss.str();
  } else {
    // If no metrics registered, return an empty string
    return std::string("");
  }
}

std::shared_ptr<Counter> MetricsRegistry::create_counter(
    const std::string name, const int initial_count) {
  std::shared_ptr<Counter> counter =
      std::make_shared<Counter>(name, initial_count);
  metrics_->push_back(counter);
  return counter;
}

std::shared_ptr<Counter> MetricsRegistry::create_counter(
    const std::string name) {
  return create_counter(name, 0);
}

std::shared_ptr<RatioCounter> MetricsRegistry::create_ratio_counter(
    const std::string name, const uint32_t num, const uint32_t denom) {
  std::shared_ptr<RatioCounter> ratio_counter =
      std::make_shared<RatioCounter>(name, num, denom);
  metrics_->push_back(ratio_counter);
  return ratio_counter;
}

std::shared_ptr<RatioCounter> MetricsRegistry::create_ratio_counter(
    const std::string name) {
  return create_ratio_counter(name, 0, 0);
}

std::shared_ptr<Meter> MetricsRegistry::create_meter(const std::string name) {
  std::shared_ptr<RealTimeClock> clock = std::make_shared<RealTimeClock>();
  std::shared_ptr<Meter> meter = std::make_shared<Meter>(
      name, std::dynamic_pointer_cast<MeterClock>(clock));
  metrics_->push_back(meter);
  return meter;
}

std::shared_ptr<Histogram> MetricsRegistry::create_histogram(
    const std::string name, const std::string unit, const size_t sample_size) {
  std::shared_ptr<Histogram> histogram =
      std::make_shared<Histogram>(name, unit, sample_size);
  metrics_->push_back(histogram);
  return histogram;
}

void MetricsRegistry::delete_metric(std::shared_ptr<Metric> target) {
  std::lock_guard<std::mutex> guard(*metrics_lock_);
  for (auto it = metrics_->begin(); it != metrics_->end();) {
    if (*it == target) {
      it = metrics_->erase(it);
    } else {
      it++;
    }
  }
}

Counter::Counter(const std::string name) : Counter(name, 0) {}

Counter::Counter(const std::string name, int initial_count)
    : name_(name), count_(initial_count) {}

void Counter::increment(const int value) {
  count_.fetch_add(value, std::memory_order_relaxed);
}

void Counter::decrement(const int value) {
  count_.fetch_sub(value, std::memory_order_relaxed);
}

int Counter::value() const { return count_.load(std::memory_order_seq_cst); }

MetricType Counter::type() const { return MetricType::Counter; }

const std::string Counter::name() const { return name_; }

const boost::property_tree::ptree Counter::report_tree() {
  int value = count_.load(std::memory_order_seq_cst);
  boost::property_tree::ptree report_tree;
  report_tree.put("count", value);
  return report_tree;
}

const std::string Counter::report_str() {
  std::ostringstream ss;
  boost::property_tree::ptree report = report_tree();
  boost::property_tree::write_json(ss, report);
  return ss.str();
}

void Counter::clear() { count_.store(0, std::memory_order_seq_cst); }

RatioCounter::RatioCounter(const std::string name) : RatioCounter(name, 0, 0) {}

RatioCounter::RatioCounter(const std::string name, uint32_t num, uint32_t denom)
    : name_(name), numerator_(num), denominator_(denom) {}

void RatioCounter::increment(const uint32_t num_incr,
                             const uint32_t denom_incr) {
  std::lock_guard<std::mutex> guard(ratio_lock_);
  numerator_.fetch_add(num_incr, std::memory_order_relaxed);
  denominator_.fetch_add(denom_incr, std::memory_order_relaxed);
}

double RatioCounter::get_ratio() {
  std::lock_guard<std::mutex> guard(ratio_lock_);
  uint32_t num_value = numerator_.load(std::memory_order_seq_cst);
  uint32_t denom_value = denominator_.load(std::memory_order_seq_cst);
  if (denom_value == 0) {
    log_error_formatted(LOGGING_TAG_METRICS, "Ratio {} has denominator zero!",
                        name_);
    return std::nan("");
  }
  double ratio =
      static_cast<double>(num_value) / static_cast<double>(denom_value);
  return ratio;
}

MetricType RatioCounter::type() const { return MetricType::RatioCounter; }

const std::string RatioCounter::name() const { return name_; }

const boost::property_tree::ptree RatioCounter::report_tree() {
  double ratio = get_ratio();
  boost::property_tree::ptree report_tree;
  report_tree.put("ratio", ratio);
  return report_tree;
}

const std::string RatioCounter::report_str() {
  std::ostringstream ss;
  boost::property_tree::ptree report = report_tree();
  boost::property_tree::write_json(ss, report);
  return ss.str();
}

void RatioCounter::clear() {
  std::lock_guard<std::mutex> guard(ratio_lock_);
  numerator_.store(0, std::memory_order_seq_cst);
  denominator_.store(0, std::memory_order_seq_cst);
}

long RealTimeClock::get_time_micros() const {
  long current_time_micros =
      std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count();
  return current_time_micros;
}

void PresetClock::set_time_micros(const long time_micros) {
  time_ = time_micros;
}

long PresetClock::get_time_micros() const { return time_; }

EWMA::EWMA(long tick_interval_seconds, LoadAverage load_average)
    : tick_interval_seconds_(tick_interval_seconds), uncounted_(0) {
  double alpha_exp = 0;
  double alpha_exp_1 =
      (static_cast<double>(-1 * tick_interval_seconds)) / SECONDS_PER_MINUTE;
  switch (load_average) {
    case LoadAverage::OneMinute:
      alpha_exp = exp(alpha_exp_1 / ONE_MINUTE);
      break;
    case LoadAverage::FiveMinute:
      alpha_exp = exp(alpha_exp_1 / FIVE_MINUTES);
      break;
    case LoadAverage::FifteenMinute:
      alpha_exp = exp(alpha_exp_1 / FIFTEEN_MINUTES);
      break;
  }
  alpha_ = 1 - alpha_exp;
}

void EWMA::tick() {
  std::lock_guard<std::mutex> guard(rate_lock_);
  double count = uncounted_.exchange(0, std::memory_order_relaxed);
  double current_rate = count / static_cast<double>(tick_interval_seconds_);
  if (rate_ == -1) {
    // current_rate is the first rate we've calculated,
    // so we set rate to current_rate.
    rate_ = current_rate;
  } else {
    // Update the rate in accordance with an exponentially decaying function
    // of current_rate
    rate_ += alpha_ * (current_rate - rate_);
  }
}

void EWMA::mark_uncounted(uint32_t num) {
  uncounted_.fetch_add(num, std::memory_order_relaxed);
}

void EWMA::reset() {
  std::lock_guard<std::mutex> guard(rate_lock_);
  rate_ = -1;
  uncounted_.store(0, std::memory_order_seq_cst);
}

double EWMA::get_rate_seconds() {
  std::lock_guard<std::mutex> guard(rate_lock_);
  double rate = rate_;
  return rate;
}

Meter::Meter(std::string name, std::shared_ptr<MeterClock> clock)
    : name_(name),
      clock_(clock),
      count_(0),
      start_time_micros_(clock->get_time_micros()),
      last_ewma_tick_micros_(start_time_micros_),
      m1_rate(ewma_tick_interval_seconds_, LoadAverage::OneMinute),
      m5_rate(ewma_tick_interval_seconds_, LoadAverage::FiveMinute),
      m15_rate(ewma_tick_interval_seconds_, LoadAverage::FifteenMinute) {}

void Meter::mark(uint32_t num) {
  tick_if_necessary();
  count_.fetch_add(num, std::memory_order_relaxed);
  m1_rate.mark_uncounted(num);
  m5_rate.mark_uncounted(num);
  m15_rate.mark_uncounted(num);
}

void Meter::tick_if_necessary() {
  long curr_micros = clock_->get_time_micros();
  auto tick_interval_seconds =
      std::chrono::seconds(ewma_tick_interval_seconds_);
  long tick_interval_micros =
      std::chrono::microseconds(tick_interval_seconds).count();
  long last_tick = last_ewma_tick_micros_.load(std::memory_order_seq_cst);
  long time_since_last_tick = curr_micros - last_tick;

  if (time_since_last_tick < tick_interval_micros) {
    // Not enough time has elapsed since the last tick, so we do no work
    return;
  }

  long new_last_tick =
      curr_micros - (time_since_last_tick % tick_interval_micros);
  bool last_tick_update_successful =
      last_ewma_tick_micros_.compare_exchange_strong(last_tick, new_last_tick,
                                                     std::memory_order_seq_cst);

  if (last_tick_update_successful) {
    double num_ticks = static_cast<double>(time_since_last_tick) /
                       static_cast<double>(tick_interval_micros);
    for (int i = 0; i < static_cast<int>(num_ticks); i++) {
      m1_rate.tick();
      m5_rate.tick();
      m15_rate.tick();
    }
  }
}

double Meter::get_rate_micros() {
  std::lock_guard<std::mutex> guard(start_time_lock_);
  uint32_t curr_count = count_.load(std::memory_order_seq_cst);
  long curr_time_micros = clock_->get_time_micros();
  double rate = static_cast<double>(curr_count) /
                static_cast<double>(curr_time_micros - start_time_micros_);
  return rate;
}

double Meter::get_rate_seconds() {
  return get_rate_micros() * MICROS_PER_SECOND;
}

double Meter::get_one_minute_rate_seconds() {
  tick_if_necessary();
  return m1_rate.get_rate_seconds();
}

double Meter::get_five_minute_rate_seconds() {
  tick_if_necessary();
  return m5_rate.get_rate_seconds();
}

double Meter::get_fifteen_minute_rate_seconds() {
  tick_if_necessary();
  return m15_rate.get_rate_seconds();
}

MetricType Meter::type() const { return MetricType::Meter; }

const std::string Meter::name() const { return name_; }

const boost::property_tree::ptree Meter::report_tree() {
  boost::property_tree::ptree report_tree;
  report_tree.put("unit", unit_);
  report_tree.put("rate", get_rate_seconds());
  report_tree.put("rate_1min", get_one_minute_rate_seconds());
  report_tree.put("rate_5min", get_five_minute_rate_seconds());
  report_tree.put("rate_15min", get_fifteen_minute_rate_seconds());
  return report_tree;
}

const std::string Meter::report_str() {
  std::ostringstream ss;
  boost::property_tree::ptree report = report_tree();
  boost::property_tree::write_json(ss, report);
  return ss.str();
}

void Meter::clear() {
  std::lock_guard<std::mutex> guard(start_time_lock_);
  start_time_micros_ = clock_->get_time_micros();
  count_.store(0, std::memory_order_seq_cst);
  m1_rate.reset();
  m5_rate.reset();
  m15_rate.reset();
}

ReservoirSampler::ReservoirSampler(size_t sample_size)
    : sample_size_(sample_size) {}

int64_t ReservoirSampler::sample(const int64_t value) {
  int64_t replaced_value = 0;
  if (n_ < sample_size_) {
    reservoir_.push_back(value);
  } else {
    if (reservoir_.size() != sample_size_) {
      throw std::length_error("Reservoir size exceeds sample size!");
    }
    size_t j = rand() % (n_ + 1);
    if (j < sample_size_) {
      replaced_value = reservoir_[j];
      reservoir_[j] = value;
    }
  }
  n_++;
  return replaced_value;
}

size_t ReservoirSampler::current_size() const { return reservoir_.size(); }

const std::vector<int64_t> ReservoirSampler::snapshot() const {
  return reservoir_;
}

void ReservoirSampler::clear() {
  reservoir_.clear();
  n_ = 0;
}

HistogramStats::HistogramStats(size_t data_size, int64_t min, int64_t max,
                               long double mean, long double std_dev,
                               long double p50, long double p95,
                               long double p99)
    : data_size_(data_size),
      min_(min),
      max_(max),
      mean_(mean),
      std_dev_(std_dev),
      p50_(p50),
      p95_(p95),
      p99_(p99) {}

Histogram::Histogram(const std::string name, const std::string unit,
                     const size_t sample_size)
    : name_(name), unit_(unit), sampler_(sample_size) {}

void Histogram::insert(const int64_t value) {
  std::lock_guard<std::mutex> guard(sampler_lock_);
  size_t old_reservoir_size = sampler_.current_size();
  int64_t replaced_value = sampler_.sample(value);
  size_t new_reservoir_size = sampler_.current_size();
  update_mean(old_reservoir_size, new_reservoir_size, value, replaced_value);
}

void Histogram::update_mean(const size_t old_reservoir_size,
                            const size_t new_reservoir_size,
                            const int64_t new_value, const int64_t old_value) {
  int64_t old_sum = static_cast<int64_t>(mean_ * old_reservoir_size);
  mean_ = static_cast<long double>(old_sum - old_value + new_value) /
          static_cast<long double>(new_reservoir_size);
}

long double Histogram::percentile(std::vector<int64_t> snapshot, double rank) {
  if (rank < 0 || rank > 1) {
    throw std::invalid_argument("Percentile rank must be in [0,1]!");
  }
  size_t sample_size = snapshot.size();
  if (sample_size <= 0) {
    throw std::length_error(
        "Percentile snapshot must have length greater than zero!");
  }
  std::sort(snapshot.begin(), snapshot.end());
  double x;
  double x_condition =
      (static_cast<double>(1) / static_cast<double>(sample_size + 1));
  if (rank <= x_condition) {
    x = 1;
  } else if (rank > x_condition &&
             rank < (static_cast<double>(sample_size) * x_condition)) {
    x = rank * static_cast<double>(sample_size + 1);
  } else {
    x = sample_size;
  }
  size_t index = std::floor(x) - 1;
  long double v = snapshot[index];
  double remainder = x - std::floor(x);
  if (remainder == 0) {
    return v;
  } else {
    return v + (remainder * (snapshot[index + 1] - snapshot[index]));
  }
}

long double Histogram::percentile(double rank) {
  std::lock_guard<std::mutex> guard(sampler_lock_);
  std::vector<int64_t> snapshot = sampler_.snapshot();
  if (snapshot.size() == 0) {
    return 0;
  }
  std::sort(snapshot.begin(), snapshot.end());

  return Histogram::percentile(snapshot, rank);
}

const HistogramStats Histogram::compute_stats() {
  std::lock_guard<std::mutex> guard(sampler_lock_);
  std::vector<int64_t> snapshot = sampler_.snapshot();
  size_t snapshot_size = snapshot.size();
  if (snapshot_size == 0) {
    HistogramStats stats;
    return stats;
  }
  std::sort(snapshot.begin(), snapshot.end());
  int64_t min = snapshot.front();
  int64_t max = snapshot.back();
  long double p50 = percentile(snapshot, .5);
  long double p95 = percentile(snapshot, .95);
  long double p99 = percentile(snapshot, .99);
  long double var = 0;
  if (snapshot_size > 1) {
    for (auto elem : snapshot) {
      long double incr = std::pow(elem - mean_, static_cast<long double>(2));
      var += incr;
    }
    var = var / static_cast<double>(snapshot_size);
  }
  long double std_dev = std::sqrt(var);
  return HistogramStats(snapshot_size, min, max, mean_, std_dev, p50, p95, p99);
}

MetricType Histogram::type() const { return MetricType::Histogram; }

const std::string Histogram::name() const { return name_; }

const boost::property_tree::ptree Histogram::report_tree() {
  HistogramStats stats = compute_stats();
  boost::property_tree::ptree report_tree;
  report_tree.put("unit", unit_);
  report_tree.put("size", stats.data_size_);
  report_tree.put("min", stats.min_);
  report_tree.put("max", stats.max_);
  report_tree.put("mean", stats.mean_);
  report_tree.put("std_dev", stats.std_dev_);
  report_tree.put("p50", stats.p50_);
  report_tree.put("p95", stats.p95_);
  report_tree.put("p99", stats.p99_);
  return report_tree;
}

const std::string Histogram::report_str() {
  std::ostringstream ss;
  boost::property_tree::ptree report = report_tree();
  boost::property_tree::write_json(ss, report);
  return ss.str();
}

void Histogram::clear() {
  std::lock_guard<std::mutex> guard(sampler_lock_);
  sampler_.clear();
}

}  // namespace metrics

}  // namespace clipper
