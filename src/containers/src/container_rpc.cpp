#include "container_rpc.hpp"

#include <chrono>

#include <zmq.hpp>

#include <clipper/logging.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/rpc_service.hpp>

using Clock = std::chrono::system_clock;

namespace clipper {

namespace container {

constexpr long SOCKET_POLLING_TIMEOUT_MILLIS = 5000;
constexpr SOCKET_ACTIVITY_TIMEOUT_MILLIS = 30000;

RPC::RPC() : stopped_(true) {

}

RPC::~RPC() {
  stop();
  serving_thread_.join();
}

void RPC::start(const std::string &clipper_ip, int clipper_port) {
  stopped_ = false;
  const std::string clipper_address = "tcp://" + ip + ":" + std::to_string(port);
  serving_thread_ = std::thread([this, clipper_address](){
    serve_model(clipper_address);
  });
}

void RPC::stop() {
  stopped_ = true;
}

void RPC::serve_model(const std::string& clipper_address) {
  zmq::context_t context = context_t(1);
  bool connected = false;
  std::chrono::time_point<Clock> last_activity_time;

  while(true) {
    zmq::socket_t socket = socket_t(context, ZMQ_DEALER);
    zmq::pollitem_t items[] = {{socket, 0, ZMQ_POLLIN, 0}};
    socket.connect(clipper_address);
    send_heartbeat(socket);

    while(true) {
      zmq_poll(items, 1, SOCKET_POLLING_TIMEOUT_MILLIS);
      if (!(items[0].revents & ZMQ_POLLIN)) {
        if (connected) {
          std::chrono::time_point<Clock> curr_time = Clock::now();
          auto time_since_last_activity = curr_time.time_since_epoch() - last_activity_time.time_since_epoch();
          long time_since_last_activity_millis =
              std::chrono::duration_cast<std::chrono::milliseconds>(time_since_last_activity).count();
          if (time_since_last_activity_millis >= SOCKET_ACTIVITY_TIMEOUT_MILLIS) {
            log_info(LOGGING_TAG_CONTAINER, "Connection timed out, reconnecting...");
            socket.close();
            break;
          } else {
            send_heartbeat(socket);
          }
        } else{
          // We weren't connected previously, so let's keep polling
          continue;
        }
      }

      connected = true;
      last_activity_time = Clock::now();

      zmq::message_t msg_delimiter;
      zmq::message_t msg_msg_type_bytes;

      socket.recv(msg_delimiter, 0);
      socket.recv(msg_msg_type_bytes, 0);


      rpc::MessageType message_type(static_cast<int *>(msg_msg_type_bytes.data()));

      switch(message_type) {
        case rpc::MessageType::Heartbeat: {
          log_info(LOGGING_TAG_CONTAINER, "Received heartbeat!");
          // TODO(czumar) Log this event to our history
          zmq::message_t msg_heartbeat_type;
          socket.recv(0, msg_heartbeat_type);
          rpc::HeartbeatType heartbeat_type(static_cast<int *>(msg_heartbeat_type.data())[0]);
          if(heartbeat_type == rpc::HeartbeatType::RequestContainerMetadata) {
            send_container_metadata(socket);
          }
        } break;

        case rpc::MessageType::ContainerContent: {
          // TODO(czumar) Log this event to our history
          zmq::message_t msg_request_id;
          zmq::message_t msg_request_header;

          socket.recv(msg_request_id, 0);
          socket.recv(msg_request_header, 0);

          long msg_id = static_cast<long *>(msg_request_id.data())[0];
          RequestType request_type(static_cast<int *>(msg_request_header.data())[0]);

          switch(request_type) {
            case RequestType::PredictRequest: {
              zmq::message_t msg_input_header_size;
              zmq::message_t msg_input_header;
              zmq::message_t msg_raw_content_size;
              zmq::message_t msg_raw_content;

              socket.recv(msg_input_header_size, 0);
              socket.recv(msg_input_header, 0);
              socket.recv(msg_raw_content_size, 0);
              socket.recv(msg_raw_content, 0);

              long input_header_size = static_cast<long*>(msg_input_header_size.data())[0];

              long* input_header = static_cast<long*>(msg_input_header.data());
              InputType input_type(static_cast<int>(input_header[0]));
              long input_content_size = input_header[1];
              // Advance the input header to the beginning of the array of input sizes
              input_header += 2;

              uint8_t* raw_content = static_cast<uint8_t *>(msg_raw_content.data());

              // Let's use a parser here and figure out zero-copy for raw content!

              std::vector<Input> inputs;
              for(int i = 0; i < input_content_size; i++) {

              }
            } break;

            case RequestType::FeedbackRequest:
              break;

            default:
              break;
          }

        } break;

        case rpc::MessageType::NewContainer:
          // TODO(czumar): Log event history
          log_error_formatted(LOGGING_TAG_CONTAINER, "Received erroneous new container message from Clipper!");

        default:
          break;
      }


    }



  }


}

void RPC::handle_heartbeat(const zmq::socket_t &socket) const {}

void RPC::handle_predict_request(long msg_id, std::vector<Input> &inputs, Model<Input> model) const {

}

void RPC::send_heartbeat(const zmq::socket_t &socket) const {
  message_t type_message(sizeof(int));
  message_t heartbeat_type_message(sizeof(int));
  static_cast<int *>(type_message.data())[0] =
      static_cast<int>(MessageType::Heartbeat);
  static_cast<int *>(heartbeat_type_message.data())[0] = static_cast<int>(rpc::HeartbeatType::KeepAlive);
  socket.send("", 0, ZMQ_SNDMORE);
  socket.send(type_message, ZMQ_SNDMORE);
  socket.send(heartbeat_type_message);
}

void RPC::send_container_metadata(const zmq::socket_t &socket) const {

}

} // namespace container

} // namespace clipper