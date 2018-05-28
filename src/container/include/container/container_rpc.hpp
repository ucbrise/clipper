#ifndef CLIPPER_CONTAINER_RPC_HPP
#define CLIPPER_CONTAINER_RPC_HPP

#include <chrono>
#include <mutex>
#include <numeric>
#include <sstream>
#include <thread>

#include <boost/circular_buffer.hpp>
#include <zmq.hpp>

#include <clipper/datatypes.hpp>
#include <clipper/logging.hpp>
#include <clipper/rpc_service.hpp>
#include <container/datatypes.hpp>
#include "container_parsing.hpp"

const std::string LOGGING_TAG_CONTAINER = "CONTAINER";
using Clock = std::chrono::system_clock;

namespace clipper {

namespace container {

constexpr long SOCKET_POLLING_TIMEOUT_MILLIS = 5000;
constexpr long SOCKET_ACTIVITY_TIMEOUT_MILLIS = 30000;
constexpr long EVENT_LOG_CAPACITY = 100;

using RPCLogItem = std::pair<rpc::RPCEvent, Clock::time_point>;

template <typename T>
struct input_trait {
  static const bool is_supported = false;
  static const InputType input_type = InputType::Invalid;
};

template <>
struct input_trait<ByteVector> {
  static const bool is_supported = true;
  static const InputType input_type = InputType::Bytes;
};

template <>
struct input_trait<IntVector> {
  static const bool is_supported = true;
  static const InputType input_type = InputType::Ints;
};

template <>
struct input_trait<FloatVector> {
  static const bool is_supported = true;
  static const InputType input_type = InputType::Floats;
};

template <>
struct input_trait<DoubleVector> {
  static const bool is_supported = true;
  static const InputType input_type = InputType::Doubles;
};

template <>
struct input_trait<SerializableString> {
  static const bool is_supported = true;
  static const InputType input_type = InputType::Strings;
};

template <class I>
class Model {
 public:
  Model() : input_type_(input_trait<I>::input_type) {
    static_assert(input_trait<I>::is_supported,
                  "Model must be of a supported input type!");
  }

  virtual std::vector<std::string> predict(
      const std::vector<I> inputs) const = 0;

  InputType get_input_type() const { return input_type_; }

 private:
  const InputType input_type_;
};

// This is not thread safe
class PerformanceTimer {
 public:
  static void start_timing() {
    log_.str("");
    last_log_ = Clock::now();
  }

  static void log_elapsed(const std::string tag) {
    Clock::time_point curr_time = Clock::now();
    long log_diff_micros =
        std::chrono::duration_cast<std::chrono::microseconds>(curr_time -
                                                              last_log_)
            .count();
    log_ << tag << ": " << std::to_string(log_diff_micros) << " us"
         << ", ";
    last_log_ = curr_time;
  }

  static std::string get_log() { return log_.str(); }

 private:
  static std::stringstream log_;
  static Clock::time_point last_log_;
};

class RPC {
 public:
  RPC();
  ~RPC();

  // Disallow copy
  RPC(const RPC&) = delete;
  RPC& operator=(const RPC&) = delete;

  // Default move constructor and assignment.
  RPC(RPC&& other) = default;
  RPC& operator=(RPC&& other) = default;

  template <typename D>
  void start(Model<Input<D>>& model, std::string model_name, int model_version,
             std::string clipper_ip, int clipper_port) {
    if (active_) {
      throw std::runtime_error(
          "Cannot start a container that is already started!");
    }
    active_ = true;
    const std::string clipper_address =
        "tcp://" + clipper_ip + ":" + std::to_string(clipper_port);
    log_info_formatted(
        LOGGING_TAG_CONTAINER,
        "Starting container RPC with clipper ip: {} and port: {}", clipper_ip,
        clipper_port);
    serving_thread_ = std::thread(
        [this, clipper_address, &model, model_name, model_version]() {
          serve_model(model, model_name, model_version, clipper_address);
        });
  }

  void stop();

  /**
   * @return The `num_events` most recent RPC events that have occurred
   */
  std::vector<RPCLogItem> get_events(int num_events = EVENT_LOG_CAPACITY) const;

 private:
  std::thread serving_thread_;
  std::atomic_bool active_;
  std::shared_ptr<std::mutex> event_log_mutex_;
  std::shared_ptr<boost::circular_buffer<RPCLogItem>> event_log_;

  void validate_rpc_version(const uint32_t received_version) {
    if (received_version != rpc::RPC_VERSION) {
      log_error_formatted(LOGGING_TAG_CONTAINER,
                          "Received an RPC message with version: {} that does "
                          "not match container version: {}",
                          received_version, rpc::RPC_VERSION);
    }
  }

  /**
   * @return `true` if the received heartbeat is a request for container
   * metadata. `false` otherwise.
   */
  bool handle_heartbeat(zmq::socket_t& socket) const;

  void send_heartbeat(zmq::socket_t& socket) const;

  void send_container_metadata(std::string& model_name, int model_version,
                               InputType model_input_type,
                               zmq::socket_t& socket) const;

  void log_event(rpc::RPCEvent event) const;

  template <typename D>
  void serve_model(Model<Input<D>>& model, std::string model_name,
                   int model_version, std::string clipper_address) {
    // Initialize a ZeroMQ context with a single IO thread.
    // This thread will be used by the socket we're about to create
    zmq::context_t context(1);
    bool connected = false;
    std::chrono::time_point<Clock> last_activity_time;

    std::vector<uint64_t> input_header_buffer;
    std::vector<D> input_data_buffer;
    std::vector<uint64_t> output_header_buffer;
    std::vector<uint8_t> output_buffer;

    while (true) {
      zmq::socket_t socket = socket_t(context, ZMQ_DEALER);
      zmq::pollitem_t items[] = {{socket, 0, ZMQ_POLLIN, 0}};
      socket.connect(clipper_address);
      send_heartbeat(socket);

      while (active_) {
        zmq_poll(items, 1, SOCKET_POLLING_TIMEOUT_MILLIS);
        if (!(items[0].revents & ZMQ_POLLIN)) {
          if (connected) {
            std::chrono::time_point<Clock> curr_time = Clock::now();
            auto time_since_last_activity =
                curr_time.time_since_epoch() -
                last_activity_time.time_since_epoch();
            long time_since_last_activity_millis =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    time_since_last_activity)
                    .count();
            if (time_since_last_activity_millis >=
                SOCKET_ACTIVITY_TIMEOUT_MILLIS) {
              log_info(LOGGING_TAG_CONTAINER,
                       "Connection timed out, reconnecting...");
              connected = false;
              break;
            } else {
              send_heartbeat(socket);
            }
          }
          continue;
        }

        connected = true;
        last_activity_time = Clock::now();

        PerformanceTimer::start_timing();

        zmq::message_t msg_delimiter;
        zmq::message_t msg_rpc_type_bytes;
        zmq::message_t msg_msg_type_bytes;

        socket.recv(&msg_delimiter, 0);
        socket.recv(&msg_rpc_type_bytes, 0);
        socket.recv(&msg_msg_type_bytes, 0);

        uint32_t rpc_version =
            static_cast<uint32_t*>(msg_rpc_type_bytes.data())[0];
        validate_rpc_version(rpc_version);

        uint32_t message_type_code =
            static_cast<uint32_t*>(msg_msg_type_bytes.data())[0];
        rpc::MessageType message_type =
            static_cast<rpc::MessageType>(message_type_code);

        switch (message_type) {
          case rpc::MessageType::Heartbeat: {
            log_info(LOGGING_TAG_CONTAINER, "Received heartbeat!");
            log_event(rpc::RPCEvent::ReceivedHeartbeat);
            bool requesting_metadata = handle_heartbeat(socket);
            if (requesting_metadata) {
              send_container_metadata(model_name, model_version,
                                      model.get_input_type(), socket);
            }
          } break;

          case rpc::MessageType::ContainerContent: {
            log_event(rpc::RPCEvent::ReceivedContainerContent);
            zmq::message_t msg_request_id;
            zmq::message_t msg_request_header;

            socket.recv(&msg_request_id, 0);
            socket.recv(&msg_request_header, 0);

            uint32_t msg_id = static_cast<uint32_t*>(msg_request_id.data())[0];
            uint32_t request_type_code =
                static_cast<uint32_t*>(msg_request_header.data())[0];
            RequestType request_type =
                static_cast<RequestType>(request_type_code);

            switch (request_type) {
              case RequestType::PredictRequest:
                handle_predict_request(model, socket, input_header_buffer,
                                       input_data_buffer, output_header_buffer,
                                       msg_id);
                break;

              case RequestType::FeedbackRequest:
                throw std::runtime_error(
                    "Received unsupported feedback request!");
                break;

              default: {
                std::stringstream ss;
                ss << "Received RPC message of an unknown request type "
                      "corresponding to integer code "
                   << request_type_code;
                throw std::runtime_error(ss.str());
              }
            }

          } break;

          case rpc::MessageType::NewContainer:
            log_event(rpc::RPCEvent::ReceivedContainerMetadata);
            log_error_formatted(
                LOGGING_TAG_CONTAINER,
                "Received erroneous new container message from Clipper!");
          default: {
            std::stringstream ss;
            ss << "Received RPC message of an unknown message type "
                  "corresponding to integer code "
               << message_type_code;
            throw std::runtime_error(ss.str());
          }
        }
      }
      // The socket associated with the previous session is no longer
      // being used, so we should close it
      socket.close();
      if (!active_) {
        // The container is no longer active. Exit the connection loop.
        return;
      }
    }
  }

  template <typename D>
  void handle_predict_request(Model<Input<D>>& model, zmq::socket_t& socket,
                              std::vector<uint64_t>& input_header_buffer,
                              std::vector<D>& input_data_buffer,
                              std::vector<uint64_t>& output_header_buffer,
                              int msg_id) const {
    zmq::message_t msg_input_header_size;
    socket.recv(&msg_input_header_size, 0);
    uint64_t input_header_size_bytes =
        static_cast<uint64_t*>(msg_input_header_size.data())[0];

    // Resize input header buffer if necessary;
    resize_if_necessary(input_header_buffer, input_header_size_bytes);

    // Receive input header
    socket.recv(input_header_buffer.data(), input_header_size_bytes, 0);

    InputType input_type = static_cast<InputType>(input_header_buffer[0]);
    if (input_type != model.get_input_type()) {
      std::stringstream ss;
      ss << "Received prediction request with incorrect input type '"
         << get_readable_input_type(input_type)
         << "' for model with input type '"
         << get_readable_input_type(model.get_input_type()) << "'";
      throw std::runtime_error(ss.str());
    }

    uint64_t num_inputs = input_header_buffer[1];
    uint64_t input_content_size_bytes =
        std::accumulate(input_header_buffer.begin() + 2,
                        input_header_buffer.begin() + 2 + num_inputs, 0);

    std::vector<Input<D>> inputs;
    inputs.reserve(num_inputs);
    resize_if_necessary(input_data_buffer, input_content_size_bytes);

    D* data_ptr = input_data_buffer.data();
    for (uint32_t i = 0; i < num_inputs; i++) {
      uint64_t input_size_bytes = input_header_buffer[i + 2];
      socket.recv(data_ptr, input_size_bytes, 0);
      inputs.push_back(Input<D>(data_ptr, input_size_bytes));
      data_ptr += input_size_bytes;
    }

    PerformanceTimer::log_elapsed("Recv and Parse");

    // Make predictions
    std::vector<std::string> outputs = model.predict(inputs);

    // Send outputs as a prediction response
    uint64_t num_outputs = outputs.size();
    if (num_outputs != inputs.size()) {
      std::stringstream ss;
      ss << "Number of model outputs: " << outputs.size()
         << " does not equal the number of inputs: " << inputs.size();
      throw std::runtime_error(ss.str());
    }

    uint64_t output_header_size =
        create_output_header(outputs, output_header_buffer);

    zmq::message_t msg_message_type(sizeof(uint32_t));
    static_cast<uint32_t*>(msg_message_type.data())[0] =
        static_cast<uint32_t>(rpc::MessageType::ContainerContent);

    zmq::message_t msg_message_id(sizeof(uint32_t));
    static_cast<uint32_t*>(msg_message_id.data())[0] = msg_id;

    zmq::message_t msg_header_size(sizeof(uint64_t));
    static_cast<uint64_t*>(msg_header_size.data())[0] = output_header_size;

    socket.send("", 0, ZMQ_SNDMORE);
    socket.send(msg_message_type, ZMQ_SNDMORE);
    socket.send(msg_message_id, ZMQ_SNDMORE);
    socket.send(msg_header_size, ZMQ_SNDMORE);
    socket.send(output_header_buffer.data(), output_header_size, ZMQ_SNDMORE);
    uint64_t last_msg_num = num_outputs - 1;
    for (uint64_t i = 0; i < num_outputs; i++) {
      std::string& output = outputs[i];
      if (i < last_msg_num) {
        socket.send(output.begin(), output.end(), ZMQ_SNDMORE);
      } else {
        // If this is the last output, we don't want to use
        // the 'SNDMORE' flag
        socket.send(output.begin(), output.end(), 0);
      }
    }

    log_event(rpc::RPCEvent::SentContainerContent);

    PerformanceTimer::log_elapsed("Handle");
    log_info(LOGGING_TAG_CONTAINER, PerformanceTimer::get_log());
  }

  uint64_t create_output_header(
      std::vector<std::string>& outputs,
      std::vector<uint64_t>& output_header_buffer) const {
    uint64_t num_outputs = outputs.size();
    uint64_t output_header_size = (num_outputs + 1) * sizeof(uint64_t);
    resize_if_necessary(output_header_buffer, output_header_size);
    uint64_t* output_header_data = output_header_buffer.data();
    output_header_data[0] = num_outputs;
    for (uint64_t i = 0; i < num_outputs; i++) {
      output_header_data[i + 1] = outputs[i].size();
    }
    return output_header_size;
  }

  template <typename D>
  void resize_if_necessary(std::vector<D>& buffer,
                           uint64_t required_buffer_size) const {
    if ((buffer.size() * sizeof(D)) < required_buffer_size) {
      buffer.reserve((2 * required_buffer_size) / sizeof(D));
    }
  }
};

}  // namespace container

}  // namespace clipper
#endif  // CLIPPER_CONTAINER_RPC_HPP
