#include <atomic>
#include <vector>

#include <clipper/util.hpp>

#ifndef CLIPPER_METRICS_HPP
#define CLIPPER_METRICS_HPP

namespace clipper {

using std::vector;

class Metric {
 public:
  virtual void report() const = 0;
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
  void report() const;
  void clear();

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
  double get_ratio() const;

  // Metric implementation
  void report() const;
  void clear();

 private:
  const std::string name_;
  std::mutex ratio_lock_;
  uint32_t numerator_;
  uint32_t denominator_;

};

class Meter : public Metric {
 public:

  // Metric implementation
  void report() const;
  void clear();

 private:

};

class Histogram : public Metric {
 public:

  // Metric implementation
  void report() const;
  void clear();

 private:

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
  size_t sample_size;
  std::vector<int64_t> reservoir;

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
  std::shared_ptr<Meter> create_meter(Meter meter);
  std::shared_ptr<Histogram> create_histogram(Histogram histogram);

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

} // namespace clipper

#endif //CLIPPER_METRICS_HPP
