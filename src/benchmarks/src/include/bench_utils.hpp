//#ifndef BENCH_UTILS_HPP
//#define BENCH_UTILS_HPP

#include <unordered_map>

namespace bench_utils {

static std::string CONFIG_KEY_PATH = "path";
static std::string CONFIG_KEY_NUM_THREADS = "num_threads";
static std::string CONFIG_KEY_NUM_BATCHES = "num_batches";
static std::string CONFIG_KEY_BATCH_SIZE = "batch_size";
static std::string CONFIG_KEY_BATCH_DELAY = "batch_delay";

/**
 * Creates a configuration from data received through the command prompt
 */
std::unordered_map<std::string, std::string> get_config_from_prompt();

/**
 * Loads a configuration from the json file at the specified path
 */
std::unordered_map<std::string, std::string> get_config_from_json(
    std::string json_path);

/**
 * Loads CIFAR data from the specified path
 */
std::unordered_map<int, std::vector<std::vector<double>>> load_cifar(
    std::unordered_map<std::string, std::string> &config);

/**
 * Returns a vector of all datapoints provided cifar data, ignoring labels.
 * Warning: this function mutates the input cifar data.
 */
std::vector<std::vector<double>> concatenate_cifar_datapoints(
    std::unordered_map<int, std::vector<std::vector<double>>> cifar_data);

}  // namespace bench_utils

//#endif  // BENCH_UTILS_HPP
