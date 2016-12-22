#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <clipper/datatypes.hpp>
#include <clipper/query_processor.hpp>

#include <server_http.hpp>

using clipper::FeedbackAck;
using clipper::QueryProcessorBase;
using clipper::VersionedModelId;
using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;

enum InputType { integer_vec, double_vec, byte_vec, float_vec };
enum OutputType { double_val, int_val };

class RequestHandler {
 public:
  RequestHandler(QueryProcessorBase& q, int portno, int num_threads,
                 bool debug=false)
      : server(portno, num_threads), qp(q), debug(debug) {}
  RequestHandler(QueryProcessorBase& q, std::string address, int portno,
                 int num_threads, bool debug=false)
      : server(address, portno, num_threads), qp(q), debug(debug) {}

  void add_application(std::string name, std::vector<VersionedModelId> models,
                       InputType input_type, OutputType output_type,
                       std::string policy, long latency);
  void add_endpoint(std::string endpoint, std::string request_method,
                    std::function<void(std::shared_ptr<HttpServer::Response>,
                                       std::shared_ptr<HttpServer::Request>)>
                        endpoint_fn);
  void start_listening();

 private:
  HttpServer server;
  QueryProcessorBase& qp;
  bool debug;
};
