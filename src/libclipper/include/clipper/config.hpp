#ifndef CLIPPER_LIB_CONFIG_HPP
#define CLIPPER_LIB_CONFIG_HPP

#include <cassert>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

// TODO: Change the name of this file.
namespace clipper {

const std::string DEFAULT_REDIS_ADDRESS("localhost");
constexpr int DEFAULT_REDIS_PORT = 6379;
constexpr int DEFAULT_TASK_EXECUTION_THREADPOOL_SIZE = 4;
constexpr long DEFAULT_PREDICTION_CACHE_SIZE_BYTES = 33554432;  // 32 MiB

/**
 * Globally readable constant configuration.
 *
 * Update any settings from their defaults in the main thread
 * (i.e. based on runtime options or a configuration file).
 * Once all updates have been written, mark the config as
 * ready: `get_config().ready()
 *
 * __NOTE:__ This class is not thread safe.
 * Any updates to configuration settings MUST be done
 * in the main thread before starting any threads that might
 * access this config.
 */
struct Config {
 public:
  explicit Config()
      : readable_(false),
        redis_address_(DEFAULT_REDIS_ADDRESS),
        redis_port_(DEFAULT_REDIS_PORT),
        task_execution_threadpool_size_(DEFAULT_TASK_EXECUTION_THREADPOOL_SIZE),
        prediction_cache_size_bytes_(DEFAULT_PREDICTION_CACHE_SIZE_BYTES) {}

  /**
   * For unit testing only!
   */
  void reset() {
    readable_ = false;
    redis_address_ = DEFAULT_REDIS_ADDRESS;
    redis_port_ = DEFAULT_REDIS_PORT;
    task_execution_threadpool_size_ = DEFAULT_TASK_EXECUTION_THREADPOOL_SIZE;
    prediction_cache_size_bytes_ = DEFAULT_PREDICTION_CACHE_SIZE_BYTES;
  }

  void ready() { readable_ = true; }

  bool is_readable() const { return readable_; }

  std::string get_redis_address() const {
    if (!readable_) {
      // TODO: use a better exception
      throw std::logic_error("Cannot read Config until ready");
    }
    assert(readable_);
    return redis_address_;
  }

  void set_redis_address(const std::string& address) {
    if (readable_) {
      // TODO: use a better exception
      throw std::logic_error("Cannot write to Config after ready");
    }
    assert(!readable_);
    redis_address_ = address;
  }

  int get_redis_port() const {
    if (!readable_) {
      // TODO: use a better exception
      throw std::logic_error("Cannot read Config until ready");
    }
    assert(readable_);
    return redis_port_;
  }

  void set_redis_port(int port) {
    if (readable_) {
      // TODO: use a better exception
      throw std::logic_error("Cannot write to Config after ready");
    }
    assert(!readable_);
    redis_port_ = port;
  }

  int get_task_execution_threadpool_size() const {
    if (!readable_) {
      // TODO: use a better exception
      throw std::logic_error("Cannot read Config until ready");
    }
    assert(readable_);
    return task_execution_threadpool_size_;
  }

  void set_task_execution_threadpool_size(int size) {
    if (readable_) {
      // TODO: use a better exception
      throw std::logic_error("Cannot write to Config after ready");
    }
    assert(!readable_);
    if(size <= 0) {
      std::stringstream ss;
      ss << "Task execution threadpool size must be positive! Attempted to set a size of: " << size;
      throw std::invalid_argument(ss.str());
    }
    task_execution_threadpool_size_ = size;
  }

  long get_prediction_cache_size() {
    if (!readable_) {
      // TODO: use a better exception
      throw std::logic_error("Cannot read Config until ready");
    }
    assert(readable_);
    return prediction_cache_size_bytes_;
  }

  void set_prediction_cache_size(long size_bytes) {
    if (readable_) {
      // TODO: use a better exception
      throw std::logic_error("Cannot write to Config after ready");
    }
    assert(!readable_);
    if (size_bytes < 0) {
      std::stringstream ss;
      ss << "Prediction cache size cannot be negative! Attempted to set a "
            "cache size of: "
         << size_bytes << " bytes";
      throw std::invalid_argument(ss.str());
    }
    prediction_cache_size_bytes_ = size_bytes;
  }

 private:
  bool readable_;
  std::string redis_address_;
  int redis_port_;
  int task_execution_threadpool_size_;
  long prediction_cache_size_bytes_;
};

inline Config& get_config() {
  static Config config;
  return config;
}

}  // namespace clipper
#endif
