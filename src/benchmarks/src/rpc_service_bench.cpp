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
#include <cxxopts.hpp>

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

std::string get_thread_id() {
  std::stringstream ss;
  ss << std::this_thread::get_id();
  return ss.str();
}

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
  Benchmarker(int num_messages, InputType input_type)
      : num_messages_(num_messages),
        rpc_(std::make_unique<rpc::RPCService>()),
        request_(create_request(input_type)) {}

  void start() {
    rpc_->start("*", RPC_SERVICE_PORT, [](VersionedModelId, int) {},
                [this](rpc::RPCResponse response) {
                  on_response_recv(std::move(response));
                });

    msg_latency_hist_ =
        metrics::MetricsRegistry::get_metrics().create_histogram(
            "rpc_bench_msg_latency", "milliseconds", 8260);
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
  ~Benchmarker() {
    std::unique_lock<std::mutex> l(bench_completed_cv_mutex);
    bench_completed_cv_.wait(l, [this]() { return bench_completed_ == true; });
  }

  void send_message() {
    cur_msg_start_time_millis_ =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
    cur_message_id_ =
        rpc_->send_message(request_.serialize(), benchmark_container_id_);
  }

  void on_response_recv(rpc::RPCResponse response) {
    // process the response
    long recv_time_millis =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
    if (response.first != cur_message_id_) {
      throw std::logic_error(
          "Response message ID did not match in flight message ID");
    }
    long message_duration_millis =
        recv_time_millis - cur_msg_start_time_millis_;
    msg_latency_hist_->insert(message_duration_millis);
    throughput_meter_->mark(1);
    messages_completed_ += 1;

    if (messages_completed_ < num_messages_) {
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
  int num_messages_;
  redox::Redox redis_connection_;
  redox::Subscriber redis_subscriber_;
  std::atomic<int> messages_completed_{0};
  std::unique_ptr<rpc::RPCService> rpc_;
  std::atomic<long> cur_msg_start_time_millis_;
  std::atomic<int> benchmark_container_id_;
  rpc::PredictionRequest request_;
  int cur_message_id_;
};

int main(int argc, char *argv[]) {
  cxxopts::Options options("rpc_bench", "Clipper RPC Benchmark");
  // clang-format off
  options.add_options()
    ("redis_ip", "Redis address",
        cxxopts::value<std::string>()->default_value("localhost"))
    ("redis_port", "Redis port",
        cxxopts::value<int>()->default_value("6379"))
    ("m,num_messages", "Number of messages to send",
        cxxopts::value<int>()->default_value("100"));
  // clang-format on
  options.parse(argc, argv);

  clipper::Config &conf = clipper::get_config();
  conf.set_redis_address(options["redis_ip"].as<std::string>());
  conf.set_redis_port(options["redis_port"].as<int>());
  conf.ready();
  Benchmarker benchmarker(options["num_messages"].as<int>(),
                          InputType::Doubles);
  benchmarker.start();

  // auto jh = std::thread([&benchmarker]() { benchmarker.start(); });
  std::unique_lock<std::mutex> l(benchmarker.bench_completed_cv_mutex);
  benchmarker.bench_completed_cv_.wait(
      l, [&benchmarker]() { return benchmarker.bench_completed_ == true; });
  // jh.join();
  metrics::MetricsRegistry &registry = metrics::MetricsRegistry::get_metrics();
  std::string metrics_report = registry.report_metrics();
  log_info(LOGGING_TAG_RPC_BENCH, "METRICS", metrics_report);
  return 0;
}
