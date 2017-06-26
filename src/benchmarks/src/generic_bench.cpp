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
#include <random>

#include "include/bench_utils.hpp"

using namespace clipper;
using namespace bench_utils;

const std::string DEFAULT_OUTPUT = "-1";
const std::string TEST_APPLICATION_LABEL = "cifar_bench";
const int UID = 0;
const std::string REPORT_DELIMITER = ":";
const double P99_PERCENT = 0.99;
const std::string DELAY_MESSAGE_POISSON =
        "Delays between batches drawn from poisson distribution";
const std::string DELAY_MESSAGE_UNIFORM = "Uniform delays between batches";

std::string _get_window_str(int window_size, int num_iters) {
  int window_lower = window_size * (num_iters - 1);
  int window_upper = window_size * num_iters;
  std::stringstream ss;
  ss << std::to_string(window_lower) << "s - " << std::to_string(window_upper)
     << "s";
  return ss.str();
}

void send_predictions(std::unordered_map<std::string, std::string> &config,
                      QueryProcessor &qp, std::vector<std::vector<double>> data,
                      clipper::app_metrics::AppMetrics &app_metrics) {
  int num_batches = get_int(NUM_BATCHES, config);
  long batch_delay_micros = get_long(BATCH_DELAY_MICROS, config);
  int latency_objective = get_int(LATENCY_OBJECTIVE, config);
  std::string model_name = get_str(MODEL_NAME, config);
  std::string model_version = get_str(MODEL_VERSION, config);
  bool draw_from_poisson = get_bool(POISSON_DELAY, config);

  int num_datapoints = static_cast<int>(data.size());
  std::vector<double> query_vec;
  std::default_random_engine generator;
  std::poisson_distribution<int> distribution(batch_delay_micros);
  long delay_micros;
  int data_index;

  for (int j = 0; j < num_batches; j++) {
    // Select datapoint and modify it to be epoch-specific (to avoid cache hits)
    data_index = j % num_datapoints;
    query_vec = data[data_index];
    query_vec[0] += j / num_datapoints;
    std::shared_ptr<Input> input = std::make_shared<DoubleVector>(query_vec);

    boost::future<Response> prediction =
            qp.predict({TEST_APPLICATION_LABEL,
                        UID,
                        input,
                        latency_objective,
                        clipper::DefaultOutputSelectionPolicy::get_name(),
                        {VersionedModelId(model_name, model_version)}});

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

    delay_micros =
            draw_from_poisson ? distribution(generator) : batch_delay_micros;
    std::this_thread::sleep_for(std::chrono::microseconds(delay_micros));
  }
}

void report_and_clear_metrics(
        std::unordered_map<std::string, std::string> &config,
        clipper::app_metrics::AppMetrics &app_metrics) {
  int report_delay_seconds = get_int(REPORT_DELAY_SECONDS, config);
  bool draw_from_poisson = get_bool(POISSON_DELAY, config);
  std::string latency_obj_string = get_str(LATENCY_OBJECTIVE, config);
  std::string batch_delay_string = get_str(BATCH_DELAY_MICROS, config);
  std::string reports_path = get_str(REPORTS_PATH, config);
  std::string reports_path_verbose = get_str(REPORTS_PATH_VERBOSE, config);

  // Write out run details
  std::ofstream out(reports_path);
  std::ofstream out_verbose(reports_path_verbose);
  std::string delay_message =
          draw_from_poisson ? DELAY_MESSAGE_POISSON : DELAY_MESSAGE_UNIFORM;

  std::stringstream ss;
  ss << "Hyperparams: Latency (us): " << latency_obj_string
     << ", Batch delay (us): " << batch_delay_string << "." << delay_message
     << std::endl;
  std::string final_message = ss.str();
  out_verbose << final_message;
  out << batch_delay_string << REPORT_DELIMITER << latency_obj_string
      << REPORT_DELIMITER << draw_from_poisson << std::endl;
  log_info("BENCH", final_message);

  std::string window, metrics;
  double throughput, p99_latency;
  int i = 1;

  // This will get interrupted
  while (true) {
    // Wait for reports to accumulate
    std::this_thread::sleep_for(std::chrono::seconds(report_delay_seconds));

    throughput = app_metrics.throughput_->get_rate_seconds();
    p99_latency = app_metrics.latency_->percentile(P99_PERCENT);
    out << throughput << REPORT_DELIMITER << p99_latency << std::endl;
    out.flush();

    metrics = metrics::MetricsRegistry::get_metrics().report_metrics(true);
    window = _get_window_str(report_delay_seconds, i);

    out_verbose << window << ": " << metrics;
    out_verbose.flush();

    log_info("WINDOW", window);
    log_info("METRICS", metrics);
    i += 1;
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

  std::vector<std::string> desired_vars = {
          CIFAR_DATA_PATH,      NUM_BATCHES,          BATCH_DELAY_MICROS,
          LATENCY_OBJECTIVE,    REPORT_DELAY_SECONDS, REPORTS_PATH,
          REPORTS_PATH_VERBOSE, POISSON_DELAY,        MODEL_NAME,
          MODEL_VERSION};
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

  // We only need the datapoints – not their labels.
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

  // Final report
  std::string metrics =
          metrics::MetricsRegistry::get_metrics().report_metrics();
  log_info("BENCH", metrics);
  log_info("BENCH", "Terminating benchmarking script");

  // Kills all threads. We don't care about the last report anyway.
  std::terminate();
}
