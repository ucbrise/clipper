#include <time.h>
#include <functional>
#include <iostream>
#include <vector>

#include <boost/thread.hpp>
#include <cxxopts.hpp>

#include <clipper/constants.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/future.hpp>
#include <clipper/json_util.hpp>
#include <clipper/logging.hpp>
#include <clipper/query_processor.hpp>
#include <fstream>

#include "include/bench_utils.hpp"

using namespace clipper;
using namespace bench_utils;

const std::string SKLEARN_MODEL_NAME = "bench_sklearn_cifar";

constexpr double SKLEARN_PLANE_LABEL = 1;
constexpr double SKLEARN_BIRD_LABEL = 0;

const std::string TEST_APPLICATION_LABEL = "test";

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

void send_predictions(
    std::unordered_map<std::string, std::string> &config, QueryProcessor &qp,
    std::unordered_map<int, std::vector<std::vector<double>>> &cifar_data,
    std::shared_ptr<metrics::RatioCounter> accuracy_ratio) {
  std::vector<std::vector<double>> planes_vecs = cifar_data.find(0)->second;
  std::vector<std::vector<double>> birds_vecs = cifar_data.find(2)->second;

  int num_batches = std::stoi(config.find(CONFIG_KEY_NUM_BATCHES)->second);
  int batch_size = std::stoi(config.find(CONFIG_KEY_BATCH_SIZE)->second);
  long batch_delay_millis =
      static_cast<long>(std::stoi(config.find(CONFIG_KEY_BATCH_DELAY)->second));
  for (int j = 0; j < num_batches; j++) {
    std::vector<boost::future<Response>> futures;
    std::vector<int> binary_labels;
    for (int i = 0; i < batch_size; i++) {
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

      std::shared_ptr<Input> cifar_input =
          std::make_shared<DoubleVector>(query_vec);
      boost::future<Response> future =
          qp.predict({TEST_APPLICATION_LABEL,
                      0,
                      cifar_input,
                      100000,
                      clipper::DefaultOutputSelectionPolicy::get_name(),
                      {VersionedModelId(SKLEARN_MODEL_NAME, "1")}});
      futures.push_back(std::move(future));
    }

    std::shared_ptr<std::atomic_int> completed =
        std::make_shared<std::atomic_int>(0);
    std::pair<boost::future<void>, std::vector<boost::future<Response>>>
        results = future::when_all(std::move(futures), completed);
    results.first.get();
    for (int i = 0; i < static_cast<int>(results.second.size()); i++) {
      boost::future<Response> &future = results.second[i];
      double label = static_cast<double>(binary_labels[i]);
      double pred = std::stod(future.get().output_.y_hat_);
      if (pred == label) {
        accuracy_ratio->increment(1, 1);
      } else {
        accuracy_ratio->increment(0, 1);
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(batch_delay_millis));
  }
}

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

  if (json_specified) {
    std::string json_path = options["filename"].as<std::string>();
    test_config = get_config_from_json(json_path);
  } else {
    test_config = get_config_from_prompt();
  }
  get_config().ready();
  QueryProcessor qp;
  std::this_thread::sleep_for(std::chrono::seconds(3));

  clipper::DefaultOutputSelectionPolicy p;
  clipper::Output parsed_default_output("0", {});
  auto init_state = p.init_state(parsed_default_output);
  clipper::StateKey state_key{TEST_APPLICATION_LABEL, clipper::DEFAULT_USER_ID,
                              0};
  qp.get_state_table()->put(state_key, p.serialize(init_state));
  std::shared_ptr<metrics::RatioCounter> accuracy_ratio =
      metrics::MetricsRegistry::get_metrics().create_ratio_counter("accuracy");
  std::unordered_map<int, std::vector<std::vector<double>>> cifar_data =
      load_cifar(test_config);

  // Seed the random number generator that will be used to randomly select
  // query input vectors from the CIFAR dataset
  std::srand(time(NULL));
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
  std::string metrics =
      metrics::MetricsRegistry::get_metrics().report_metrics();
  log_info("BENCH", metrics);
}
