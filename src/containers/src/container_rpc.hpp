#ifndef CLIPPER_CONTAINER_RPC_HPP
#define CLIPPER_CONTAINER_RPC_HPP

#include <thread>

#include <zmq.hpp>

#include <clipper/datatypes.hpp>

const std::string LOGGING_TAG_CONTAINER = "CONTAINER";

namespace clipper {

namespace container {

template<class T>
class Model {

  // This needs work - we have to understand the translation between one of our 5
  // input data types and R data frames that R models can accept

  // Questions:
  // 1. How do we map input types to R dataframes?
  // 2. How do we invoke an R function with the parsed input dataframe?
  // 3. How do we retrieve the result in C++?
  // 4. How do we parse the result and return it to Clipper?

  std::vector<std::string> predict(const std::vector<T> inputs) {

  }

  InputType get_input_type() {

  }

};

class RPC {
 public:
  explicit RPC();
  ~RPC();
  // TODO(czumar): MOVE AND COPY CONSTRUCTORS

  void start(const std::string &clipper_ip, int clipper_port);
  void stop();

 private:
  void serve_model(const std::string& clipper_address);
  void handle_predict_request(zmq::socket_t &socket) const;
  void handle_heartbeat(zmq::socket_t &socket) const;
  void send_heartbeat(zmq::socket_t &socket) const;
  void send_container_metadata(zmq::socket_t &socket) const;

  std::thread serving_thread_;
  std::atomic_bool active_;
};

} // namespace container

} // namespace clipper
#endif //CLIPPER_CONTAINER_RPC_HPP
