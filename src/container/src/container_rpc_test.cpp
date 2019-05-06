#include <cxxopts.hpp>

#include <rapidjson/rapidjson.h>

#include <clipper/json_util.hpp>
#include <clipper/logging.hpp>
#include <container/container_parsing.hpp>
#include <container/container_rpc.hpp>

using namespace clipper::container;

class RPCTestModel : public Model<DoubleVector> {
 public:
  RPCTestModel(RPC& container_rpc) : container_rpc_(container_rpc) {}

  std::vector<std::string> predict(
      const std::vector<DoubleVector> inputs) const override {
    std::vector<std::string> outputs;
    for (auto const& input : inputs) {
      long min_timestamp_millis = static_cast<long>(input.get_data()[0]);
      std::vector<clipper::rpc::RPCEvent> recent_events =
          get_events(min_timestamp_millis);
      rapidjson::Document output_json;
      output_json.SetArray();
      rapidjson::Document::AllocatorType& allocator =
          output_json.GetAllocator();
      for (const clipper::rpc::RPCEvent& event : recent_events) {
        int code = static_cast<int>(event);
        output_json.PushBack(code, allocator);
      }
      std::string output = clipper::json::to_json_string(output_json);
      outputs.push_back(output);
    }
    return outputs;
  }

 private:
  RPC& container_rpc_;

  std::vector<clipper::rpc::RPCEvent> get_events(long min_time_millis) const {
    std::vector<clipper::rpc::RPCEvent> ordered_events;
    std::vector<RPCLogItem> log_items = container_rpc_.get_events();
    std::chrono::milliseconds min_duration(min_time_millis);

    bool added_event = false;
    for (int i = 0; i < static_cast<int>(log_items.size()); i++) {
      RPCLogItem curr_item = log_items[i];
      long time_difference =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              curr_item.second.time_since_epoch() - min_duration)
              .count();
      if (time_difference > 0) {
        if (!added_event && i > 0) {
          // Capture the heartbeat message sent before
          // Clipper came online
          ordered_events.push_back(log_items[i - 1].first);
          added_event = true;
        }
        ordered_events.push_back(curr_item.first);
      }
    }
    return ordered_events;
  }
};

int main(int argc, char* argv[]) {
  cxxopts::Options options("Cpp RPC Test",
                           "RPC layer testing for cpp model containers");
  // clang-format off
  options.add_options()
      ("t,test_length", "length of the test in seconds",
       cxxopts::value<long>()->default_value("10"))
      ("p,rpc_service_port", "RPCService's port",
       cxxopts::value<int>()->default_value("7000"));
  // clang-format on
  options.parse(argc, argv);

  long test_length = options["test_length"].as<long>();
  int clipper_port = options["rpc_service_port"].as<int>();

  RPC container_rpc;
  RPCTestModel test_model(container_rpc);
  std::string model_name = "cpp_test";
  int model_version = 1;

  std::string clipper_ip = "localhost";

  container_rpc.start(test_model, model_name, model_version, clipper_ip,
                      clipper_port);

  std::this_thread::sleep_for(std::chrono::seconds(test_length));

  container_rpc.stop();
}