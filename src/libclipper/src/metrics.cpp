#include <atomic>
#include <string>
#include <vector>

#include <clipper/metrics.hpp>
#include <clipper/util.hpp>

namespace clipper {

MetricsRegistry::MetricsRegistry()
    : metrics_(std::make_shared<std::vector<std::shared_ptr<Metric>>>()),
      metrics_lock_(std::make_shared<std::mutex>()) {
  boost::thread(boost::bind(&MetricsRegistry::manage_metrics, this, metrics_,
                            metrics_lock_, boost::ref(active_)))
  .detach();
}

MetricsRegistry::~MetricsRegistry() {
  active_ = false;
}

MetricsRegistry &MetricsRegistry::instance() {
  static MetricsRegistry instance;
  return instance;
}

void MetricsRegistry::manage_metrics(std::shared_ptr<std::vector<std::shared_ptr<Metric>>> metrics,
                                     std::shared_ptr<std::mutex> metrics_lock,
                                     bool &active) {
  while (true) {
    if (!active) {
      return;
    }
    if (!metrics) {

    }
    std::lock_guard<std::mutex> guard(*metrics_lock);
    // Do logging, clearing here!
  }
}

std::shared_ptr<Counter> MetricsRegistry::create_counter(const std::string name, const int initial_count) {
  std::shared_ptr<Counter> counter = std::make_shared<Counter>(name, initial_count);
  metrics_->push_back(counter);
  return counter;
}

std::shared_ptr<Counter> MetricsRegistry::create_default_counter(const std::string name) {
  return create_counter(name, 0);
}

Counter::Counter(const std::string name) : Counter(name, 1) {

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

void Counter::report() const {
  int value = count_.load(std::memory_order_seq_cst);
  std::cout << "name: " << name_ << std::endl;
  std::cout << "count: " << value << std::endl;
}

void Counter::clear() {
  count_.store(0, std::memory_order_seq_cst);
}

RatioCounter::RatioCounter(const std::string name) : name_(name), RatioCounter(name, 0, 0) {

}

RatioCounter::RatioCounter(const std::string name, uint32_t num, uint32_t denom)
    : name_(name), numerator_(num), denominator_(denom) {

}

void RatioCounter::increment(const uint32_t num_incr, const uint32_t denom_incr) {
  std::lock_guard<std::mutex> guard(ratio_lock_);
  numerator_ += num_incr;
  denominator_ += denom_incr;
}

double RatioCounter::get_ratio() const {
  std::lock_guard<std::mutex> guard(ratio_lock_);
  if (denominator_ == 0) {
    std::cout << "Ratio " << name_ << " has denominator zero!";
    return std::nan("");
  }
  double ratio = static_cast<double>(numerator_) / static_cast<double>(denominator_);
  return ratio;
}

void RatioCounter::report() const {
  double ratio = get_ratio();
  std::cout << "name: " << name_ << std::endl;
  std::cout << "ratio: " << ratio << std::endl;
}

void RatioCounter::clear() {
  std::lock_guard<std::mutex> guard(ratio_lock_);
  numerator_ = 0;
  denominator_ = 0;
}

} // namespace clipper