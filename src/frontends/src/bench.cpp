#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <functional>

#include <time.h>

#include <boost/thread.hpp>

#include <clipper/constants.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/logging.hpp>
#include <clipper/query_processor.hpp>
#include <clipper/future.hpp>
#include <clipper/json_util.hpp>
#include <fstream>
#include <rapidjson/document.h>
#include "../../libs/cxxopts/cxxopts.hpp"

using namespace clipper;

const std::string SKLEARN_MODEL_NAME = "bench_sklearn_cifar";
const std::string CONFIG_KEY_PATH = "path";
const std::string CONFIG_KEY_NUM_THREADS = "num_threads";
const std::string CONFIG_KEY_NUM_BATCHES = "num_batches";
const std::string CONFIG_KEY_BATCH_SIZE = "batch_size";
const std::string CONFIG_KEY_BATCH_DELAY = "batch_delay";

constexpr double SKLEARN_PLANE_LABEL = 1;
constexpr double SKLEARN_BIRD_LABEL = 0;

std::unordered_map<int, std::vector<std::vector<double>>> load_cifar(
    std::unordered_map<std::string, std::string> &config) {
  std::ifstream cifar_file(config.find(CONFIG_KEY_PATH)->second, std::ios::binary);
  std::istreambuf_iterator<char> cifar_data(cifar_file);
  std::unordered_map<int, std::vector<std::vector<double>>> vecs_map;
  for (int i = 0; i < 10000; i++) {
    int label = static_cast<int>(*cifar_data);
    cifar_data++;
    std::vector<uint8_t> cifar_byte_vec;
    cifar_byte_vec.reserve(3072);
    std::copy_n(cifar_data, 3072, std::back_inserter(cifar_byte_vec));
    cifar_data++;
    std::vector<double> cifar_double_vec(cifar_byte_vec.begin(), cifar_byte_vec.end());

    std::unordered_map<int, std::vector<std::vector<double>>>::iterator label_vecs = vecs_map.find(label);
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

void send_predictions(std::unordered_map<std::string, std::string> &config, QueryProcessor &qp,
                      std::unordered_map<int, std::vector<std::vector<double>>> &cifar_data,
                      std::shared_ptr<metrics::RatioCounter> accuracy_ratio) {
  std::vector<std::vector<double>> planes_vecs = cifar_data.find(0)->second;
  std::vector<std::vector<double>> birds_vecs = cifar_data.find(2)->second;

  int num_batches = std::stoi(config.find(CONFIG_KEY_NUM_BATCHES)->second);
  int batch_size = std::stoi(config.find(CONFIG_KEY_BATCH_SIZE)->second);
  long batch_delay_millis = static_cast<long>(std::stoi(config.find(CONFIG_KEY_BATCH_DELAY)->second));
  for (int j = 0; j < num_batches; j++) {
    std::vector<boost::future<Response>> futures;
    std::vector<int> binary_labels;
    for (int i = 0; i < batch_size; i++) {
      std::srand(time(NULL));
      int index = std::rand() % 2;
      std::vector<double> query_vec;
      if (index == 0) {
        // Send a plane
        size_t plane_index = std::rand() % planes_vecs.size();
        query_vec = planes_vecs[plane_index];
        binary_labels.emplace_back(SKLEARN_PLANE_LABEL);
      } else {
        // Send a bird
        size_t bird_index = std::rand() % birds_vecs.size();
        query_vec = birds_vecs[bird_index];
        binary_labels.emplace_back(SKLEARN_BIRD_LABEL);
      }

      std::shared_ptr<Input> cifar_input = std::make_shared<DoubleVector>(query_vec);
      boost::future<Response> future =
          qp.predict({"test", 0, cifar_input, 100000, "EXP3", {std::make_pair(SKLEARN_MODEL_NAME, 1)}});
      futures.push_back(std::move(future));
    }

    std::shared_ptr<std::atomic_int> completed = std::make_shared<std::atomic_int>(0);
    std::pair<boost::future<void>, std::vector<boost::future<Response>>> results =
        future::when_all(std::move(futures), completed);
    results.first.get();
    for (int i = 0; i < static_cast<int>(results.second.size()); i++) {
      boost::future<Response> &future = results.second[i];
      double label = static_cast<double>(binary_labels[i]);
      double pred = future.get().output_.y_hat_;
      if (pred == label) {
        accuracy_ratio->increment(1, 1);
      } else {
        accuracy_ratio->increment(0, 1);
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(batch_delay_millis));
  }
}

std::unordered_map<std::string, std::string> create_config(std::string path,
                                                           std::string num_threads,
                                                           std::string num_batches,
                                                           std::string batch_size,
                                                           std::string batch_delay) {
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

  std::cout << "Before proceeding, run bench/setup_bench.sh from clipper's root directory." << std::endl;
  std::cout << "Enter a path to the CIFAR10 binary data set: ";
  std::cin >> path;
  std::cout << "Enter the number of threads of execution: ";
  std::cin >> num_threads;
  std::cout << "Enter the number of request batches to be sent by each thread: ";
  std::cin >> num_batches;
  std::cout << "Enter the number of requests per batch: ";
  std::cin >> batch_size;
  std::cout << "Enter the delay between batches, in milliseconds: ";
  std::cin >> batch_delay;

  return create_config(path, num_threads, num_batches, batch_size, batch_delay);
};

std::unordered_map<std::string, std::string> get_config_from_json(std::string json_path) {
  std::ifstream json_file(json_path);
  std::stringstream buffer;
  buffer << json_file.rdbuf();
  std::string json_text = buffer.str();
  rapidjson::Document d;
  json::parse_json(json_text, d);
  std::string cifar_path = json::get_string(d, "cifar_data_path");
  std::string num_threads = json::get_string(d, "num_threads");
  std::string num_batches = json::get_string(d, "num_batches");
  std::string batch_size = json::get_string(d, "batch_size");
  std::string batch_delay = json::get_string(d, "batch_delay_millis");
  return create_config(cifar_path, num_threads, num_batches, batch_size, batch_delay);
};

int main(int argc, char *argv[]) {
  cxxopts::Options options("performance_bench",
                           "Clipper performance benchmarking");
  // clang-format off
  options.add_options()
      ("f,filename", "Config file name", cxxopts::value<std::string>());
  // clang-format on
  options.parse(argc, argv);
  bool json_specified = (options.count("filename") > 0);
  std::unordered_map<std::string, std::string> test_config;

  if(json_specified) {
    std::string json_path = options["filename"].as<std::string>();
    test_config = get_config_from_json(json_path);
  } else {
    test_config = get_config_from_prompt();
  }
  get_config().ready();
  QueryProcessor qp;
  std::this_thread::sleep_for(std::chrono::seconds(3));
  std::shared_ptr<metrics::RatioCounter> accuracy_ratio =
      metrics::MetricsRegistry::get_metrics().create_ratio_counter("accuracy");
  std::unordered_map<int, std::vector<std::vector<double>>> cifar_data = load_cifar(test_config);

  int num_threads = std::stoi(test_config.find(CONFIG_KEY_NUM_THREADS)->second);
  std::vector<std::thread> threads;
  for (int i = 0; i < num_threads; i++) {
    std::thread thread([&]() {
      send_predictions(test_config, qp, cifar_data, accuracy_ratio);
    });
    threads.push_back(std::move(thread));
  }
  for (auto &thread : threads) {
    thread.join();
  }
  std::string metrics = metrics::MetricsRegistry::get_metrics().report_metrics();
  log_info("BENCH", metrics);
}
