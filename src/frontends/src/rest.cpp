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
#include <frontends/rest.hpp>

#include <server_http.hpp>

using namespace boost::property_tree;
using clipper::DoubleVector;
using clipper::FeedbackAck;
using clipper::Input;
using clipper::Output;
using clipper::QueryProcessor;
using clipper::Response;
using clipper::VersionedModelId;
using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;

template <typename T>
std::vector<T> as_vector(ptree const& pt, ptree::key_type const& key) {
  std::vector<T> r;
  for (auto& item : pt.get_child(key)) r.push_back(item.second.get_value<T>());
  return r;
}

std::shared_ptr<Input> decode_input(InputType input_type, ptree& parsed_json) {
  std::shared_ptr<Input> result;
  switch (input_type) {
    case double_vec: {
      std::vector<double> inputs = as_vector<double>(parsed_json, "input");
      return std::make_shared<DoubleVector>(inputs);
    }
    default:
      throw std::invalid_argument("input_type is not a valid type");
  }
}

Output decode_output(OutputType output_type, ptree parsed_json) {
  std::string model_name = parsed_json.get<std::string>("model_name");
  int model_version = parsed_json.get<int>("model_version");
  VersionedModelId versioned_model = std::make_pair(model_name, model_version);
  switch (output_type) {
    case double_val: {
      double y_hat = parsed_json.get<double>("label");
      return Output(y_hat, versioned_model);
    }
    default:
      throw std::invalid_argument("output_type is not a valid type");
  }
}

void RequestHandler::add_application(std::string name, std::vector<VersionedModelId> models,
                       InputType input_type, OutputType output_type,
                       std::string policy, long latency) {
  QueryProcessor& q = qp;
  auto predict_fn = [&q, name, input_type, policy, latency, models](
      std::shared_ptr<HttpServer::Response> response,
      std::shared_ptr<HttpServer::Request> request) {
    try {
      ptree pt;
      read_json(request->content, pt);

      long uid = pt.get<long>("uid");
      std::shared_ptr<Input> input = decode_input(input_type, pt);
      auto prediction =
          q.predict({name, uid, input, latency, policy, models});
      prediction.then([response](boost::future<Response> f) {
        Response r = f.get();
        std::stringstream ss;
        ss << "qid:" << r.query_id_ << " predict:" << r.output_.y_hat_;
        std::string content = ss.str();
        *response << "HTTP/1.1 200 OK\r\nContent-Length: " << content.length()
                  << "\r\n\r\n"
                  << content << "\n";
      });
    } catch (const ptree_error& e) {
      *response << "HTTP/1.1 200 OK\r\nContent-Length: "
                << std::strlen(e.what()) << "\r\n\r\n"
                << e.what() << "\n";
    } catch (const std::invalid_argument& e) {
      *response << "HTTP/1.1 200 OK\r\nContent-Length: "
                << std::strlen(e.what()) << "\r\n\r\n"
                << e.what() << "\n";
    }
  };
  std::string predict_endpoint = "^/" + name + "/predict$";
  server.add_endpoint(predict_endpoint, "POST", predict_fn);
  std::cout << "added endpoint " + predict_endpoint + " for " + name + "\n";

  auto update_fn = [&q, name, input_type, output_type, policy, latency,
                    models](std::shared_ptr<HttpServer::Response> response,
                            std::shared_ptr<HttpServer::Request> request) {
    try {
      ptree pt;
      read_json(request->content, pt);

      long uid = pt.get<long>("uid");
      std::shared_ptr<Input> input = decode_input(input_type, pt);
      Output output = decode_output(output_type, pt);
      auto update = q.update(
          {name, uid, {std::make_pair(input, output)}, policy, models});
      update.then([response](boost::future<FeedbackAck> f) {
        FeedbackAck ack = f.get();
        std::stringstream ss;
        ss << "Feedback received? " << ack;
        std::string content = ss.str();
        *response << "HTTP/1.1 200 OK\r\nContent-Length: " << content.length()
                  << "\r\n\r\n"
                  << content << "\n";
      });
    } catch (const ptree_error& e) {
      *response << "HTTP/1.1 200 OK\r\nContent-Length: "
                << std::strlen(e.what()) << "\r\n\r\n"
                << e.what() << "\n";
    } catch (const std::invalid_argument& e) {
      *response << "HTTP/1.1 200 OK\r\nContent-Length: "
                << std::strlen(e.what()) << "\r\n\r\n"
                << e.what() << "\n";
    }
  };
  std::string update_endpoint = "^/" + name + "/update$";
  server.add_endpoint(update_endpoint, "POST", update_fn);
  std::cout << "added endpoint " + update_endpoint + " for " + name + "\n";
}

void RequestHandler::add_endpoint(std::string endpoint, std::string request_method,
                  std::function<void(std::shared_ptr<HttpServer::Response>,
                                     std::shared_ptr<HttpServer::Request>)>
                      endpoint_fn) {
  server.resource[endpoint][request_method] = endpoint_fn;
  std::cout << "added " + endpoint + "\n";
}

void RequestHandler::start_listening() {
  HttpServer& s = server;
  std::thread server_thread([&s]() { s.start(); });

  server_thread.join();
}

int main() {
  QueryProcessor qp;
  RequestHandler rh(qp, "0.0.0.0", 1337, 1);
  rh.start_listening();
}
