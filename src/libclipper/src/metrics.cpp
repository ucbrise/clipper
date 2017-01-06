#include <atomic>
#include <string>
#include <vector>
#include <mutex>
#include <iostream>
#include <numeric>
#include <thread>
#include <chrono>

#include <math.h>

#include <boost/thread.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <clipper/metrics.hpp>

namespace clipper {

namespace metrics {

constexpr long METRICS_REPORTING_FREQUENCY_MICROS = 15000000;
constexpr long MICROS_PER_SECOND = 1000000;
constexpr long CLOCKS_PER_MILLISECOND = CLOCKS_PER_SEC / MICROS_PER_SECOND;
constexpr double SECONDS_PER_MINUTE = 60;
constexpr double ONE_MINUTE = 1;
constexpr double FIVE_MINUTES = 5;
constexpr double FIFTEEN_MINUTES = 15;

/**
 * This comparison function is used to sort metrics based on their
 * type (Counter, Meter, etc) for structured logging
 */
bool compare_metrics(std::shared_ptr<Metric> first, std::shared_ptr<Metric> second) {
  MetricType first_type = first->type();
  MetricType second_type = second->type();
  int diff = static_cast<int>(first_type) - static_cast<int>(second_type);
  return (diff < 0);
}

/**
 * When periodically logging metrics, this message indicates
 * the beginning of a new metrics category
 */
void log_metrics_category(MetricType type) {
  switch (type) {
    case MetricType::Counter:std::cout << "Counters" << std::endl << "-------" << std::endl;
      break;
    case MetricType::RatioCounter:std::cout << "Ratio Counters" << std::endl << "-------" << std::endl;
      break;
    case MetricType::Meter:std::cout << "Meters" << std::endl << "-------" << std::endl;
      break;
    case MetricType::Histogram:std::cout << "Histograms" << std::endl << "-------" << std::endl;
      break;
  }
}

MetricsRegistry::MetricsRegistry()
    : metrics_(std::make_shared<std::vector<std::shared_ptr<Metric>>>()),
      metrics_lock_(std::make_shared<std::mutex>()),
      active_(true) {
  metrics_thread_ = std::thread([this]() {
    manage_metrics();
  });
}

MetricsRegistry::~MetricsRegistry() {
  // signal metrics thread to shutdown
  active_.store(false, std::memory_order_seq_cst);
  // wait for it to finish
  metrics_thread_.join();
}

MetricsRegistry &MetricsRegistry::get_metrics() {
  // References a global singleton MetricsRegistry object.
  // This object is created if it does not already exist,
  // and it is automatically memory managed
  static MetricsRegistry instance;
  return instance;
}

void MetricsRegistry::manage_metrics() {
  while (active_.load(std::memory_order_seq_cst)) {
    std::this_thread::sleep_for(std::chrono::microseconds(METRICS_REPORTING_FREQUENCY_MICROS));
    std::lock_guard<std::mutex> guard(*metrics_lock_);
    // Sorts the metrics by MetricType in order to log them by category
    std::sort((*metrics_).begin(), (*metrics_).end(), compare_metrics);
    MetricType prev_type;
    for (int i = 0; i < (int) (*metrics_).size(); i++) {
      std::shared_ptr<Metric> metric = (*metrics_)[i];
      MetricType curr_type = metric->type();
      if (i == 0 || prev_type != curr_type) {
        log_metrics_category(curr_type);
      }
      prev_type = curr_type;
      const std::string report = metric->report();
      std::cout << report;
      metric->clear();
      std::cout << std::endl;
    }
  }
}

std::shared_ptr<Counter> MetricsRegistry::create_counter(const std::string name, const int initial_count) {
  std::shared_ptr<Counter> counter = std::make_shared<Counter>(name, initial_count);
  metrics_->push_back(counter);
  return counter;
}

std::shared_ptr<Counter> MetricsRegistry::create_counter(const std::string name) {
  return create_counter(name, 0);
}

std::shared_ptr<RatioCounter> MetricsRegistry::create_ratio_counter(const std::string name,
                                                                    const uint32_t num,
                                                                    const uint32_t denom) {
  std::shared_ptr<RatioCounter> ratio_counter = std::make_shared<RatioCounter>(name, num, denom);
  metrics_->push_back(ratio_counter);
  return ratio_counter;
}

std::shared_ptr<RatioCounter> MetricsRegistry::create_ratio_counter(const std::string name) {
  return create_ratio_counter(name, 0, 0);
}

std::shared_ptr<Meter> MetricsRegistry::create_meter(const std::string name) {
  std::shared_ptr<RealTimeClock> clock = std::make_shared<RealTimeClock>();
  std::shared_ptr<Meter> meter = std::make_shared<Meter>(name, std::dynamic_pointer_cast<MeterClock>(clock));
  metrics_->push_back(meter);
  return meter;
}

std::shared_ptr<Histogram> MetricsRegistry::create_histogram(const std::string name,
                                                             const std::string unit,
                                                             const size_t sample_size) {
  std::shared_ptr<Histogram> histogram = std::make_shared<Histogram>(name, unit, sample_size);
  metrics_->push_back(histogram);
  return histogram;
}

Counter::Counter(const std::string name) : Counter(name, 0) {

}

Counter::Counter(const std::string name, int initial_count)
    : name_(name), count_(initial_count) {

}

void Counter::increment(const int value) {
  count_.fetch_add(value, std::memory_order_relaxed);
}

void Counter::decrement(const int value) {
  count_.fetch_sub(value, std::memory_order_relaxed);
}

int Counter::value() const {
  return count_.load(std::memory_order_seq_cst);
}

MetricType Counter::type() const {
  return MetricType::Counter;
}

const std::string Counter::report() {
  int value = count_.load(std::memory_order_seq_cst);
  boost::property_tree::ptree ptree;
  ptree.put("name", name_);
  ptree.put("count", value);
  std::ostringstream ss;
  boost::property_tree::write_json(ss, ptree);
  return ss.str();
}

void Counter::clear() {
  count_.store(0, std::memory_order_seq_cst);
}

RatioCounter::RatioCounter(const std::string name) : RatioCounter(name, 0, 0) {

}

RatioCounter::RatioCounter(const std::string name, uint32_t num, uint32_t denom)
    : name_(name), numerator_(num), denominator_(denom) {

}

void RatioCounter::increment(const uint32_t num_incr, const uint32_t denom_incr) {
  ratio_lock_.lock_shared();
  numerator_.fetch_add(num_incr, std::memory_order_relaxed);
  denominator_.fetch_add(denom_incr, std::memory_order_relaxed);
  ratio_lock_.unlock();
}

double RatioCounter::get_ratio() {
  ratio_lock_.lock();
  uint32_t num_value = numerator_.load(std::memory_order_seq_cst);
  uint32_t denom_value = denominator_.load(std::memory_order_seq_cst);
  ratio_lock_.unlock();
  if (denom_value == 0) {
    std::cout << "Ratio " << name_ << " has denominator zero!" << std::endl;
    return std::nan("");
  }
  double ratio = static_cast<double>(num_value) / static_cast<double>(denom_value);
  return ratio;
}

MetricType RatioCounter::type() const {
  return MetricType::RatioCounter;
}

const std::string RatioCounter::report() {
  double ratio = get_ratio();
  boost::property_tree::ptree ptree;
  ptree.put("name", name_);
  ptree.put("ratio", ratio);
  std::ostringstream ss;
  boost::property_tree::write_json(ss, ptree);
  return ss.str();
}

void RatioCounter::clear() {
  ratio_lock_.lock_shared();
  numerator_.store(0, std::memory_order_seq_cst);
  denominator_.store(0, std::memory_order_seq_cst);
  ratio_lock_.unlock();
}

long RealTimeClock::get_time_micros() const {
  clock_t clocks = clock();
  return static_cast<long>(clocks) * CLOCKS_PER_MILLISECOND;
}

void PresetClock::set_time_micros(const long time_micros) {
  time_ = time_micros;
}

long PresetClock::get_time_micros() const {
  return time_;
}

EWMA::EWMA(long tick_interval_seconds, LoadAverage load_average)
    : tick_interval_seconds_(tick_interval_seconds), uncounted_(0) {
  double alpha_exp;
  double alpha_exp_1 = (static_cast<double>(-1 * tick_interval_seconds)) / SECONDS_PER_MINUTE;
  switch (load_average) {
    case LoadAverage::OneMinute:alpha_exp = exp(alpha_exp_1 / ONE_MINUTE);
      break;
    case LoadAverage::FiveMinute:alpha_exp = exp(alpha_exp_1 / FIVE_MINUTES);
      break;
    case LoadAverage::FifteenMinute:alpha_exp = exp(alpha_exp_1 / FIFTEEN_MINUTES);
      break;
  }
  alpha_ = 1 - alpha_exp;
}

void EWMA::tick() {
  rate_lock_.lock();
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
  rate_lock_.unlock();
}

void EWMA::mark_uncounted(uint32_t num) {
  uncounted_.fetch_add(num, std::memory_order_relaxed);
}

void EWMA::reset() {
  rate_lock_.lock();
  rate_ = -1;
  uncounted_.store(0, std::memory_order_seq_cst);
  rate_lock_.unlock();
}

double EWMA::get_rate_seconds() {
  rate_lock_.lock_shared();
  double rate = rate_;
  rate_lock_.unlock();
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
      m15_rate(ewma_tick_interval_seconds_, LoadAverage::FifteenMinute) {

}

void Meter::mark(uint32_t num) {
  count_.fetch_add(num, std::memory_order_relaxed);
  m1_rate.mark_uncounted(num);
  m5_rate.mark_uncounted(num);
  m15_rate.mark_uncounted(num);
}

void Meter::tick_if_necessary() {
  long curr_micros = clock_->get_time_micros();
  long tick_interval_micros = ewma_tick_interval_seconds_ * MICROS_PER_SECOND;
  long last_tick = last_ewma_tick_micros_.load(std::memory_order_seq_cst);
  long time_since_last_tick = curr_micros - last_tick;

  if (time_since_last_tick < tick_interval_micros) {
    // Not enough time has elapsed since the last tick, so we do no work
    return;
  }

  long new_last_tick = curr_micros - (time_since_last_tick % tick_interval_micros);
  bool last_tick_update_successful =
      last_ewma_tick_micros_.compare_exchange_strong(last_tick, new_last_tick, std::memory_order_seq_cst);

  if (last_tick_update_successful) {
    double num_ticks = static_cast<double>(time_since_last_tick) / static_cast<double>(tick_interval_micros);
    for (int i = 0; i < static_cast<int>(num_ticks); i++) {
      m1_rate.tick();
      m5_rate.tick();
      m15_rate.tick();
    }
  }
}

double Meter::get_rate_micros() {
  start_time_lock_.lock_shared();
  uint32_t curr_count = count_.load(std::memory_order_seq_cst);
  long curr_time_micros = clock_->get_time_micros();
  double rate = static_cast<double>(curr_count) / static_cast<double>(curr_time_micros - start_time_micros_);
  start_time_lock_.unlock();
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
  return m15_rate.get_rate_seconds();
}

MetricType Meter::type() const {
  return MetricType::Meter;
}

const std::string Meter::report() {
  boost::property_tree::ptree ptree;
  ptree.put("name", name_);
  ptree.put("unit", unit_);
  ptree.put("rate", get_rate_seconds());
  ptree.put("rate_1min", get_one_minute_rate_seconds());
  ptree.put("rate_5min", get_five_minute_rate_seconds());
  ptree.put("rate_15min", get_fifteen_minute_rate_seconds());
  std::ostringstream ss;
  boost::property_tree::write_json(ss, ptree);
  return ss.str();
}

void Meter::clear() {
  start_time_lock_.lock();
  start_time_micros_ = clock_->get_time_micros();
  count_.store(0, std::memory_order_seq_cst);
  start_time_lock_.unlock();
  m1_rate.reset();
  m5_rate.reset();
  m15_rate.reset();
}

ReservoirSampler::ReservoirSampler(size_t sample_size) : sample_size_(sample_size) {

}

void ReservoirSampler::sample(const int64_t value) {
  if (n_ < sample_size_) {
    reservoir_.push_back(value);
  } else {
    if (reservoir_.size() != sample_size_) {
      throw std::length_error("Reservoir size exceeds sample size!");
    }
    size_t j = rand() % (n_ + 1);
    if (j < sample_size_) {
      reservoir_[j] = value;
    }
  }
  n_++;
}

const std::vector<int64_t> ReservoirSampler::snapshot() const {
  return reservoir_;
}

void ReservoirSampler::clear() {
  reservoir_.clear();
  n_ = 0;
}

HistogramStats::HistogramStats(size_t data_size,
                               int64_t min,
                               int64_t max,
                               double mean,
                               double std_dev,
                               double p50,
                               double p95,
                               double p99)
    : data_size_(data_size), min_(min), max_(max), mean_(mean), std_dev_(std_dev), p50_(p50), p95_(p95), p99_(p99) {

}

Histogram::Histogram(const std::string name, const std::string unit, const size_t sample_size)
    : name_(name), unit_(unit), sampler_(sample_size) {

}

void Histogram::insert(const int64_t value) {
  sampler_lock_.lock();
  sampler_.sample(value);
  sampler_lock_.unlock();
}

double Histogram::percentile(std::vector<int64_t> snapshot, double rank) {
  if (rank < 0 || rank > 1) {
    throw std::invalid_argument("Percentile rank must be in [0,1]!");
  }
  size_t sample_size = snapshot.size();
  if (sample_size <= 0) {
    throw std::length_error("Percentile snapshot must have length greater than zero!");
  }
  std::sort(snapshot.begin(), snapshot.end());
  double x;
  double x_condition = (static_cast<double>(1) / static_cast<double>(sample_size + 1));
  if (rank <= x_condition) {
    x = 1;
  } else if (rank > x_condition && rank < (static_cast<double>(sample_size) * x_condition)) {
    x = rank * static_cast<double>(sample_size + 1);
  } else {
    x = sample_size;
  }
  size_t index = std::floor(x) - 1;
  double v = snapshot[index];
  double remainder = x - std::floor(x);
  if (remainder == 0) {
    return v;
  } else {
    return v + (remainder * (snapshot[index + 1] - snapshot[index]));
  }
}

const HistogramStats Histogram::compute_stats() {
  sampler_lock_.lock_shared();
  std::vector<int64_t> snapshot = sampler_.snapshot();
  sampler_lock_.unlock();
  size_t snapshot_size = snapshot.size();
  if (snapshot_size == 0) {
    HistogramStats stats;
    return stats;
  }
  std::sort(snapshot.begin(), snapshot.end());
  int64_t min = snapshot.front();
  int64_t max = snapshot.back();
  double p50 = percentile(snapshot, .5);
  double p95 = percentile(snapshot, .95);
  double p99 = percentile(snapshot, .99);
  double mean =
      static_cast<double>(std::accumulate(snapshot.begin(), snapshot.end(), 0)) / static_cast<double>(snapshot_size);
  double var = 0;
  if (snapshot_size > 1) {
    for (auto elem : snapshot) {
      double incr = std::pow((static_cast<double>(elem) - mean), 2);
      var += incr;
    }
    var = var / static_cast<double>(snapshot_size);
  }
  double std_dev = std::sqrt(var);
  return HistogramStats(snapshot_size, min, max, mean, std_dev, p50, p95, p99);
}

MetricType Histogram::type() const {
  return MetricType::Histogram;
}

const std::string Histogram::report() {
  HistogramStats stats = compute_stats();
  boost::property_tree::ptree ptree;
  ptree.put("name", name_);
  ptree.put("unit", unit_);
  ptree.put("size", stats.data_size_);
  ptree.put("min", stats.min_);
  ptree.put("max", stats.max_);
  ptree.put("mean", stats.mean_);
  ptree.put("std_dev", stats.std_dev_);
  ptree.put("p50", stats.p50_);
  ptree.put("p95", stats.p95_);
  ptree.put("p99", stats.p99_);
  std::ostringstream ss;
  boost::property_tree::write_json(ss, ptree);
  return ss.str();
}

void Histogram::clear() {
  sampler_lock_.lock();
  sampler_.clear();
  sampler_lock_.unlock();
}

} // namespace metrics

} // namespace clipper