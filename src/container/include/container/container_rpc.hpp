#ifndef CLIPPER_CONTAINER_RPC_HPP
#define CLIPPER_CONTAINER_RPC_HPP

#include <chrono>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>

#include <zmq.hpp>

#include <container/container_parsing.hpp>
#include <container/datatypes.hpp>
#include <container/util.hpp>

const std::string LOGGING_TAG_CONTAINER = "CONTAINER";

using Clock = std::chrono::system_clock;

namespace container {

constexpr long SOCKET_POLLING_TIMEOUT_MILLIS = 5000;
constexpr long SOCKET_ACTIVITY_TIMEOUT_MILLIS = 30000;
constexpr long EVENT_LOG_CAPACITY = 100;

using RPCLogItem = std::pair<RPCEvent, Clock::time_point>;

template <typename T>
struct supported_input_trait {
  static const bool is_supported = false;
};

template <>
struct supported_input_trait<ByteVector> {
  static const bool is_supported = true;
};

template <>
struct supported_input_trait<IntVector> {
  static const bool is_supported = true;
};

template <>
struct supported_input_trait<FloatVector> {
  static const bool is_supported = true;
};

template <>
struct supported_input_trait<DoubleVector> {
  static const bool is_supported = true;
};

template <>
struct supported_input_trait<SerializableString> {
  static const bool is_supported = true;
};

template <class I>
class Model {
 public:
  virtual std::vector<std::string> predict(
      const std::vector<I> inputs) const = 0;
  virtual InputType get_input_type() const = 0;
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
  void start_async(Model<Input<D>>& model, std::string model_name,
                   int model_version, std::string clipper_ip,
                   int clipper_port) {
    start_rpc(model, model_name, model_version, clipper_ip, clipper_port);
    serving_thread_.detach();
  }

  template <typename D>
  void start(Model<Input<D>>& model, std::string model_name, int model_version,
             std::string clipper_ip, int clipper_port) {
    start_rpc(model, model_name, model_version, clipper_ip, clipper_port);
    serving_thread_.join();
  }

  void stop();

  /**
   * @return The most recent RPC events that have occurred
   */
  std::vector<RPCLogItem> get_events() const;

 private:
  std::thread serving_thread_;
  std::atomic_bool active_;
  std::shared_ptr<std::mutex> event_log_mutex_;
  std::shared_ptr<CircularBuffer<RPCLogItem>> event_log_;

  template <typename D>
  void start_rpc(Model<Input<D>>& model, std::string& model_name,
                 int model_version, std::string& clipper_ip, int clipper_port) {
    static_assert(supported_input_trait<Input<D>>::is_supported,
                  "Model must be of a supported input type!");
    if (active_) {
      throw std::runtime_error(
          "Cannot start a container that is already started!");
    }
    active_ = true;
    const std::string clipper_address =
        "tcp://" + clipper_ip + ":" + std::to_string(clipper_port);
    std::cout << "Starting container RPC with clipper ip: " << clipper_ip
              << " and port: " << clipper_port << std::endl;
    serving_thread_ = std::thread(
        [this, clipper_address, &model, model_name, model_version]() {
          serve_model(model, model_name, model_version, clipper_address);
        });
  }

  /**
 * @return `true` if the received heartbeat is a request for container metadata.
 * `false` otherwise.
 */
  bool handle_heartbeat(zmq::socket_t& socket) const;

  void send_heartbeat(zmq::socket_t& socket) const;

  void send_container_metadata(std::string& model_name, int model_version,
                               InputType model_input_type,
                               zmq::socket_t& socket) const;

  void log_event(RPCEvent event) const;

  template <typename D>
  void serve_model(Model<Input<D>>& model, std::string model_name,
                   int model_version, std::string clipper_address) {
    zmq::context_t context(1);
    bool connected = false;
    std::chrono::time_point<Clock> last_activity_time;

    std::vector<int> input_header_buffer;
    std::vector<D> input_buffer;
    InputParser<D> input_parser(input_buffer);
    std::vector<uint8_t> output_buffer;

    while (true) {
      zmq::socket_t socket = zmq::socket_t(context, ZMQ_DEALER);
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
              std::cout << "Connection timed out, reconnecting..." << std::endl;
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
        zmq::message_t msg_msg_type_bytes;

        socket.recv(&msg_delimiter, 0);
        socket.recv(&msg_msg_type_bytes, 0);

        MessageType message_type = static_cast<MessageType>(
            static_cast<int*>(msg_msg_type_bytes.data())[0]);

        switch (message_type) {
          case MessageType::Heartbeat: {
            std::cout << "Received heartbeat!" << std::endl;
            log_event(RPCEvent::ReceivedHeartbeat);
            bool requesting_metadata = handle_heartbeat(socket);
            if (requesting_metadata) {
              send_container_metadata(model_name, model_version,
                                      model.get_input_type(), socket);
            }
          } break;

          case MessageType::ContainerContent: {
            log_event(RPCEvent::ReceivedContainerContent);
            zmq::message_t msg_request_id;
            zmq::message_t msg_request_header;

            socket.recv(&msg_request_id, 0);
            socket.recv(&msg_request_header, 0);

            int msg_id = static_cast<int*>(msg_request_id.data())[0];
            RequestType request_type = static_cast<RequestType>(
                static_cast<int*>(msg_request_header.data())[0]);

            switch (request_type) {
              case RequestType::PredictRequest:
                handle_predict_request(model, input_parser, socket,
                                       input_header_buffer, output_buffer,
                                       msg_id);
                break;

              case RequestType::FeedbackRequest:
                // Do nothing for now
                break;

              default: break;
            }

          } break;

          case MessageType::NewContainer:
            log_event(RPCEvent::ReceivedContainerMetadata);
            std::cout << "Error! Received erroneous new container message from "
                         "Clipper!"
                      << std::endl;
          default: break;
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
  void handle_predict_request(Model<Input<D>>& model,
                              InputParser<D> input_parser,
                              zmq::socket_t& socket,
                              std::vector<int>& input_header_buffer,
                              std::vector<uint8_t>& output_buffer,
                              int msg_id) const {
    zmq::message_t msg_input_header_size;
    socket.recv(&msg_input_header_size, 0);
    long input_header_size_bytes =
        static_cast<long*>(msg_input_header_size.data())[0];

    // Resize input header buffer if necessary
    if (static_cast<long>((input_header_buffer.size() / sizeof(long))) <
        input_header_size_bytes) {
      input_header_buffer.resize(2 * (input_header_size_bytes) / sizeof(long));
    }

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

    zmq::message_t msg_raw_content_size;
    socket.recv(&msg_raw_content_size, 0);
    long input_content_size_bytes =
        static_cast<long*>(msg_raw_content_size.data())[0];

    std::vector<D>& input_data_buffer =
        input_parser.get_data_buffer(input_content_size_bytes);
    // Receive input content
    socket.recv(input_data_buffer.data(), input_content_size_bytes, 0);

    PerformanceTimer::log_elapsed("Recv");

    std::vector<Input<D>> inputs =
        input_parser.get_inputs(input_header_buffer, input_content_size_bytes);

    PerformanceTimer::log_elapsed("Parse");

    // Make predictions
    std::vector<std::string> outputs = model.predict(inputs);

    // Send the outputs as a prediction response

    int num_outputs = static_cast<int>(outputs.size());

    // The output buffer begins with the number of outputs
    // encoded as an integer
    long response_size_bytes = sizeof(int);

    for (int i = 0; i < num_outputs; i++) {
      int output_length = static_cast<int>(outputs[i].length());
      // The output buffer contains the size of each output
      // encoded as an integer
      response_size_bytes += sizeof(int);
      response_size_bytes += output_length;
    }

    if (static_cast<long>(output_buffer.size()) < response_size_bytes) {
      output_buffer.resize(2 * response_size_bytes);
    }

    int* output_int_view = reinterpret_cast<int*>(output_buffer.data());
    output_int_view[0] = num_outputs;
    // Advance the integer output buffer to the correct position
    // for the output lengths content
    output_int_view++;

    uint8_t* output_byte_view =
        reinterpret_cast<uint8_t*>(output_buffer.data());
    // Advance the byte output buffer to the correct position for the
    // output content
    output_byte_view += sizeof(int) + (num_outputs * sizeof(int));

    for (int i = 0; i < num_outputs; i++) {
      int output_length = static_cast<int>(outputs[i].length());
      output_int_view[i] = output_length;
      memcpy(output_byte_view, outputs[i].data(), output_length);
      output_byte_view += output_length;
    }

    zmq::message_t msg_message_type(sizeof(int));
    static_cast<int*>(msg_message_type.data())[0] =
        static_cast<int>(MessageType::ContainerContent);

    zmq::message_t msg_message_id(sizeof(int));
    static_cast<int*>(msg_message_id.data())[0] = msg_id;

    zmq::message_t msg_prediction_response(output_buffer.data(),
                                           response_size_bytes, NULL);

    socket.send("", 0, ZMQ_SNDMORE);
    socket.send(msg_message_type, ZMQ_SNDMORE);
    socket.send(msg_message_id, ZMQ_SNDMORE);
    socket.send(msg_prediction_response, 0);
    log_event(RPCEvent::SentContainerContent);

    PerformanceTimer::log_elapsed("Handle");
    std::cout << PerformanceTimer::get_log() << std::endl;
  }
};

}  // namespace container
#endif  // CLIPPER_CONTAINER_RPC_HPP
