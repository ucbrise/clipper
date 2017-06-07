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

void send_predictions(
    std::unordered_map<std::string, std::string> &config, QueryProcessor &qp,
    std::unordered_map<int, std::vector<std::vector<double>>> &cifar_data,
    std::shared_ptr<metrics::RatioCounter> accuracy_ratio) {
  std::vector<std::vector<double>> planes_vecs = cifar_data.find(0)->second;
  std::vector<std::vector<double>> birds_vecs = cifar_data.find(2)->second;

  int num_batches = get_int(NUM_BATCHES, config);
  int batch_size = get_int(BATCH_SIZE, config);
  long batch_delay_millis = get_long(BATCH_DELAY_MILLIS, config);
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

  std::vector<std::string> desired_vars = {CIFAR_DATA_PATH, NUM_THREADS,
                                           NUM_BATCHES, BATCH_SIZE,
                                           BATCH_DELAY_MILLIS};

  if (json_specified) {
    std::string json_path = options["filename"].as<std::string>();
    test_config = get_config_from_json(json_path, desired_vars);
  } else {
    std::string setup_message =
        "Before proceeding, run bench/setup_sklearn_bench.sh from clipper's "
        "root "
        "directory";
    test_config = get_config_from_prompt(setup_message, desired_vars);
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
  int num_threads = get_int(NUM_THREADS, test_config);
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
