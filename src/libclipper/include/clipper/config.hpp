#ifndef CLIPPER_LIB_CONFIG_HPP
#define CLIPPER_LIB_CONFIG_HPP

#include <cassert>
#include <stdexcept>
#include <string>
#include <utility>

// TODO: Change the name of this file.
namespace clipper {

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
      : readable_(false), redis_address_("localhost"), redis_port_(6379) {}

  /**
   * For unit testing only!
   */
  void reset() {
    readable_ = false;
    redis_address_ = "localhost";
    redis_port_ = 6379;
  }

  void ready() { readable_ = true; }

  std::string get_redis_address() const {
    if (!readable_) {
      // TODO: use a better exception
      throw std::logic_error("Cannot read Config until ready");
    }
    assert(readable_);
    return redis_address_;
  }

  bool is_readable() const { return readable_; }

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

 private:
  bool readable_;
  std::string redis_address_;
  int redis_port_;
};

inline Config& get_config() {
  static Config config;
  return config;
}

}  // namespace clipper
#endif
