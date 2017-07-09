#include <algorithm>

#include <rapidjson/rapidjson.h>
#include <zmq.hpp>

#include <container/container_rpc.hpp>
#include <container/container_parsing.hpp>

namespace clipper {

namespace container {

RPC::RPC() : active_(false),
             event_log_mutex_(std::make_shared<std::mutex>()),
             event_log_(std::make_shared<boost::circular_buffer<RPCLogItem>>(EVENT_LOG_CAPACITY)) {

}

RPC::~RPC() {
  stop();
}

void RPC::stop() {
  if (active_) {
    active_ = false;
    serving_thread_.join();
  }
}

std::vector<RPCLogItem> RPC::get_events(int num_events) const {

  std::vector<RPCLogItem> events;
  std::lock_guard<std::mutex> lock(*event_log_mutex_);
  int num_to_return = std::min(num_events, static_cast<int>(events.size()));
  for (auto it = event_log_->begin(); it != event_log_->end(); ++it) {
    if(num_to_return == 0) {
      break;
    }
    events.push_back(*it);
    num_to_return--;
  }
  return events;
}

bool RPC::handle_heartbeat(zmq::socket_t &socket) const {
  zmq::message_t msg_heartbeat_type;
  socket.recv(&msg_heartbeat_type, 0);
  rpc::HeartbeatType heartbeat_type
      = static_cast<rpc::HeartbeatType>(static_cast<int *>(msg_heartbeat_type.data())[0]);
  return (heartbeat_type == rpc::HeartbeatType::RequestContainerMetadata);
}

void RPC::send_heartbeat(zmq::socket_t &socket) const {
  zmq::message_t type_message(sizeof(int));
  static_cast<int *>(type_message.data())[0] =
      static_cast<int>(rpc::MessageType::Heartbeat);
  socket.send("", 0, ZMQ_SNDMORE);
  socket.send(type_message);
  log_info(LOGGING_TAG_CONTAINER, "Sent heartbeat!");
  log_event(rpc::RPCEvent::SentHeartbeat);
}

void RPC::send_container_metadata(std::string &model_name,
                                  int model_version,
                                  InputType model_input_type,
                                  zmq::socket_t &socket) const {
  zmq::message_t msg_message_type(sizeof(int));
  static_cast<int *>(msg_message_type.data())[0] = static_cast<int>(rpc::MessageType::NewContainer);

  zmq::message_t msg_model_name(&model_name[0], model_name.length(), NULL);

  std::string model_version_str = std::to_string(model_version);
  zmq::message_t msg_model_version(&model_version_str[0], model_version_str.length(), NULL);

  std::string model_input_type_str = std::to_string(static_cast<int>(model_input_type));
  zmq::message_t msg_model_input_type(&model_input_type_str[0], model_input_type_str.length(), NULL);

  socket.send("", 0, ZMQ_SNDMORE);
  socket.send(msg_message_type, ZMQ_SNDMORE);
  socket.send(msg_model_name, ZMQ_SNDMORE);
  socket.send(msg_model_version, ZMQ_SNDMORE);
  socket.send(msg_model_input_type);
  log_info(LOGGING_TAG_CONTAINER, "Sent container metadata!");
  log_event(rpc::RPCEvent::SentContainerMetadata);
}

void RPC::log_event(rpc::RPCEvent event) const {
  std::lock_guard<std::mutex> lock(*event_log_mutex_);
  Clock::time_point curr_time = Clock::now();
  auto new_log_item = std::make_pair(event, curr_time);
  event_log_->push_back(new_log_item);
}

RPCTestModel::RPCTestModel(RPC &container_rpc) : container_rpc_(container_rpc) {

}

std::vector<std::string> RPCTestModel::predict(const std::vector<std::shared_ptr<DoubleVector>> inputs) const {
  std::vector<std::string> outputs;
  for(auto const& input : inputs) {
    long min_timestamp_millis = static_cast<long>(input->get_data()[0]);
    std::vector<rpc::RPCEvent> recent_events = get_events(min_timestamp_millis);
    rapidjson::Document output_json;
    output_json.SetArray();
    rapidjson::Document::AllocatorType& allocator = output_json.GetAllocator();
    for(const rpc::RPCEvent &event : recent_events) {
      int code = static_cast<int>(event);
      output_json.PushBack(code, allocator);
    }
    std::string output = json::to_json_string(output_json);
    outputs.push_back(output);
  }
  return outputs;
}

std::vector<rpc::RPCEvent> RPCTestModel::get_events(long min_time_millis) const {
  std::vector<rpc::RPCEvent> ordered_events;
  std::vector<RPCLogItem> log_items = container_rpc_.get_events();
  std::chrono::milliseconds min_duration(min_time_millis);

  bool added_event = false;
  for(int i = 0; i < static_cast<int>(log_items.size()); i++) {
    RPCLogItem curr_item = log_items[i];
    long time_difference =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            min_duration - curr_item.second.time_since_epoch()).count();
    if(time_difference > 0) {
      if(!added_event && i > 0) {
        // Capture the heartbeat message sent before
        // Clipper came online
        ordered_events.push_back(log_items[i - 1].first);
      }
      ordered_events.push_back(curr_item.first);
    }
  }
  return ordered_events;
}

InputType RPCTestModel::get_input_type() const {
  return InputType::Doubles;
}

} // namespace container

} // namespace clipper