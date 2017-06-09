//#ifndef BENCH_UTILS_HPP
//#define BENCH_UTILS_HPP

#include <unordered_map>

namespace bench_utils {

const std::string CIFAR_DATA_PATH = "cifar_data_path";
const std::string NUM_THREADS = "num_threads";
const std::string NUM_BATCHES = "num_batches";
const std::string BATCH_SIZE = "batch_size";
const std::string BATCH_DELAY_MILLIS = "batch_delay_millis";
const std::string LATENCY_OBJECTIVE = "latency_objective";
const std::string REPORT_DELAY_SECONDS = "report_delay_seconds";
const std::string REPORTS_PATH = "reports_path";
const std::string REPORTS_PATH_VERBOSE = "reports_path_verbose";

/**
 * Creates a configuration from data received through the command prompt
 */
std::unordered_map<std::string, std::string> get_config_from_prompt(
    std::string setup_message, std::vector<std::string> desired_vars);

/**
 * Loads a configuration from the json file at the specified path
 */
std::unordered_map<std::string, std::string> get_config_from_json(
    std::string json_path, std::vector<std::string> desired_vars);

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
