#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <clipper/datatypes.hpp>
#include <clipper/query_processor.hpp>

#include <server_http.hpp>

using clipper::Response;
using clipper::FeedbackAck;
using clipper::QueryProcessorBase;
using clipper::VersionedModelId;
using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;

enum InputType { integer_vec, double_vec, byte_vec, float_vec };
enum OutputType { double_val, int_val };

class RequestHandler {
 public:
  RequestHandler(QueryProcessorBase& q, int portno, int num_threads)
      : server(portno, num_threads), qp(q) {}
  RequestHandler(QueryProcessorBase& q, std::string address, int portno,
                 int num_threads)
      : server(address, portno, num_threads), qp(q) {}

  void add_application(std::string name, std::vector<VersionedModelId> models,
                       InputType input_type, OutputType output_type,
                       std::string policy, long latency);
  void add_endpoint(std::string endpoint, std::string request_method,
                    std::function<void(std::shared_ptr<HttpServer::Response>,
                                       std::shared_ptr<HttpServer::Request>)>
                        endpoint_fn);

  boost::future<Response> decode_and_handle_predict(
    std::string json_content, QueryProcessorBase& q, std::string name,
    std::vector<VersionedModelId> models, std::string policy, long latency,
    InputType input_type);
  boost::future<FeedbackAck> decode_and_handle_update(
    std::string json_content, QueryProcessorBase& q, std::string name,
    std::vector<VersionedModelId> models, std::string policy,
    InputType input_type, OutputType output_type);

  void start_listening();

 private:
  HttpServer server;
  QueryProcessorBase& qp;
};
