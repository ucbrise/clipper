#include <algorithm>
#include <ctime>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_real_distribution.hpp>
#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION
#define BOOST_THREAD_PROVIDES_FUTURE_WHEN_ALL_WHEN_ANY
#define PROVIDES_EXECUTORS
#include <boost/thread.hpp>
#include <boost/thread/executors/basic_thread_pool.hpp>

#include <clipper/concurrency.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/metrics.hpp>
#include <clipper/query_processor.hpp>

using namespace clipper;
using boost::future;
using std::vector;

constexpr int SLO_MICROS = 20000;

std::shared_ptr<DoubleVector> generate_rand_doublevec(
    int input_len, boost::random::mt19937& gen) {
  vector<double> input;
  boost::random::uniform_real_distribution<> dist(0.0, 1.0);
  for (int i = 0; i < input_len; ++i) {
    input.push_back(dist(gen));
  }
  return std::make_shared<DoubleVector>(input);
}

Query generate_query(int input_len, boost::random::mt19937& gen) {
  std::shared_ptr<Input> input = generate_rand_doublevec(input_len, gen);
  vector<VersionedModelId> models{std::make_pair("m", 1),
                                  std::make_pair("j", 1)};
  return Query{"bench", 3, input, SLO_MICROS, "simple_policy", models};
}

void run_benchmark(QueryProcessor& qp, int num_requests) {
  boost::random::mt19937 gen(std::time(0));
  vector<future<Response>> preds;
  auto start = std::chrono::high_resolution_clock::now();

  for (int req_num = 0; req_num < num_requests; ++req_num) {
    preds.push_back(qp.predict(generate_query(1000, gen)));
  }

  vector<long> durations;
  double completed_tasks_sum = 0.0;

  for (auto p = preds.begin(); p != preds.end(); ++p) {
    Response r{p->get()};
    durations.push_back(r.duration_micros_);
    completed_tasks_sum += r.output_.y_hat_;
  }
  auto end = std::chrono::high_resolution_clock::now();
  double benchmark_time_secs =
      metrics::get_duration_micros(end, start) / 1000.0 / 1000.0;

  double thruput = num_requests / benchmark_time_secs;

  double p99 = metrics::compute_percentile(durations, 0.99);
  // double p95 = metrics::compute_percentile(durations, 0.95);
  // double p50 = metrics::compute_percentile(durations, 0.50);
  double mean_lat = metrics::compute_mean(durations);
  std::cout << "Sent " << num_requests << " in " << benchmark_time_secs
            << " seconds" << std::endl;
  std::cout << "Throughput: " << thruput << std::endl;
  std::cout << "p99 latency (us): " << p99 << ", mean latency (us) " << mean_lat
            << std::endl;
  std::cout << "Mean tasks completed: "
            << completed_tasks_sum / (double)num_requests << std::endl;
}

void drive_benchmark() {
  QueryProcessor qp;
  std::this_thread::sleep_for(std::chrono::seconds(3));
  std::string line;
  std::cout << "Please enter number of requests to make:" << std::endl;
  while (std::getline(std::cin, line)) {
    try {
      int num_reqs = std::stoi(line);
      std::cout << "Running benchmark..." << std::endl;
      run_benchmark(qp, num_reqs);
      std::cout << std::endl;
    } catch (std::invalid_argument e) {
    }
    std::cout << "Please enter number of requests to make:" << std::endl;
  }
}

// std::vector<boost::future<Output>> schedule_predictions(
//     std::vector<PredictTask> tasks) {
//   std::vector<boost::future<Output>> output_futures;
//   for (const PredictTask& t : tasks) {
//     std::cout << "BBBBBBBBB: {" << t.model_.first << ", " << t.model_.second
//               << "}" << std::endl;
//     boost::promise<Output> p;
//     p.set_value(Output{1.0, t.model_});
//     output_futures.push_back(p.get_future());
//   }
//   return output_futures;
// }
//
// void debug_future_comp() {
//   boost::random::mt19937 gen(std::time(0));
//   for (int qid = 0; qid < 10000; ++qid) {
//     Query q = generate_query(1000, gen);
//     std::cout << "Query number: " << qid << std::endl;
//
//     std::vector<PredictTask> tasks;
//     // construct the task and put in the vector
//     for (auto v : q.candidate_models_) {
//       tasks.emplace_back(q.input_, v, 0.5, qid, q.latency_micros_);
//     }
//     for (const PredictTask& t : tasks) {
//       std::cout << "AAAAAAAAAAA: {" << t.model_.first << ", " <<
//       t.model_.second
//                 << "}" << std::endl;
//     }
//     auto task_futures = schedule_predictions(tasks);
//     for (const PredictTask& t : tasks) {
//       std::cout << "CCCCCCCCCCC: {" << t.model_.first << ", " <<
//       t.model_.second
//                 << "}" << std::endl;
//     }
//
//     // auto all_tasks_completed =
//     //     boost::when_all(task_futures.begin(), task_futures.end());
//     auto all_tasks_completed =
//         future_composition::when_all(std::move(task_futures));
//     all_tasks_completed.then([q, qid](auto result) {
//       result.get();
//       std::cout << "All tasks completed" << std::endl;
//     });
//   }
// }

int main() {
  drive_benchmark();
  // debug_future_comp();
  return 0;
}
