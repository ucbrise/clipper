#include <atomic>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <string>

#ifndef CLIPPER_METRICS_HPP
#define CLIPPER_METRICS_HPP

namespace clipper {

namespace metrics {

using std::vector;

enum class MetricType {
  Counter = 0,
  RatioCounter = 1,
  Meter = 2,
  Histogram = 3
};

class Metric {
 public:
  virtual MetricType type() const = 0;
  virtual void report() = 0;
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
  void report() override;
  void clear() override;

 private:
  const std::string name_;
  std::atomic_int count_;

};

/**
 * Represents a numerator/denominator ratio, where both
 * numerator and denominator are positive integers.
 *
 * Note: To prevent race conditions, all calls are blocking!
 * TODO(Corey-Zumar): Explore alternative solution to blocking calls
 */
class RatioCounter : public Metric {
 public:
  /** Creates a RatioCounter with numerator 0 and denominator 0 **/
  explicit RatioCounter(const std::string name);
  explicit RatioCounter(const std::string name, const uint32_t num, const uint32_t denom);

  // Disallow copy and move
  RatioCounter(RatioCounter &other) = delete;
  RatioCounter &operator=(RatioCounter &other) = delete;
  RatioCounter(RatioCounter &&other) = delete;
  RatioCounter &operator=(RatioCounter &&other) = delete;

  void increment(const uint32_t num_incr, const uint32_t denom_incr);
  double get_ratio();

  // Metric implementation
  MetricType type() const override;
  void report() override;
  void clear() override;

 private:
  const std::string name_;
  std::mutex ratio_lock_;
  uint32_t numerator_;
  uint32_t denominator_;

};

class MeterClock {
 public:
  virtual long get_time_micros() const = 0;
};

class RealTimeClock : public MeterClock {
 public:
  long get_time_micros() const override;
};

class PresetClock : public MeterClock {
 public:
  long get_time_micros() const override;
  void set_time_micros(const long time_micros);

 private:
  long time_ = 0;
};

enum class LoadAverage {
  OneMinute,
  FiveMinute,
  FifteenMinute
};

class EWMA {
 public:
  EWMA(long tick_interval, LoadAverage load_average);
  void tick();
  void mark_uncounted(uint32_t num);
  void reset();
  double get_rate_seconds();

 private:
  const long tick_interval_seconds_;
  double alpha_;
  double rate_ = -1;
  std::shared_timed_mutex rate_lock_;
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
   * @return the rate of this meter, in events-per-second, for the last five minutes
   * This rate is calculated using an expontentially weighted moving average.
   */
  double get_five_minute_rate_seconds();

  /**
   * @return the rate of this meter, in events-per-second, for the last fifteen minutes
   * This rate is calculated using an expontentially weighted moving average.
   */
  double get_fifteen_minute_rate_seconds();

  // Metric implementation
  MetricType type() const override;
  void report() override;
  void clear() override;

 private:
  const std::string unit_ = std::string("events per second");
  std::string name_;
  std::shared_ptr<MeterClock> clock_;
  std::atomic<uint32_t> count_;
  std::shared_timed_mutex start_time_lock_;
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

  void sample(const int64_t value);
  void clear();
  const std::vector<int64_t> snapshot() const;

 private:
  size_t sample_size_;
  size_t n_ = 0;
  std::vector<int64_t> reservoir_;

};

class HistogramStats {
 public:
  /** Constructs a HistogramStats object with all values zero **/
  explicit HistogramStats() {};
  explicit HistogramStats(size_t data_size,
                          int64_t min,
                          int64_t max,
                          double mean,
                          double std_dev,
                          double p50,
                          double p95,
                          double p99);
  const std::string to_reportable_string() const;

  size_t data_size_ = 0;
  int64_t min_ = 0;
  int64_t max_ = 0;
  double mean_ = 0;
  double std_dev_ = 0;
  double p50_ = 0;
  double p95_ = 0;
  double p99_ = 0;
};

class Histogram : public Metric {
 public:
  explicit Histogram(const std::string name, const size_t sample_size);

  // Disallow copy and move
  Histogram(Histogram &other) = delete;
  Histogram &operator=(Histogram &other) = delete;
  Histogram(Histogram &&other) = delete;
  Histogram &operator=(Histogram &&other) = delete;

  void insert(const int64_t value);
  const HistogramStats compute_stats();
  static double percentile(std::vector<int64_t> snapshot, double rank);

  // Metric implementation
  MetricType type() const override;
  void report() override;
  void clear() override;

 private:
  std::string name_;
  ReservoirSampler sampler_;
  std::shared_timed_mutex sampler_lock_;

};

class MetricsRegistry {

 public:
  ~MetricsRegistry();
  static MetricsRegistry &instance();

  /** Creates a Counter with initial value zero */
  std::shared_ptr<Counter> create_default_counter(const std::string name);
  std::shared_ptr<Counter> create_counter(const std::string name, const int initial_count);
  /** Creates a RatioCounter with initial value zero */
  std::shared_ptr<RatioCounter> create_default_ratio_counter(const std::string name);
  std::shared_ptr<RatioCounter> create_ratio_counter(const std::string name, const uint32_t num, const uint32_t denom);
  std::shared_ptr<Meter> create_meter(const std::string name);
  std::shared_ptr<Histogram> create_histogram(const std::string name, const size_t sample_size);

 private:
  MetricsRegistry();
  MetricsRegistry(MetricsRegistry &other) = delete;
  MetricsRegistry &operator=(MetricsRegistry &other) = delete;
  MetricsRegistry(MetricsRegistry &&other) = delete;
  MetricsRegistry &operator=(MetricsRegistry &&other) = delete;

  void manage_metrics(std::shared_ptr<vector<std::shared_ptr<Metric>>> metrics,
                      std::shared_ptr<std::mutex> metrics_lock,
                      bool &active);
  std::shared_ptr<vector<std::shared_ptr<Metric>>> metrics_;
  std::shared_ptr<std::mutex> metrics_lock_;
  bool active_ = true;

};

} // namespace metrics

} // namespace clipper

#endif //CLIPPER_METRICS_HPP
