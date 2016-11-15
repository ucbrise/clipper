#include <rpc_service.hpp>
#include <boost/thread.hpp>
#include <boost/bimap.hpp>

using zmq::socket_t;
using zmq::message_t;
using zmq::context_t;
using std::shared_ptr;
using std::string;
using std::vector;

namespace clipper {

RPCService::RPCService(shared_ptr<std::queue<RPCResponse>> response_queue, shared_ptr<std::mutex> response_lock) :
    request_queue_(std::make_shared<std::queue<RPCRequest>>()),
    response_queue_(response_queue),
    request_lock_(std::make_shared<std::mutex>()),
    response_lock_(response_lock) {
}

RPCService::~RPCService() {
  stop();
}

void RPCService::start(const string ip, const int port) {
  const string address = "tcp://" + ip + ":" + std::to_string(port);
  // TODO: Propagate errors from new child thread for handling
  boost::thread(boost::bind(&RPCService::manage_service,
                            this,
                            address,
                            request_queue_,
                            response_queue_,
                            request_lock_,
                            response_lock_,
                            boost::ref(shutdown_))).detach();
}

void RPCService::stop() {
  shutdown_ = true;
}

int RPCService::send_message(const std::vector<uint8_t> &msg, const int container_id) {
  request_lock_->lock();
  int id = message_id_++;
  RPCRequest request(container_id, id, msg.data(), msg.size());
  request_queue_->push(request);
  request_lock_->unlock();
  return id;
}

void RPCService::manage_service(const string address,
                                shared_ptr<std::queue<RPCRequest>> request_queue,
                                shared_ptr<std::queue<RPCResponse>> response_queue,
                                shared_ptr<std::mutex> request_lock,
                                shared_ptr<std::mutex> response_lock,
                                const bool &shutdown) {
  boost::bimap<int, vector<uint8_t>> connections;
  context_t context = context_t(1);
  socket_t socket = socket_t(context, ZMQ_ROUTER);
  socket.bind(address);
  zmq::pollitem_t items[] = {
      {socket, 0, ZMQ_POLLIN, 0}
  };
  int container_id = 0;
  while (true) {
    if (shutdown) {
      shutdown_service(address, socket);
      return;
    }
    zmq_poll(items, 1, 0);
    if (items[0].revents & ZMQ_POLLIN) {
      receive_message(socket, response_queue, response_lock, connections, container_id);
    }
    send_messages(socket, request_queue, request_lock, connections);
  }
}

void RPCService::shutdown_service(const string address, socket_t &socket) {
  socket.disconnect(address);
  socket.close();
}

void RPCService::send_messages(socket_t &socket,
                               shared_ptr<std::queue<RPCRequest>> request_queue,
                               shared_ptr<std::mutex> request_lock,
                               boost::bimap<int, vector<uint8_t>> &connections) {
  request_lock->lock();
  while (!request_queue->empty()) {
    RPCRequest request = request_queue->front();
    boost::bimap<int, vector<uint8_t>>::left_const_iterator connection = connections.left.find(std::get<0>(request));
    if (connection == connections.left.end()) {
      // Error handling
      continue;
    }
    message_t id_message(sizeof(int));
    memcpy(id_message.data(), &std::get<1>(request), sizeof(int));
    socket.send(connection->second.data(), connection->second.size(), ZMQ_SNDMORE);
    socket.send("", 0, ZMQ_SNDMORE);
    socket.send(id_message, ZMQ_SNDMORE);
    socket.send((uint8_t *) std::get<2>(request), std::get<3>(request), 0);
    request_queue->pop();
    // Send message to destination with the same socket (container) identity
  }
  request_lock->unlock();
}

void RPCService::receive_message(socket_t &socket,
                                 shared_ptr<std::queue<RPCResponse>> response_queue,
                                 shared_ptr<std::mutex> response_lock,
                                 boost::bimap<int, vector<uint8_t>> &connections,
                                 int &container_id) {
  message_t msg_identity;
  message_t msg_delimiter;
  message_t msg_id;
  message_t msg_content;
  socket.recv(&msg_identity, 0);
  socket.recv(&msg_delimiter, 0);
  socket.recv(&msg_id, 0);
  socket.recv(&msg_content, 0);

  vector<uint8_t> connection_id((uint8_t *) msg_identity.data(), (uint8_t *) msg_identity.data() + msg_identity.size());
  boost::bimap<int, vector<uint8_t>>::right_const_iterator connection = connections.right.find(connection_id);
  if (connection == connections.right.end()) {
    // We have a new connection, process it accordingly
    connections.insert(boost::bimap<int, vector<uint8_t>>::value_type(container_id, connection_id));
    container_id++;
  } else {
    response_lock->lock();
    int id = ((int *) msg_id.data())[0];
    vector<uint8_t> content((uint8_t *) msg_content.data(), (uint8_t *) msg_content.data() + msg_content.size());
    RPCResponse response(id, content);
    response_queue->push(response);
    response_lock->unlock();
  }
}

} // namespace clipper