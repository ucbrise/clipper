/**
  NOTE: This file was slightly adapted from `container_rpc.cpp`
  within the C++ container source to meet R packaging requirements.
*/

#include <algorithm>
#include <iostream>

#include <Rcpp.h>
#include "zmq.hpp"

#include "container_rpc.hpp"
#include "container_util.hpp"

namespace container {

std::stringstream PerformanceTimer::log_;
Clock::time_point PerformanceTimer::last_log_ =
    Clock::time_point(std::chrono::milliseconds(0));

RPC::RPC()
    : active_(false),
      event_log_mutex_(std::make_shared<std::mutex>()),
      event_log_(
          std::make_shared<CircularBuffer<RPCLogItem>>(EVENT_LOG_CAPACITY)) {}

RPC::~RPC() { stop(); }

void RPC::stop() {
  if (active_) {
    active_ = false;
  }
}

std::vector<RPCLogItem> RPC::get_events() const {
  std::vector<RPCLogItem> events;
  std::lock_guard<std::mutex> lock(*event_log_mutex_);
  return event_log_->get_items();
}

bool RPC::handle_heartbeat(zmq::socket_t &socket) const {
  zmq::message_t msg_heartbeat_type;
  socket.recv(&msg_heartbeat_type, 0);
  HeartbeatType heartbeat_type = static_cast<HeartbeatType>(
      static_cast<int *>(msg_heartbeat_type.data())[0]);
  return (heartbeat_type == HeartbeatType::RequestContainerMetadata);
}

void RPC::send_heartbeat(zmq::socket_t &socket) const {
  zmq::message_t type_message(sizeof(int));
  static_cast<int *>(type_message.data())[0] =
      static_cast<int>(MessageType::Heartbeat);
  socket.send("", 0, ZMQ_SNDMORE);
  socket.send(type_message, 0);
  Rcpp::Rcout << "Sent heartbeat!" << std::endl;
  log_event(RPCEvent::SentHeartbeat);
}

void RPC::send_container_metadata(std::string &model_name, int model_version,
                                  InputType model_input_type,
                                  zmq::socket_t &socket) const {
  zmq::message_t msg_message_type(sizeof(int));
  static_cast<int *>(msg_message_type.data())[0] =
      static_cast<int>(MessageType::NewContainer);

  zmq::message_t msg_rpc_version(sizeof(uint32_t));
  static_cast<uint32_t *>(msg_rpc_version.data())[0] = RPC_VERSION;

  std::string model_version_str = std::to_string(model_version);
  std::string model_input_type_str =
      std::to_string(static_cast<int>(model_input_type));

  socket.send("", 0, ZMQ_SNDMORE);
  socket.send(msg_message_type, ZMQ_SNDMORE);
  socket.send(model_name.data(), model_name.length(), ZMQ_SNDMORE);
  socket.send(model_version_str.data(), model_version_str.length(),
              ZMQ_SNDMORE);
  socket.send(model_input_type_str.data(), model_version_str.length(),
              ZMQ_SNDMORE);
  socket.send(msg_rpc_version, 0);
  Rcpp::Rcout << "Sent container metadata!" << std::endl;
  log_event(RPCEvent::SentContainerMetadata);
}

void RPC::log_event(RPCEvent event) const {
  std::lock_guard<std::mutex> lock(*event_log_mutex_);
  Clock::time_point curr_time = Clock::now();
  auto new_log_item = std::make_pair(event, curr_time);
  event_log_->insert(new_log_item);
}

}  // namespace container
