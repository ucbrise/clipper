#include <rapidjson/rapidjson.h>

#include <container/container_rpc.hpp>
#include <container/container_parsing.hpp>
#include <clipper/logging.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/json_util.hpp>

using namespace clipper::container;

class StringModel : public Model<clipper::SerializableString> {
 public:
  std::vector<std::string> predict(const std::vector<std::shared_ptr<clipper::SerializableString>> inputs) const {
    std::vector<std::string> outputs;
    for(auto const& elem : inputs) {
      clipper::log_info(LOGGING_TAG_CONTAINER, elem->get_data());
      outputs.push_back("result");
    }
    return outputs;
  }

  InputType get_input_type() const {
    return InputType::Strings;
  }
};

class RPCTestModel : public Model<clipper::DoubleVector> {
 public:
  RPCTestModel(RPC& container_rpc) : container_rpc_(container_rpc) {

  }

  std::vector<std::string> predict(const std::vector<std::shared_ptr<clipper::DoubleVector>> inputs) const override {
    std::vector<std::string> outputs;
    for(auto const& input : inputs) {
      long min_timestamp_millis = static_cast<long>(input->get_data()[0]);
      std::vector<clipper::rpc::RPCEvent> recent_events = get_events(min_timestamp_millis);
      rapidjson::Document output_json;
      output_json.SetArray();
      rapidjson::Document::AllocatorType& allocator = output_json.GetAllocator();
      for(const clipper::rpc::RPCEvent &event : recent_events) {
        int code = static_cast<int>(event);
        output_json.PushBack(code, allocator);
      }
      std::string output = clipper::json::to_json_string(output_json);
      outputs.push_back(output);
    }
    return outputs;
  }

  InputType get_input_type() const override {
    return InputType::Doubles;
  }

 private:
  RPC &container_rpc_;

  std::vector<clipper::rpc::RPCEvent> get_events(long min_time_millis) const {
    std::vector<clipper::rpc::RPCEvent> ordered_events;
    std::vector<RPCLogItem> log_items = container_rpc_.get_events();
    std::chrono::milliseconds min_duration(min_time_millis);

    bool added_event = false;
    for(int i = 0; i < static_cast<int>(log_items.size()); i++) {
      RPCLogItem curr_item = log_items[i];
      long time_difference =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              curr_item.second.time_since_epoch() - min_duration).count();
      if(time_difference > 0) {
        if(!added_event && i > 0) {
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

int main() {
  RPC container_rpc;
  //RPCTestModel test_model(container_rpc);
  StringModel test_model;
  std::string model_name = "cpp_test";
  int model_version = 1;
  //DoubleVectorParser parser;
  SerializableStringParser parser;


  std::string clipper_ip = "localhost";
  int clipper_port = 7000;

  container_rpc.start(test_model, model_name, model_version, parser, clipper_ip, clipper_port);
  while(true);
}