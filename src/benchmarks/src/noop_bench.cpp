#include <time.h>
#include <functional>
#include <iostream>
#include <vector>

#include <boost/thread.hpp>
#include <cxxopts.hpp>

#include <clipper/app_metrics.hpp>
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

const std::string DEFAULT_OUTPUT = "-1";
const std::string TEST_APPLICATION_LABEL = "throughput_testing_app";

// These should match the values in setup_throughput_bench.sh
const std::string MODEL_NAME = "bench_noop";
const int MODEL_VERISON = 1;

void send_predictions(std::unordered_map<std::string, std::string> &config,
                      QueryProcessor &qp,
                      std::vector<std::vector<double>> data) {
  int num_batches = std::stoi(config.find(CONFIG_KEY_NUM_BATCHES)->second);
  long batch_delay_millis =
      static_cast<long>(std::stoi(config.find(CONFIG_KEY_BATCH_DELAY)->second));

  int num_datapoints;
  num_datapoints = static_cast<int>(data.size());  // assume that data.size()
                                                   // doesn't exceed int
                                                   // representation bounds

  clipper::app_metrics::AppMetrics app_metrics(TEST_APPLICATION_LABEL);
  std::vector<double> query_vec;

  for (int j = 0; j < num_batches; j++) {
    // Select datapoint and modify it to be epoch-specific (to avoid cache hits)
    // Will modify when batch sizes are allowed to be > 1
    int index = j % num_datapoints;
    query_vec = data[index];
    query_vec[0] += j / num_datapoints;
    std::shared_ptr<Input> input = std::make_shared<DoubleVector>(query_vec);

    boost::future<Response> prediction =
        qp.predict({TEST_APPLICATION_LABEL,
                    0,
                    input,
                    100000,
                    clipper::DefaultOutputSelectionPolicy::get_name(),
                    {std::make_pair(MODEL_NAME, MODEL_VERISON)}});

    prediction.then([app_metrics](boost::future<Response> f) {
      Response r = f.get();

      // Update metrics
      if (r.output_is_default_) {
        app_metrics.default_pred_ratio_->increment(1, 1);
      } else {
        app_metrics.default_pred_ratio_->increment(0, 1);
      }
      app_metrics.latency_->insert(r.duration_micros_);
      app_metrics.num_predictions_->increment(1);
      app_metrics.throughput_->mark(1);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(batch_delay_millis));
  }
}

int main(int argc, char *argv[]) {
  cxxopts::Options options("noop_bench",
                           "Clipper noop performance benchmarking");
  // clang-format off
  options.add_options()
          ("f,filename", "Config file name", cxxopts::value<std::string>());
  // clang-format on
  options.parse(argc, argv);
  bool json_specified = (options.count("filename") > 0);
  std::unordered_map<std::string, std::string> test_config;

  if (json_specified) {
    std::string json_path = options["filename"].as<std::string>();
    test_config = get_cifar_config_from_json(json_path);
  } else {
    std::string setup_message =
        "Before proceeding, run bench/setup_noop_bench.sh from clipper's root "
        "directory";
    test_config = get_cifar_config_from_prompt(setup_message);
  }
  get_config().ready();
  QueryProcessor qp;
  std::this_thread::sleep_for(std::chrono::seconds(3));

  clipper::DefaultOutputSelectionPolicy p;
  clipper::Output parsed_default_output(DEFAULT_OUTPUT, {});
  auto init_state = p.init_state(parsed_default_output);
  clipper::StateKey state_key{TEST_APPLICATION_LABEL, clipper::DEFAULT_USER_ID,
                              0};
  qp.get_state_table()->put(state_key, p.serialize(init_state));

  // Seed the random number generator that will be used to randomly generate
  // datapoints
  std::srand(time(NULL));

  // Prepare data for threads.
  // We concatenate because we only need the datapoints – not their labels.
  // We modify datapoints to be thread-specific to avoid cache hits
  std::unordered_map<int, std::vector<std::vector<double>>> cifar_data =
      load_cifar(test_config);
  std::vector<std::vector<double>> concatendated_datapoints =
      concatenate_cifar_datapoints(cifar_data);

  send_predictions(test_config, qp, concatendated_datapoints);

  std::string metrics =
      metrics::MetricsRegistry::get_metrics().report_metrics();
  log_info("BENCH", metrics);
}
