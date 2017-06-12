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
const int UID = 0;
const std::string REPORT_DELIMTER = ":";

// These should match the values in setup_throughput_bench.sh
const std::string MODEL_NAME = "bench_noop";
const int MODEL_VERISON = 1;

void send_predictions(std::unordered_map<std::string, std::string> &config,
                      QueryProcessor &qp, std::vector<std::vector<double>> data,
                      clipper::app_metrics::AppMetrics &app_metrics) {
  int num_batches = std::stoi(config.find(NUM_BATCHES)->second);
  long batch_delay_micros =
      static_cast<long>(std::stoi(config.find(BATCH_DELAY_MICROS)->second));
  int latency_objective = std::stoi(config.find(LATENCY_OBJECTIVE)->second);

  int num_datapoints;
  num_datapoints = static_cast<int>(data.size());  // assume that data.size()
                                                   // doesn't exceed int
                                                   // representation bounds

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
                    UID,
                    input,
                    latency_objective,
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

    std::this_thread::sleep_for(std::chrono::microseconds(batch_delay_micros));
  }
}

void report_and_clear_metrics(
    std::unordered_map<std::string, std::string> &config,
    clipper::app_metrics::AppMetrics &app_metrics) {
  bool clear = true;
  int report_delay_seconds =
      std::stoi(config.find(REPORT_DELAY_SECONDS)->second);
  int num_iters = 1;

  // Set up output files
  std::ofstream out(config.find(REPORTS_PATH)->second);
  std::ofstream out_verbose(config.find(REPORTS_PATH_VERBOSE)->second);

  // Write out run details
  std::string latency_obj_string = config.find(LATENCY_OBJECTIVE)->second;
  std::string batch_delay_string = config.find(BATCH_DELAY_MILLIS)->second;
  std::stringstream ss;
  ss << "Hyperparams in this noop_bench run: ";
  ss << "Latency (ms): " << latency_obj_string;
  ss << ", Batch delay (ms): " << batch_delay_string;
  ss << std::endl;
  out_verbose << ss.str();
  out << batch_delay_string << REPORT_DELIMTER << latency_obj_string
      << std::endl;

  int window_lower;
  int window_upper;
  std::string window;
  double throughput;
  double p99_latency;

  // This will get interrupted
  while (true) {
    // Wait for reports to accumulate
    std::this_thread::sleep_for(std::chrono::seconds(report_delay_seconds));

    // Collect and write out specific datapoints
    throughput = app_metrics.throughput_->get_rate_seconds();
    p99_latency = app_metrics.latency_->get_p99();
    out << throughput << REPORT_DELIMTER << p99_latency << std::endl;
    out.flush();

    // Timestamp stuff
    window_lower = report_delay_seconds * (num_iters - 1);
    window_upper = report_delay_seconds * num_iters;
    window = std::to_string(window_lower) + "s – " +
             std::to_string(window_upper) + "s";
    num_iters += 1;

    // Log and write out verbose metrics
    std::string metrics =
        metrics::MetricsRegistry::get_metrics().report_metrics(clear);
    log_info("WINDOW", window);
    log_info("METRICS", metrics);

    out_verbose << window << ": " << metrics;
    out_verbose.flush();
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

  // Need to update this when we allow for batch sizes > 1
  std::vector<std::string> desired_vars = {
      CIFAR_DATA_PATH,     NUM_BATCHES,          BATCH_DELAY_MICROS,
      LATENCY_OBJECTIVE,   REPORT_DELAY_SECONDS, REPORTS_PATH,
      REPORTS_PATH_VERBOSE};
  if (json_specified) {
    std::string json_path = options["filename"].as<std::string>();
    test_config = get_config_from_json(json_path, desired_vars);
  } else {
    std::string setup_message =
        "Before proceeding, run bench/setup_noop_bench.sh from clipper's root "
        "directory";
    test_config = get_config_from_prompt(setup_message, desired_vars);
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

  clipper::app_metrics::AppMetrics app_metrics(TEST_APPLICATION_LABEL);
  std::thread prediction_thread([&]() {
    send_predictions(test_config, qp, concatendated_datapoints, app_metrics);
  });

  std::thread metrics_thread(
      [&]() { report_and_clear_metrics(test_config, app_metrics); });

  prediction_thread.join();

  // final report
  std::string metrics =
      metrics::MetricsRegistry::get_metrics().report_metrics();
  log_info("BENCH", metrics);

  log_info("BENCH", "Terminating benchmarking script");

  // Kills all threads. We don't care about the last report anyway.
  std::terminate();
}
