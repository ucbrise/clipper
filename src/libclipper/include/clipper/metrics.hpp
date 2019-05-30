#ifndef CLIPPER_METRICS_HPP
#define CLIPPER_METRICS_HPP

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <boost/property_tree/ptree.hpp>

namespace clipper {

namespace metrics {

using std::vector;

const std::string LOGGING_TAG_METRICS = "METRICS";

enum class MetricType {
  Counter = 0,
  RatioCounter = 1,
  Meter = 2,
  Histogram = 3
};

class Metric {
 public:
  /**
   * @return The metric's type (Counter, RatioCounter, Meter, etc...)
   */
  virtual MetricType type() const = 0;
  /**
   *
   * @return The name of the metric, generally user-defined
   */
  virtual const std::string name() const = 0;
  /**
   * @return A boost property tree containing relevant metric attributes
   */
  virtual const boost::property_tree::ptree report_tree() = 0;
  /**
   * @return A json-formatted string containing relevant metric attributes
   */
  virtual const std::string report_str() = 0;
  /**
   * Resets all metric attributes to their default values (typically zero)
   */
  virtual void clear() = 0;
};

class Counter : public Metric {
 public:
  /** Creates a Counter with initial count zero */
  explicit Counter(const std::string name);
  explicit Counter(const std::string name, const int initial_count);

  // Disallow copy and move
  Counter(Counter &other) = delete;
  Counter &operator=(Counter &other) = delete;
  Counter(Counter &&other) = delete;
  Counter &operator=(Counter &&other) = delete;

  void increment(const int value);
  void decrement(const int value);
  int value() const;

  // Metric implementation
  MetricType type() const override;
  const std::string name() const override;
  const boost::property_tree::ptree report_tree() override;
  const std::string report_str() override;
  void clear() override;

 private:
  const std::string name_;
  std::atomic_int count_;
};

/**
 * Represents a numerator/denominator ratio, where both
 * numerator and denominator are positive integers.
 */
class RatioCounter : public Metric {
 public:
  /** Creates a RatioCounter with numerator 0 and denominator 0 **/
  explicit RatioCounter(const std::string name);
  explicit RatioCounter(const std::string name, const uint32_t num,
                        const uint32_t denom);

  // Disallow copy and move
  RatioCounter(RatioCounter &other) = delete;
  RatioCounter &operator=(RatioCounter &other) = delete;
  RatioCounter(RatioCounter &&other) = delete;
  RatioCounter &operator=(RatioCounter &&other) = delete;

  void increment(const uint32_t num_incr, const uint32_t denom_incr);
  double get_ratio();

  // Metric implementation
  MetricType type() const override;
  const std::string name() const override;
  const boost::property_tree::ptree report_tree() override;
  const std::string report_str() override;
  void clear() override;

 private:
  const std::string name_;
  std::mutex ratio_lock_;
  std::atomic<uint32_t> numerator_;
  std::atomic<uint32_t> denominator_;
};

class MeterClock {
 public:
  virtual long get_time_micros() const = 0;
};

class RealTimeClock : public MeterClock {
 public:
  long get_time_micros() const override;
};

/**
 * A manually adjustable clock used for testing
 * exponentially weighted moving average (EWMA)
 * functionality
 */
class PresetClock : public MeterClock {
 public:
  long get_time_micros() const override;
  void set_time_micros(const long time_micros);

 private:
  long time_ = 0;
};

/// Represents the different load average options for exponentially
/// weighted moving averages (EWMA) within meters. For a one minute EWMA,
/// we use the OneMinute load average, etc...
enum class LoadAverage { OneMinute, FiveMinute, FifteenMinute };

/// Represents an exponentially weighted moving average.
/// Multiple EWMAs are included within a single meter
/// corresponding to different load averages.
///
/// The EWMA is updated using "ticks" at a frequency determined
/// by a specified tick interval. For a detailed explanation of the update
/// (tick) procedure and decay mechanisms, see the following:
/// 1. http://www.teamquest.com/pdfs/whitepaper/ldavg1.pdf
/// 2. http://www.teamquest.com/pdfs/whitepaper/ldavg2.pdf
/// 3. http://en.wikipedia.org/wiki/Moving_average#Exponential_moving_average
class EWMA {
 public:
  EWMA(long tick_interval_seconds, LoadAverage load_average);
  void tick();
  void mark_uncounted(uint32_t num);
  void reset();
  double get_rate_seconds();

 private:
  // The time interval between ticks
  const long tick_interval_seconds_;
  // The exponential decay factor applied to the previous
  // rate at every tick
  double alpha_;
  double rate_ = -1;
  std::mutex rate_lock_;
  // The number of new events to be included
  // in the rate at the next tick
  std::atomic<uint32_t> uncounted_;
};

class Meter : public Metric {
 public:
  explicit Meter(std::string name, std::shared_ptr<MeterClock> clock);

  // Disallow copy and move
  Meter(Meter &other) = delete;
  Meter &operator=(Meter &other) = delete;
  Meter(Meter &&other) = delete;
  Meter &operator=(Meter &&other) = delete;

  void mark(uint32_t num);

  /**
   * @return The rate of this meter, in events-per-microsecond, since
   * the time of initialization
   */
  double get_rate_micros();

  /**
   * @return The rate of this meter, in events-per-second, since the
   * time of initialization
   */
  double get_rate_seconds();

  /**
   * @return The rate of this meter, in events-per-second, for the last minute
   * This rate is calculated using an expontentially weighted moving average
   */
  double get_one_minute_rate_seconds();

  /**
   * @return the rate of this meter, in events-per-second, for the last five
   * minutes
   * This rate is calculated using an expontentially weighted moving average.
   */
  double get_five_minute_rate_seconds();

  /**
   * @return the rate of this meter, in events-per-second, for the last fifteen
   * minutes
   * This rate is calculated using an expontentially weighted moving average.
   */
  double get_fifteen_minute_rate_seconds();

  // Metric implementation
  MetricType type() const override;
  const std::string name() const override;
  const boost::property_tree::ptree report_tree() override;
  const std::string report_str() override;
  void clear() override;

 private:
  const std::string unit_ = "events per second";
  std::string name_;
  std::shared_ptr<MeterClock> clock_;
  std::atomic<uint32_t> count_;
  std::mutex start_time_lock_;
  long start_time_micros_;

  // EWMA
  void tick_if_necessary();
  const long ewma_tick_interval_seconds_ = 5;
  std::atomic_long last_ewma_tick_micros_;
  EWMA m1_rate;
  EWMA m5_rate;
  EWMA m15_rate;
};

class ReservoirSampler {
 public:
  explicit ReservoirSampler(size_t sample_size);

  // Disallow copy and move
  ReservoirSampler(ReservoirSampler &other) = delete;
  ReservoirSampler &operator=(ReservoirSampler &other) = delete;
  ReservoirSampler(ReservoirSampler &&other) = delete;
  ReservoirSampler &operator=(ReservoirSampler &&other) = delete;

  /**
   * Inserts a value into the reservoir, potentially
   * removing an old value if the reservoir is at maximum
   * capacity
   *
   * @param value The new value to be inserted
   * @return The value that was removed, or zero if no value was removed
   */
  int64_t sample(const int64_t value);
  void clear();
  const std::vector<int64_t> snapshot() const;
  size_t current_size() const;

 private:
  size_t sample_size_;
  size_t n_ = 0;
  std::vector<int64_t> reservoir_;
};

class HistogramStats {
 public:
  /** Constructs a HistogramStats object with all values zero **/
  explicit HistogramStats(){};
  explicit HistogramStats(size_t data_size, int64_t min, int64_t max,
                          long double mean, long double std_dev,
                          long double p50, long double p95, long double p99);

  size_t data_size_ = 0;
  int64_t min_ = 0;
  int64_t max_ = 0;
  long double mean_ = 0;
  long double std_dev_ = 0;
  long double p50_ = 0;
  long double p95_ = 0;
  long double p99_ = 0;
};

class Histogram : public Metric {
 public:
  explicit Histogram(const std::string name, const std::string unit,
                     const size_t sample_size);

  // Disallow copy and move
  Histogram(Histogram &other) = delete;
  Histogram &operator=(Histogram &other) = delete;
  Histogram(Histogram &&other) = delete;
  Histogram &operator=(Histogram &&other) = delete;

  void insert(const int64_t value);
  const HistogramStats compute_stats();
  static long double percentile(std::vector<int64_t> snapshot, double rank);
  // This method obtains a snapshot from the histogram's reservoir sampler
  // and then calculates the percentile
  long double percentile(double rank);

  // Metric implementation
  MetricType type() const override;
  const std::string name() const override;
  const std::string report_str() override;
  const boost::property_tree::ptree report_tree() override;
  void clear() override;

 private:
  /**
   * @param old_reservoir_size The old size of the reservoir
   * @param new_reservoir_size The new size of the reservoir
   * @param new_value The value that was just inserted into the reservoir
   * @param old_value The value that was just removed from the reservoir (use
   * zero if nothing was removed)
   */
  void update_mean(const size_t old_reservoir_size,
                   const size_t new_reservoir_size, const int64_t new_value,
                   const int64_t old_value);

  std::string name_;
  std::string unit_;
  long double mean_ = 0;
  ReservoirSampler sampler_;
  std::mutex sampler_lock_;
};

/**
 * Obtains a human readable string representation of the provided MetricType
 */
const std::string get_metrics_category_name(MetricType type);

/**
 * Singleton object that manages creation, logging, and persistence
 * of system metrics.
 */
class MetricsRegistry {
 public:
  /**
   * Obtains an instance of the MetricsRegistry singleton
   * that can be used to create new metrics
   */
  static MetricsRegistry &get_metrics();
  /**
   * @return A JSON-formatted string containing structured reports
   * of all metrics in the registry. The schema is as follows:
   * {
   *  "counters" : [{counter_name: counter_report}, {counter_name:
   * counter_report}],
   *  "histograms": [{counter_name: counter_report}, {counter_name:
   * counter_report}],
   *  ...
   * }
   */
  const std::string report_metrics(const bool clear = false);

  /** Creates a Counter with initial value zero */
  std::shared_ptr<Counter> create_counter(const std::string name);
  std::shared_ptr<Counter> create_counter(const std::string name,
                                          const int initial_count);
  /** Creates a RatioCounter with initial value zero */
  std::shared_ptr<RatioCounter> create_ratio_counter(const std::string name);
  std::shared_ptr<RatioCounter> create_ratio_counter(const std::string name,
                                                     const uint32_t num,
                                                     const uint32_t denom);
  std::shared_ptr<Meter> create_meter(const std::string name);
  std::shared_ptr<Histogram> create_histogram(const std::string name,
                                              const std::string unit,
                                              const size_t sample_size);

  void delete_metric(std::shared_ptr<Metric> target);

 private:
  MetricsRegistry();
  MetricsRegistry(MetricsRegistry &other) = delete;
  MetricsRegistry &operator=(MetricsRegistry &other) = delete;
  MetricsRegistry(MetricsRegistry &&other) = delete;
  MetricsRegistry &operator=(MetricsRegistry &&other) = delete;

  void manage_metrics();
  std::shared_ptr<vector<std::shared_ptr<Metric>>> metrics_;
  std::shared_ptr<std::mutex> metrics_lock_;
};

}  // namespace metrics

}  // namespace clipper

#endif  // CLIPPER_METRICS_HPP
