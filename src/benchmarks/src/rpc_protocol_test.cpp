#include <cxxopts.hpp>
#include <redox.hpp>

#include <chrono>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <vector>

#include <clipper/config.hpp>
#include <clipper/constants.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/json_util.hpp>
#include <clipper/logging.hpp>
#include <clipper/memory.hpp>
#include <clipper/redis.hpp>
#include <clipper/rpc_service.hpp>

using namespace clipper;

const std::string LOGGING_TAG_RPC_TEST = "RPCTEST";

// From the container's perspective, we expect at least the following activity:
// 1. Send a heartbeat message to Clipper
// 2. Receive a heartbeat response from Clipper requesting container metadata
// 3. Send container metadata to Clipper
// 4. Receive a container content message from Clipper
constexpr int EVENT_HISTORY_MINIMUM_LENGTH = 4;

using RPCValidationResult = std::pair<bool, std::string>;

class Tester {
 public:
  explicit Tester(const int num_containers)
      : rpc_(std::make_unique<rpc::RPCService>()),
        num_containers_(num_containers) {}

  Tester(const Tester &other) = delete;
  Tester &operator=(const Tester &other) = delete;

  Tester(Tester &&other) = default;
  Tester &operator=(Tester &&other) = default;
  ~Tester() {
    stop_timeout_thread();
    std::unique_lock<std::mutex> l(test_completed_cv_mutex_);
    test_completed_cv_.wait(l, [this]() { return test_completed_ == true; });
  }

  void start(long timeout_seconds) {
    Config &conf = get_config();
    rpc_->start("127.0.0.1", conf.get_rpc_service_port(),
                [](VersionedModelId /*model*/, int /*container_id*/) {},
                [this](rpc::RPCResponse &response) {
                  on_response_received(std::move(response));
                },
                [](VersionedModelId, int) {});
    while (!redis_connection_.connect(conf.get_redis_address(),
                                      conf.get_redis_port())) {
      log_error(LOGGING_TAG_RPC_TEST, "RPCTest failed to connect to redis",
                "Retrying in 1 second...");
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    while (!redis_subscriber_.connect(conf.get_redis_address(),
                                      conf.get_redis_port())) {
      log_error(LOGGING_TAG_RPC_TEST,
                "RPCBench subscriber failed to connect to redis",
                "Retrying in 1 second...");
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    redis::send_cmd_no_reply<std::string>(
        redis_connection_, {"CONFIG", "SET", "notify-keyspace-events", "AKE"});
    redis::subscribe_to_container_changes(
        redis_subscriber_,
        [this](const std::string &key, const std::string &event_type) {
          if (event_type == "hset") {
            // Detected a new container
            if (containers_connected_ < num_containers_) {
              containers_connected_++;
              auto container_info =
                  redis::get_container_by_key(redis_connection_, key);
              int container_id = std::stoi(container_info["zmq_connection_id"]);
              std::string model_name = container_info["model_name"];
              // Add a validation entry for the new connected container
              // indicating that it has not yet been validated
              std::unique_lock<std::mutex> lock(container_maps_mutex);
              container_names_map_.emplace(container_id, model_name);
              const int validation_msg_id =
                  send_validation_message(container_id);
              msg_id_to_container_map_.emplace(validation_msg_id, container_id);
            }
          }
        });
    start_timeout_thread(timeout_seconds);
  }

  bool succeeded() {
    std::unique_lock<std::mutex> lock(container_maps_mutex);
    int valid_containers = 0;
    for (auto const &container_entry : container_validation_map_) {
      bool rpc_valid = container_entry.second.first;
      if (rpc_valid) {
        valid_containers++;
      }
    }
    return (valid_containers == num_containers_);
  }

  // Sends a message requesting the container's log of all
  // RPC events since several milliseconds before the current time
  int send_validation_message(int container_id) {
    auto log_start_time =
        std::chrono::system_clock::now() - std::chrono::milliseconds(50);
    double log_start_time_millis =
        static_cast<double>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                log_start_time.time_since_epoch())
                .count()) /
        1000;
    UniquePoolPtr<double> data = memory::allocate_unique<double>(1);
    data.get()[0] = log_start_time_millis;
    std::unique_ptr<PredictionData> input =
        std::make_unique<DoubleVector>(std::move(data), 1);
    rpc::PredictionRequest request(InputType::Doubles);
    request.add_input(std::move(input));
    auto serialized_request = request.serialize();
    int msg_id =
        rpc_->send_message(std::move(serialized_request), container_id);
    return msg_id;
  }

  std::condition_variable_any test_completed_cv_;
  std::mutex test_completed_cv_mutex_;
  std::atomic<bool> test_completed_{false};

 private:
  std::unique_ptr<rpc::RPCService> rpc_;
  redox::Subscriber redis_subscriber_;
  redox::Redox redis_connection_;
  std::thread timeout_thread_;
  std::atomic<int> containers_connected_{0};
  std::atomic<int> containers_validated_{0};
  std::atomic<bool> timeout_thread_interrupted_{false};
  int num_containers_;

  // Mutex used to ensure stability of container-related
  // maps in asynchronous environment
  std::mutex container_maps_mutex;
  // Maintains a mapping between a container's connection id
  // and its associated model's name for debugging purposes
  std::unordered_map<int, std::string> container_names_map_;
  // Maintains a mapping from a container's id to a boolean
  // flag indicating whether or not its RPC protocol is valid
  std::unordered_map<int, RPCValidationResult> container_validation_map_;
  // Mapping of message id to the connection id of its
  // corresponding container
  std::unordered_map<int, int> msg_id_to_container_map_;

  void start_timeout_thread(long timeout_seconds) {
    timeout_thread_ = std::thread([this, timeout_seconds]() {
      int num_iters = 30;
      long sleep_interval_millis = (timeout_seconds * 1000) / num_iters;
      for (int i = 0; i < num_iters; i++) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(sleep_interval_millis));
        if (timeout_thread_interrupted_) {
          return;
        }
      }
      // Failed to validate containers after 30 seconds
      // End the test with a failed exit code
      log_error(LOGGING_TAG_RPC_TEST,
                "Failed to validate containers - test timed out!");
      timeout_thread_interrupted_ = true;
      test_completed_ = true;
      test_completed_cv_.notify_all();
    });
  }

  void stop_timeout_thread() {
    if (!timeout_thread_interrupted_) {
      timeout_thread_interrupted_ = true;
      timeout_thread_.join();
    }
  }

  void on_response_received(rpc::RPCResponse response) {
    int msg_id = response.first;
    std::unique_lock<std::mutex> lock(container_maps_mutex);
    auto container_id_entry = msg_id_to_container_map_.find(msg_id);
    if (container_id_entry == msg_id_to_container_map_.end()) {
      throw std::runtime_error(
          "Failed to find container associated with previously sent message!");
    }
    int container_id = container_id_entry->second;
    auto container_valid_entry = container_validation_map_.find(container_id);
    RPCValidationResult container_rpc_protocol_valid;
    if (container_valid_entry == container_validation_map_.end()) {
      // Container has not yet been validated
      rpc::PredictionResponse prediction_response =
          rpc::PredictionResponse::deserialize_prediction_response(
              std::move(response.second));
      auto event_history = prediction_response.outputs_[0];
      SharedPoolPtr<char> event_history_data = get_data<char>(event_history);
      std::string event_history_str(
          event_history_data.get() + event_history->start(),
          event_history_data.get() + event_history->start() +
              event_history->size());
      rapidjson::Document d;
      json::parse_json(event_history_str, d);
      auto events = d.GetArray();
      std::vector<int> parsed_event_history;
      for (int i = 0; static_cast<size_t>(i) < events.Size(); i++) {
        parsed_event_history.push_back(events[i].GetInt());
      }
      container_rpc_protocol_valid =
          validate_rpc_protocol(parsed_event_history);
      container_validation_map_[container_id] = container_rpc_protocol_valid;
      containers_validated_ += 1;
      if (containers_validated_ == num_containers_) {
        stop_timeout_thread();
        test_completed_ = true;
        test_completed_cv_.notify_all();
      }
    } else {
      container_rpc_protocol_valid = RPCValidationResult(
          false,
          "Container sent excessive container content messages (expected 1)");
      container_validation_map_[container_id] = container_rpc_protocol_valid;
    }
    log_validation_result(container_id, container_rpc_protocol_valid);
  }

  RPCValidationResult validate_rpc_protocol(std::vector<int> &event_history) {
    if (event_history.size() < EVENT_HISTORY_MINIMUM_LENGTH) {
      return RPCValidationResult(
          false, "Protocol failed to exchange minimally required messages!");
    }
    int recv_heartbeat_index = 0;
    for (int i = 0; static_cast<size_t>(i) < event_history.size(); i++) {
      rpc::RPCEvent curr_event = static_cast<rpc::RPCEvent>(event_history[i]);
      if (curr_event == rpc::RPCEvent::ReceivedHeartbeat) {
        recv_heartbeat_index = i;
        break;
      }
    }
    if (recv_heartbeat_index < 1) {
      // The container definitely sent a heartbeat message to Clipper,
      // but it's missing from the log
      return RPCValidationResult(
          false, "Container log is missing sending of heartbeat message!");
    }
    bool initial_messages_correct =
        (static_cast<rpc::RPCEvent>(event_history[recv_heartbeat_index - 1]) ==
         rpc::RPCEvent::SentHeartbeat) &&
        (static_cast<rpc::RPCEvent>(event_history[recv_heartbeat_index + 1]) ==
         rpc::RPCEvent::SentContainerMetadata);
    if (!initial_messages_correct) {
      return RPCValidationResult(
          false, "Initial protocol messages are of incorrect types!");
    }
    int received_container_content_count = 0;
    for (int i = 3; i < static_cast<int>(event_history.size()); i++) {
      if (static_cast<rpc::RPCEvent>(event_history[i]) ==
          rpc::RPCEvent::ReceivedContainerContent) {
        received_container_content_count++;
      }
      if (static_cast<rpc::RPCEvent>(event_history[i]) ==
          rpc::RPCEvent::ReceivedContainerMetadata) {
        // The container should never receive container metadata from Clipper
        return RPCValidationResult(
            false, "Clipper sent an erroneous container metadata message!");
      }
    }
    if (received_container_content_count > 1) {
      std::stringstream ss;
      ss << "Clipper sent excessive container content messages! " << std::endl;
      ss << "Expected: 1, Sent: " << received_container_content_count;
      return RPCValidationResult(false, ss.str());
    } else if (received_container_content_count < 1) {
      // The container definitely received a container content message,
      // but its missing from the log
      return RPCValidationResult(
          false,
          "Container log is missing reception of container content message!");
    }
    return RPCValidationResult(true, "");
  }

  void log_validation_result(int container_id, RPCValidationResult &result) {
    std::string container_name =
        container_names_map_.find(container_id)->second;
    if (result.first) {
      log_info_formatted(LOGGING_TAG_RPC_TEST,
                         "Successfully validated container: \"{}\"",
                         container_name);
    } else {
      log_error_formatted(LOGGING_TAG_RPC_TEST,
                          "Failed to validate container: \"{}\". Error: {}",
                          container_name, result.second);
    }
  }
};

int main(int argc, char *argv[]) {
  cxxopts::Options options("rpc_test", "Clipper RPC Correctness Test");
  // clang-format off
  options.add_options()
      ("redis_ip", "Redis address",
       cxxopts::value<std::string>()->default_value("localhost"))
      ("redis_port", "Redis port",
       cxxopts::value<int>()->default_value(std::to_string(DEFAULT_REDIS_PORT)))
      ("num_containers", "Number of containers to validate",
       cxxopts::value<int>()->default_value("1"))
      ("rpc_service_port", "RPCService's port",
       cxxopts::value<int>()->default_value(std::to_string(DEFAULT_RPC_SERVICE_PORT)))
      ("timeout_seconds", "Timeout in seconds",
       cxxopts::value<long>()->default_value("30"));
  // clang-format on
  options.parse(argc, argv);

  get_config().set_redis_address(options["redis_ip"].as<std::string>());
  get_config().set_redis_port(options["redis_port"].as<int>());
  get_config().set_rpc_service_port(options["rpc_service_port"].as<int>());
  get_config().ready();

  Tester tester(options["num_containers"].as<int>());
  tester.start(options["timeout_seconds"].as<long>());

  std::unique_lock<std::mutex> l(tester.test_completed_cv_mutex_);
  tester.test_completed_cv_.wait(
      l, [&tester]() { return tester.test_completed_ == true; });
  exit(tester.succeeded() ? EXIT_SUCCESS : EXIT_FAILURE);
}
