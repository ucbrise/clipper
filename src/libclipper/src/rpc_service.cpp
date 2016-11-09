#include <rpc_service.hpp>
#include <boost/thread.hpp>
#include <zmq.hpp>

using zmq::socket_t
using zmq::message_t;
using zmq::context_t;

namespace clipper {

RPCService::RPCService(shared_ptr <Queue<RPCResponse>> response_queue) {

}

RPCService::~RPCService() {

}

void RPCService::start(const std::string ip, const int port) {
  const std::string address = "tcp://" + ip + std::to_string(port);
  // TODO: Propagate errors from new child thread for handling
  boost::thread(boost::bind(&RPCService::manage_service,
                            this,
                            address,
                            send_queue_,
                            response_queue_,
                            boost::ref(shutdown_)));
}

void RPCService::stop() {
  shutdown_ = true;
}

const int RPCService::send_message(const std::vector<uint8_t> &msg, const int container_id) {
  return 0;
}

void RPCService::manage_service(const std::string address,
                                shared_ptr <Queue<RPCRequest>> send_queue,
                                shared_ptr <Queue<RPCResponse>> response_queue,
                                const bool &shutdown) {
  context_t context = context_t(1);
  socket_t socket = socket_t(context, ZMQ_ROUTER);
  socket.bind(address);

}

} // namespace clipper