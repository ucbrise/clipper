#include <unordered_map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <clipper/json_util.hpp>

#include "include/bench_utils.hpp"

using namespace clipper;

namespace bench_utils {

const std::string KEY_PATH = "cifar_data_path";
const std::string NUM_THREADS = "num_threads";
const std::string NUM_BATCHES = "num_batches";
const std::string BATCH_SIZE = "batch_size";
const std::string BATCH_DELAY_MILLIS = "batch_delay_millis";

std::unordered_map<std::string, std::string> create_config_(
        std::string path, std::string num_threads, std::string num_batches,
        std::string batch_size, std::string batch_delay) {
  std::unordered_map<std::string, std::string> config;
  config.emplace(CONFIG_KEY_PATH, path);
  config.emplace(CONFIG_KEY_NUM_THREADS, num_threads);
  config.emplace(CONFIG_KEY_NUM_BATCHES, num_batches);
  config.emplace(CONFIG_KEY_BATCH_SIZE, batch_size);
  config.emplace(CONFIG_KEY_BATCH_DELAY, batch_delay);
  return config;
};

std::unordered_map<std::string, std::string> get_config_from_prompt() {
  std::string path;
  std::string num_threads;
  std::string num_batches;
  std::string batch_size;
  std::string batch_delay;

  std::cout << "Before proceeding, run bench/setup_bench.sh from clipper's "
          "root directory."
            << std::endl;
  std::cout << "Enter a path to the CIFAR10 binary data set: ";
  std::cin >> path;
  std::cout << "Enter the number of threads of execution: ";
  std::cin >> num_threads;
  std::cout
          << "Enter the number of request batches to be sent by each thread: ";
  std::cin >> num_batches;
  std::cout << "Enter the number of requests per batch: ";
  std::cin >> batch_size;
  std::cout << "Enter the delay between batches, in milliseconds: ";
  std::cin >> batch_delay;

  return create_config_(path, num_threads, num_batches, batch_size, batch_delay);
};

std::unordered_map<std::string, std::string> get_config_from_json(
        std::string json_path) {
  std::ifstream json_file(json_path);
  std::stringstream buffer;
  buffer << json_file.rdbuf();
  std::string json_text = buffer.str();
  rapidjson::Document d;
  json::parse_json(json_text, d);
  std::string cifar_path = json::get_string(d, KEY_PATH.c_str());
  std::string num_threads = json::get_string(d, NUM_THREADS.c_str());
  std::string num_batches = json::get_string(d, NUM_BATCHES.c_str());
  std::string batch_size = json::get_string(d, BATCH_SIZE.c_str());
  std::string batch_delay = json::get_string(d, BATCH_DELAY_MILLIS.c_str());
  return create_config_(cifar_path, num_threads, num_batches, batch_size,
                        batch_delay);
};

} // namespace bench_utils
