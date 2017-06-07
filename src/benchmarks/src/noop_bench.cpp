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
const std::string NOOP_MODEL_NAME = "bench_noop";

const int RANDOM_VEC_LEN = 784;
const int RANDOM_VEC_UPPERBOUND = 10;

std::vector<double> gen_random_vector(int length, int upper_bound) {
  std::vector<double> vec(length);
  for (size_t i = 0; i< vec.size(); i++) {
    vec[i] = std::rand() % upper_bound;
  }
  return vec;
}

void send_predictions(
    std::unordered_map<std::string, std::string> &config,
    QueryProcessor &qp) {

  int num_batches = std::stoi(config.find(CONFIG_KEY_NUM_BATCHES)->second);
  int batch_size = std::stoi(config.find(CONFIG_KEY_BATCH_SIZE)->second);
  long batch_delay_millis =
          static_cast<long>(std::stoi(config.find(CONFIG_KEY_BATCH_DELAY)->second));

  clipper::app_metrics::AppMetrics app_metrics(TEST_APPLICATION_LABEL);
  std::vector<double> query_vec;

  for (int j = 0; j < num_batches; j++) {
    // batch sizes of 1 for now
    query_vec = gen_random_vector(RANDOM_VEC_LEN, RANDOM_VEC_UPPERBOUND);

    std::shared_ptr<Input> input = std::make_shared<DoubleVector>(query_vec);
    boost::future<Response> prediction =
        qp.predict({TEST_APPLICATION_LABEL,
                    0,
                    input,
                    100000,
                    clipper::DefaultOutputSelectionPolicy::get_name(),
                    {std::make_pair(NOOP_MODEL_NAME, 1)}});

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

  std::unordered_map<std::string, std::string> test_config = bench_utils::get_config_from_prompt();

  get_config().ready();
  QueryProcessor qp;
  std::this_thread::sleep_for(std::chrono::seconds(3));

  clipper::DefaultOutputSelectionPolicy p;
  clipper::Output parsed_default_output(DEFAULT_OUTPUT, {});
  auto init_state = p.init_state(parsed_default_output);
  clipper::StateKey state_key{TEST_APPLICATION_LABEL, clipper::DEFAULT_USER_ID,
                              0};
  qp.get_state_table()->put(state_key, p.serialize(init_state));

  // Seed the random number generator that will be used to randomly generate datapoints
  std::srand(time(NULL));
  int num_threads = std::stoi(test_config.find(CONFIG_KEY_NUM_THREADS)->second);
  std::vector<std::thread> threads;
  for (int i = 0; i < num_threads; i++) {
    std::thread thread([&]() {
        send_predictions(test_config, qp);
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
