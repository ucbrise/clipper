#include <clipper/json_util.hpp>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>

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

  return create_config_(path, num_threads, num_batches, batch_size,
                        batch_delay);
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

std::unordered_map<int, std::vector<std::vector<double>>> load_cifar(
    std::unordered_map<std::string, std::string> &config) {
  std::ifstream cifar_file(config.find(CONFIG_KEY_PATH)->second,
                           std::ios::binary);
  std::istreambuf_iterator<char> cifar_data(cifar_file);
  std::unordered_map<int, std::vector<std::vector<double>>> vecs_map;
  for (int i = 0; i < 10000; i++) {
    int label = static_cast<int>(*cifar_data);
    cifar_data++;
    std::vector<uint8_t> cifar_byte_vec;
    cifar_byte_vec.reserve(3072);
    std::copy_n(cifar_data, 3072, std::back_inserter(cifar_byte_vec));
    cifar_data++;
    std::vector<double> cifar_double_vec(cifar_byte_vec.begin(),
                                         cifar_byte_vec.end());

    std::unordered_map<int, std::vector<std::vector<double>>>::iterator
        label_vecs = vecs_map.find(label);
    if (label_vecs != vecs_map.end()) {
      label_vecs->second.push_back(cifar_double_vec);
    } else {
      std::vector<std::vector<double>> new_label_vecs;
      new_label_vecs.push_back(cifar_double_vec);
      vecs_map.emplace(label, new_label_vecs);
    }
  }
  return vecs_map;
}

std::vector<std::vector<double>> concatenate_cifar_datapoints(
    std::unordered_map<int, std::vector<std::vector<double>>> cifar_data) {
  std::vector<std::vector<double>> planes_vecs = cifar_data.find(0)->second;
  std::vector<std::vector<double>> birds_vecs = cifar_data.find(2)->second;

  planes_vecs.insert(planes_vecs.end(), birds_vecs.begin(), birds_vecs.end());
  return planes_vecs;
}

}  // namespace bench_utils
