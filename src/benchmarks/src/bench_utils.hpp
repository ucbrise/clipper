#include <clipper/metrics.hpp>
#include <unordered_map>

namespace bench_utils {

class BenchMetrics {
 public:
  BenchMetrics(std::string app_name);
  ~BenchMetrics() = default;

  BenchMetrics(const BenchMetrics &) = default;

  BenchMetrics &operator=(const BenchMetrics &) = default;

  BenchMetrics(BenchMetrics &&) = default;

  BenchMetrics &operator=(BenchMetrics &&) = default;

  std::string app_name_;
  std::shared_ptr<clipper::metrics::Histogram> latency_;
  std::shared_ptr<clipper::metrics::Meter> throughput_;
  std::shared_ptr<clipper::metrics::Meter> request_throughput_;
  std::shared_ptr<clipper::metrics::Counter> num_predictions_;
  std::shared_ptr<clipper::metrics::RatioCounter> default_pred_ratio_;
};

/**
 * Loads a configuration from the json file at the specified path
 */
std::unordered_map<std::string, std::string> get_config_from_json(
    std::string json_path, std::vector<std::string> desired_vars);

/**
 * Loads CIFAR data from the specified path
 */
std::unordered_map<int, std::vector<std::vector<double>>> load_cifar(
    std::string &cifar_data_path);

/**
 * Returns a vector of all the cifar datapoints.
 * Warning: this function mutates the input cifar data.
 */
std::vector<std::vector<double>> concatenate_cifar_datapoints(
    std::unordered_map<int, std::vector<std::vector<double>>> cifar_data);

/**
 * Returns the value corresponding to `key` in `config` as a string
 */
std::string get_str(const std::string &key,
                    std::unordered_map<std::string, std::string> &config);

/**
 * Returns the value corresponding to `key` in `config` as an int
 */
int get_int(const std::string &key,
            std::unordered_map<std::string, std::string> &config);

/**
 * Returns the value corresponding to `key` in `config` as a long
 */
long get_long(const std::string &key,
              std::unordered_map<std::string, std::string> &config);

/**
 * Returns true iff the value corresponding to `key` in `config` is "true"
 */
bool get_bool(const std::string &key,
              std::unordered_map<std::string, std::string> &config);

}  // namespace bench_utils
