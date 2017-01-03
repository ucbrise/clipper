#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <boost/thread.hpp>
#define BOOST_SPIRIT_THREADSAFE
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <clipper/datatypes.hpp>
#include <clipper/query_processor.hpp>

#include <server_http.hpp>

using namespace boost::property_tree;
using clipper::Response;
using clipper::FeedbackAck;
using clipper::VersionedModelId;
using clipper::InputType;
using clipper::OutputType;
using clipper::Input;
using clipper::Output;
using clipper::Query;
using clipper::FeedbackQuery;
using clipper_json::json_parse_error;
using clipper_json::json_semantic_error;
using clipper_json::decode_input;
using clipper_json::decode_output;
using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;

namespace query_frontend {

void respond_http(std::string content, std::string message,
                  std::shared_ptr<HttpServer::Response> response) {
  *response << "HTTP/1.1 " << message
            << "\r\nContent-Length: " << content.length() << "\r\n\r\n"
            << content << "\n";
}

template <class QP>
class RequestHandler {
 public:
  RequestHandler(int portno, int num_threads)
      : server_(portno, num_threads), query_processor_() {}
  RequestHandler(std::string address, int portno, int num_threads)
      : server_(address, portno, num_threads), query_processor_() {}

  void add_application(std::string name, std::vector<VersionedModelId> models,
                       InputType input_type, OutputType output_type,
                       std::string policy, long latency) {
    auto predict_fn = [this, name, input_type, policy, latency, models](
        std::shared_ptr<HttpServer::Response> response,
        std::shared_ptr<HttpServer::Request> request) {
      try {
        auto prediction =
            decode_and_handle_predict(request->content.string(), name, models,
                                      policy, latency, input_type);
        prediction.then([response](boost::future<Response> f) {
          Response r = f.get();
          std::stringstream ss;
          ss << "qid:" << r.query_id_ << " predict:" << r.output_.y_hat_;
          std::string content = ss.str();
          respond_http(content, "200 OK", response);
        });
      } catch (const json_parse_error& e) {
        respond_http(e.what(), "400 Bad Request", response);
      } catch (const json_semantic_error& e) {
        respond_http(e.what(), "400 Bad Request", response);
      } catch (const std::invalid_argument& e) {
        respond_http(e.what(), "400 Bad Request", response);
      }
    };
    std::string predict_endpoint = "^/" + name + "/predict$";
    server_.add_endpoint(predict_endpoint, "POST", predict_fn);

    auto update_fn = [this, name, input_type, output_type, policy, models](
        std::shared_ptr<HttpServer::Response> response,
        std::shared_ptr<HttpServer::Request> request) {
      try {
        auto update =
            decode_and_handle_update(request->content.string(), name, models,
                                     policy, input_type, output_type);
        update.then([response](boost::future<FeedbackAck> f) {
          FeedbackAck ack = f.get();
          std::stringstream ss;
          ss << "Feedback received? " << ack;
          std::string content = ss.str();
          respond_http(content, "200 OK", response);
        });
      } catch (const json_parse_error& e) {
        respond_http(e.what(), "400 Bad Request", response);
      } catch (const json_semantic_error& e) {
        respond_http(e.what(), "400 Bad Request", response);
      } catch (const std::invalid_argument& e) {
        respond_http(e.what(), "400 Bad Request", response);
      }
    };
    std::string update_endpoint = "^/" + name + "/update$";
    server_.add_endpoint(update_endpoint, "POST", update_fn);
  }

  /* Predict JSON format:
   * {"uid": <user id>, "input": <query input>}
   */
  boost::future<Response> decode_and_handle_predict(
      std::string json_content, std::string name,
      std::vector<VersionedModelId> models, std::string policy, long latency,
      InputType input_type) {
    std::istringstream is(json_content);
    ptree pt;
    try {
      read_json(is, pt);
    } catch (const ptree_error& e) {
      throw json_parse_error(e.what());
    }

    try {
      long uid = pt.get<long>("uid");
      std::shared_ptr<Input> input = decode_input(input_type, pt);
      auto prediction = query_processor_.predict(
          Query{name, uid, input, latency, policy, models});
      return prediction;
    } catch (const ptree_error& e) {
      throw json_semantic_error(e.what());
    }
  }

  /* Update JSON format:
   * {"uid": <user id>, "input": <query input>, "label": <query y_hat>,
   *  "model_name": <model name>, "model_version": <model_version>}
   */
  boost::future<FeedbackAck> decode_and_handle_update(
      std::string json_content, std::string name,
      std::vector<VersionedModelId> models, std::string policy,
      InputType input_type, OutputType output_type) {
    std::istringstream is(json_content);
    ptree pt;
    try {
      read_json(is, pt);
    } catch (const ptree_error& e) {
      throw json_parse_error(e.what());
    }

    try {
      long uid = pt.get<long>("uid");
      std::shared_ptr<Input> input = decode_input(input_type, pt);
      Output output = decode_output(output_type, pt);
      auto update = query_processor_.update(FeedbackQuery{
          name, uid, {std::make_pair(input, output)}, policy, models});
      return update;
    } catch (const ptree_error& e) {
      throw json_semantic_error(e.what());
    }
  }

  void start_listening() {
    HttpServer& s = server_;
    std::thread server_thread([&s]() { s.start(); });

    server_thread.join();
  }

  size_t num_applications() {
    size_t count = server_.num_endpoints();
    assert(count % 2 == 0);
    return count / 2;
  }

 private:
  HttpServer server_;
  QP query_processor_;
};

}  // namespace query_frontend
