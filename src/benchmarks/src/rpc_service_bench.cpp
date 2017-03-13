#include <iostream>
#include <thread>
#include <unordered_map>

#include <clipper/config.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/logging.hpp>
#include <clipper/metrics.hpp>
#include <clipper/redis.hpp>
#include <clipper/rpc_service.hpp>
#include <clipper/util.hpp>

using namespace clipper;

const std::string LOGGING_TAG_RPC_BENCH = "RPCBENCH";

// taken from http://stackoverflow.com/a/12468109/814642
std::string gen_random_string(size_t length) {
  auto randchar = []() -> char {
    const char charset[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    const size_t max_index = (sizeof(charset) - 1);
    return charset[rand() % max_index];
  };
  std::string str(length, 0);
  std::generate_n(str.begin(), length, randchar);
  return str;
}

// void await_responses(std::shared_ptr<rpc::RPCService> rpc_service,
//                      std::shared_ptr<std::unordered_map<int, long>>
//                      times_map,
//                      bool &shutdown, long &total_time_elapsed) {
//   std::thread([&] {
//     while (true) {
//       if (shutdown) {
//         return;
//       }
//       long end = std::chrono::duration_cast<std::chrono::milliseconds>(
//                      std::chrono::system_clock::now().time_since_epoch())
//                      .count();
//       vector<rpc::RPCResponse> responses = rpc_service->try_get_responses(1);
//       if (responses.empty()) {
//         continue;
//       }
//       std::unordered_map<int, long>::const_iterator start_time =
//           times_map->find(responses.front().first);
//       if (start_time != times_map->end()) {
//         long elapsed = end - start_time->second;
//         total_time_elapsed += elapsed;
//         log_info_formatted(LOGGING_TAG_RPC_BENCH, "{} ms", (int)elapsed);
//       }
//     }
//   }).detach();
// }

// void send_request_repeated(
//     int container_id, int num_iterations, rpc::PredictionRequest &request,
//     std::shared_ptr<rpc::RPCService> rpc_service,
//     std::shared_ptr<std::unordered_map<int, long>> times_map) {
//   for (int i = 0; i < num_iterations; i++) {
//     long start = std::chrono::duration_cast<std::chrono::milliseconds>(
//                      std::chrono::system_clock::now().time_since_epoch())
//                      .count();
//     int id = rpc_service->send_message(request.serialize(), container_id);
//     times_map->emplace(id, start);
//     usleep(50000);
//   }
// }

template <typename T, class N>
std::vector<std::shared_ptr<Input>> get_primitive_inputs(
    int num_inputs, int input_len, InputType type, std::vector<T> data_vector,
    std::vector<std::shared_ptr<N>> input_vector) {
  input_vector.clear();
  std::vector<std::shared_ptr<Input>> generic_input_vector;
  for (int k = 0; k < num_inputs; ++k) {
    for (int j = 0; j < input_len; ++j) {
      if (type == InputType::Bytes) {
        uint8_t *bytes = reinterpret_cast<uint8_t *>(&j);
        for (int i = 0; i < (int)(sizeof(int) / sizeof(uint8_t)); i++) {
          data_vector.push_back(*(bytes + i));
        }
      } else {
        data_vector.push_back(static_cast<T>(j));
      }
    }
    std::shared_ptr<N> input = std::make_shared<N>(data_vector);
    generic_input_vector.push_back(std::dynamic_pointer_cast<Input>(input));
    data_vector.clear();
  }
  return generic_input_vector;
}

// void run_bytes_benchmark(
//     int container_id, int num_iterations,
//     std::shared_ptr<rpc::RPCService> rpc_service,
//     std::shared_ptr<std::unordered_map<int, long>> times_map) {
//   std::vector<uint8_t> type_vec;
//   std::vector<std::shared_ptr<ByteVector>> input_vec;
//   std::vector<std::shared_ptr<Input>> inputs =
//       get_primitive_inputs(InputType::Bytes, type_vec, input_vec);
//   rpc::PredictionRequest request(inputs, InputType::Bytes);
//   send_request_repeated(container_id, num_iterations, request, rpc_service,
//                         times_map);
// }
//
// void run_ints_benchmark(
//     int container_id, int num_iterations,
//     std::shared_ptr<rpc::RPCService> rpc_service,
//     std::shared_ptr<std::unordered_map<int, long>> times_map) {
//   std::vector<int> type_vec;
//   std::vector<std::shared_ptr<IntVector>> input_vec;
//   std::vector<std::shared_ptr<Input>> inputs =
//       get_primitive_inputs(InputType::Ints, type_vec, input_vec);
//   rpc::PredictionRequest request(inputs, InputType::Ints);
//   send_request_repeated(container_id, num_iterations, request, rpc_service,
//                         times_map);
// }
//
// void run_floats_benchmark(
//     int container_id, int num_iterations,
//     std::shared_ptr<rpc::RPCService> rpc_service,
//     std::shared_ptr<std::unordered_map<int, long>> times_map) {
//   std::vector<float> type_vec;
//   std::vector<std::shared_ptr<FloatVector>> input_vec;
//   std::vector<std::shared_ptr<Input>> inputs =
//       get_primitive_inputs(InputType::Floats, type_vec, input_vec);
//   rpc::PredictionRequest request(inputs, InputType::Floats);
//   send_request_repeated(container_id, num_iterations, request, rpc_service,
//                         times_map);
// }
//
// void run_doubles_benchmark(
//     int container_id, int num_iterations,
//     std::shared_ptr<rpc::RPCService> rpc_service,
//     std::shared_ptr<std::unordered_map<int, long>> times_map) {
//   std::vector<double> type_vec;
//   std::vector<std::shared_ptr<DoubleVector>> input_vec;
//   std::vector<std::shared_ptr<Input>> inputs =
//       get_primitive_inputs(InputType::Doubles, type_vec, input_vec);
//   rpc::PredictionRequest request(inputs, InputType::Doubles);
//   send_request_repeated(container_id, num_iterations, request, rpc_service,
//                         times_map);
// }
//
// void run_strings_benchmark(
//     int container_id, int num_iterations,
//     std::shared_ptr<rpc::RPCService> rpc_service,
//     std::shared_ptr<std::unordered_map<int, long>> times_map) {
//   rpc::PredictionRequest request(InputType::Strings);
//   for (int num_inputs = 0; num_inputs < 500; ++num_inputs) {
//     std::string str = random_string(150);
//     if ((num_inputs % 3) == 0) {
//       str = std::string("CAT");
//     } else if ((num_inputs % 3) == 1) {
//       str = std::string("DOG");
//     } else {
//       str = std::string("COW");
//     }
//     std::shared_ptr<SerializableString> input =
//         std::make_shared<SerializableString>(str);
//     request.add_input(input);
//   }
//   send_request_repeated(container_id, num_iterations, request, rpc_service,
//                         times_map);
// }

rpc::PredictionRequest generate_bytes_request(int num_inputs) {
  std::vector<uint8_t> type_vec;
  std::vector<std::shared_ptr<ByteVector>> input_vec;
  std::vector<std::shared_ptr<Input>> inputs = get_primitive_inputs(
      num_inputs, 784, InputType::Bytes, type_vec, input_vec);
  rpc::PredictionRequest request(inputs, InputType::Bytes);
  return request;
}

rpc::PredictionRequest generate_floats_request(int num_inputs) {
  std::vector<float> type_vec;
  std::vector<std::shared_ptr<FloatVector>> input_vec;
  std::vector<std::shared_ptr<Input>> inputs = get_primitive_inputs(
      num_inputs, 784, InputType::Floats, type_vec, input_vec);
  rpc::PredictionRequest request(inputs, InputType::Floats);
  return request;
}

rpc::PredictionRequest generate_ints_request(int num_inputs) {
  std::vector<int> type_vec;
  std::vector<std::shared_ptr<IntVector>> input_vec;
  std::vector<std::shared_ptr<Input>> inputs = get_primitive_inputs(
      num_inputs, 784, InputType::Ints, type_vec, input_vec);
  rpc::PredictionRequest request(inputs, InputType::Ints);
  return request;
}

rpc::PredictionRequest generate_doubles_request(int num_inputs) {
  std::vector<double> type_vec;
  std::vector<std::shared_ptr<DoubleVector>> input_vec;
  std::vector<std::shared_ptr<Input>> inputs = get_primitive_inputs(
      num_inputs, 784, InputType::Doubles, type_vec, input_vec);
  rpc::PredictionRequest request(inputs, InputType::Doubles);
  return request;
}

rpc::PredictionRequest generate_string_request(int num_inputs) {
  rpc::PredictionRequest request(InputType::Strings);
  for (int i = 0; i < num_inputs; ++i) {
    std::string str = gen_random_string(150);
    std::shared_ptr<SerializableString> input =
        std::make_shared<SerializableString>(str);
    request.add_input(input);
  }
  return request;
}

// void run_benchmarks() {
//   std::shared_ptr<rpc::RPCService> rpc_service =
//       std::make_shared<rpc::RPCService>();
//   std::shared_ptr<std::unordered_map<int, long>> times_map =
//       std::make_shared<std::unordered_map<int, long>>();
//   bool shutdown = false;
//   long total_time_elapsed = 0;
//   int num_iterations_per_benchmark = 300;
//   int num_benchmarks = 1;
//
//   rpc_service->start("127.0.0.1", 8000);
//   usleep(5000000);
//
//   await_responses(rpc_service, times_map, shutdown, total_time_elapsed);
//   // run_bytes_benchmark(0, num_iterations_per_benchmark, rpc_service,
//   // times_map);
//   // run_ints_benchmark(0, num_iterations_per_benchmark, rpc_service,
//   // times_map);
//   // run_floats_benchmark(0, num_iterations_per_benchmark, rpc_service,
//   // times_map);
//   run_doubles_benchmark(0, num_iterations_per_benchmark, rpc_service,
//                         times_map);
//   // run_strings_benchmark(0, num_iterations_per_benchmark, rpc_service,
//   //                       times_map);
//
//   shutdown = true;
//   rpc_service->stop();
//   double time_per_iter = ((double)total_time_elapsed) /
//                          (num_iterations_per_benchmark * num_benchmarks);
//   log_info_formatted(LOGGING_TAG_RPC_BENCH, "{} ms", time_per_iter);
// }

rpc::PredictionRequest create_request(InputType input_type) {
  int num_inputs = 500;
  switch (input_type) {
    case InputType::Strings: return generate_string_request(num_inputs);
    case InputType::Doubles: return generate_doubles_request(num_inputs);
    case InputType::Floats: return generate_floats_request(num_inputs);
    case InputType::Bytes: return generate_bytes_request(num_inputs);
    case InputType::Ints: return generate_ints_request(num_inputs);
  }
  throw std::invalid_argument("Unsupported input type");
}

class Benchmarker {
 public:
  Benchmarker(int num_iterations, InputType input_type)
      : num_iterations_(num_iterations), request_(create_request(input_type)) {}

  void start() {
    rpc_->start("*", RPC_SERVICE_PORT, [this](VersionedModelId, int) {},
                [this](rpc::RPCResponse response) {
                  on_response_recv(std::move(response));
                });

    msg_latency_hist_ =
        metrics::MetricsRegistry::get_metrics().create_histogram(
            "rpc_bench_msg_latency", "microseconds", 8260);
    throughput_meter_ = metrics::MetricsRegistry::get_metrics().create_meter(
        "rpc_bench_throughput");

    Config &conf = get_config();
    while (!redis_connection_.connect(conf.get_redis_address(),
                                      conf.get_redis_port())) {
      log_error(LOGGING_TAG_RPC_BENCH, "RPCBench failed to connect to redis",
                "Retrying in 1 second...");
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    while (!redis_subscriber_.connect(conf.get_redis_address(),
                                      conf.get_redis_port())) {
      log_error(LOGGING_TAG_RPC_BENCH,
                "RPCBench subscriber failed to connect to redis",
                "Retrying in 1 second...");
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    redis::send_cmd_no_reply<std::string>(
        redis_connection_, {"CONFIG", "SET", "notify-keyspace-events", "AKE"});
    redis::subscribe_to_container_changes(
        redis_subscriber_,
        // event_type corresponds to one of the Redis event types
        // documented in https://redis.io/topics/notifications.
        [this](const std::string &key, const std::string &event_type) {
          if (event_type == "hset") {
            auto container_info =
                redis::get_container_by_key(redis_connection_, key);
            benchmark_container_id_ =
                std::stoi(container_info["zmq_connection_id"]);

            // SEND FIRST MESSAGE
            send_message();
          }

        });
  }

  Benchmarker(const Benchmarker &other) = delete;
  Benchmarker &operator=(const Benchmarker &other) = delete;

  Benchmarker(Benchmarker &&other) = default;
  Benchmarker &operator=(Benchmarker &&other) = default;

  // void start(int container_id) {
  //   switch (input_type_) {
  //     case InputType::Strings: run_strings_benchmarks(container_id); break;
  //     case InputType::Doubles: run_doubles_benchmarks(container_id); break;
  //     case InputType::Floats: run_floats_benchmarks(container_id); break;
  //     case InputType::Bytes: run_bytes_benchmarks(container_id); break;
  //     case InputType::Ints: run_ints_benchmarks(container_id); break;
  //   }
  // }

  // void on_container_ready(VersionedModelId, int) {
  //   // std::unique_lock<std::mutex> l(inflight_messages_mutex_);
  //   int id = rpc_->send_message(request.serialize(), container_id);
  //   inflight_message_start_times_->emplace(id, start);
  //   long current_time =
  //   std::chrono::duration_cast<std::chrono::milliseconds>(
  //                           std::chrono::system_clock::now().time_since_epoch())
  //                           .count();
  // }

  void send_message() {
    cur_msg_start_time_micros_ =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
    cur_message_id_ =
        rpc_->send_message(request_.serialize(), benchmark_container_id_);
  }

  void on_response_recv(rpc::RPCResponse response) {
    // process the response
    long recv_time_micros =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
    if (response.first != cur_message_id_) {
      throw std::logic_error(
          "Response message ID did not match in flight message ID");
    }
    long message_duration_micros =
        recv_time_micros - cur_msg_start_time_micros_;
    msg_latency_hist_->insert(message_duration_micros);
    throughput_meter_->mark(1);
    messages_completed_ += 1;

    if (messages_completed_ < num_iterations_) {
      // send the new message
      send_message();
    } else {
      bench_completed_ = true;
      bench_completed_cv_.notify_all();
    }
  }

  std::condition_variable_any bench_completed_cv_;
  std::mutex bench_completed_cv_mutex;
  std::atomic<bool> bench_completed_{false};

 private:
  std::shared_ptr<metrics::Histogram> msg_latency_hist_;
  std::shared_ptr<metrics::Meter> throughput_meter_;
  int num_iterations_;
  redox::Redox redis_connection_;
  redox::Subscriber redis_subscriber_;
  std::atomic<int> messages_completed_;
  std::unique_ptr<rpc::RPCService> rpc_;
  long cur_msg_start_time_micros_;
  int benchmark_container_id_;
  rpc::PredictionRequest request_;
  int cur_message_id_;
};

int main(int argc, char *argv[]) {
  Benchmarker benchmarker(10, InputType::Doubles);
  auto jh = std::thread([&benchmarker] { benchmarker.start(); });
  std::unique_lock<std::mutex> l(benchmarker.bench_completed_cv_mutex);
  benchmarker.bench_completed_cv_.wait(
      l, [&benchmarker]() { return benchmarker.bench_completed_ == true; });

  metrics::MetricsRegistry &registry = metrics::MetricsRegistry::get_metrics();
  std::string metrics_report = registry.report_metrics();
  log_info(LOGGING_TAG_RPC_BENCH, "METRICS", metrics_report);
  return 0;
}
