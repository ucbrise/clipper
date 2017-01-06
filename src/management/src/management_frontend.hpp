#include <cassert>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <boost/thread.hpp>
#define BOOST_SPIRIT_THREADSAFE
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <redox.hpp>
#include <server_http.hpp>

#include <clipper/config.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/redis.hpp>
#include <clipper/util.hpp>

using namespace boost::property_tree;
using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;
using clipper::VersionedModelId;
using clipper::InputType;

namespace management {

const std::string ADMIN_PATH = "^/admin";
const std::string ADD_APPLICATION = ADMIN_PATH + "/add_app$";
const std::string ADD_MODEL = ADMIN_PATH + "/add_model$";

// const std::string ADD_CONTAINER = ADMIN_PATH + "/add_container$";
const std::string GET_METRICS = ADMIN_PATH + "/metrics$";
const std::string GET_POLICY_STATE = ADMIN_PATH + "/policy_state$";
const std::string GET_APPLICATIONS = ADMIN_PATH + "/applications$";

template <typename T>
std::vector<T> as_vector(ptree const& pt, ptree::key_type const& key) {
  std::vector<T> r;
  for (auto& item : pt.get_child(key)) {
    r.push_back(item.second.get_value<T>());
  }
  return r;
}

/**
 * @param models_node A ptree with a child object named `key` that
 * contains an array of VersionedModelId objects
 * objects.
 */
std::vector<VersionedModelId> parse_candidate_models(
    const ptree& models_node, ptree::key_type const& key) {
  std::vector<VersionedModelId> models;
  for (auto vm_json : models_node.get_child(key)) {
    VersionedModelId cur_vm =
        std::make_pair(vm_json.second.get<std::string>("model_name"),
                       vm_json.second.get<int>("model_version"));
    models.push_back(cur_vm);
  }
  return models;
}

void respond_http(std::string content, std::string message,
                  std::shared_ptr<HttpServer::Response> response) {
  *response << "HTTP/1.1 " << message
            << "\r\nContent-Length: " << content.length() << "\r\n\r\n"
            << content << "\n";
}

class RequestHandler {
 public:
  RequestHandler(int portno, int num_threads) : server_(portno, num_threads) {
    clipper::Config& conf = clipper::get_config();
    while (!redis_connection_.connect(conf.get_redis_address(),
                                      conf.get_redis_port())) {
      std::cout << "ERROR: Management connecting to Redis" << std::endl;
      std::cout << "Sleeping 1 second..." << std::endl;
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    while (!redis_subscriber_.connect(conf.get_redis_address(),
                                      conf.get_redis_port())) {
      std::cout << "ERROR: Management subscriber connecting to Redis"
                << std::endl;
      std::cout << "Sleeping 1 second..." << std::endl;
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    server_.add_endpoint(
        ADD_APPLICATION, "POST",
        [this](std::shared_ptr<HttpServer::Response> response,
               std::shared_ptr<HttpServer::Request> request) {
          try {
            std::string result = add_application(request->content.string());
            respond_http(result, "200 OK", response);
          } catch (const ptree_error& e) {
            respond_http(e.what(), "400 Bad Request", response);
          } catch (const std::invalid_argument& e) {
            respond_http(e.what(), "400 Bad Request", response);
          }
        });
    server_.add_endpoint(
        ADD_MODEL, "POST",
        [this](std::shared_ptr<HttpServer::Response> response,
               std::shared_ptr<HttpServer::Request> request) {
          try {
            std::string result = add_model(request->content.string());
            respond_http(result, "200 OK", response);
          } catch (const ptree_error& e) {
            respond_http(e.what(), "400 Bad Request", response);
          } catch (const std::invalid_argument& e) {
            respond_http(e.what(), "400 Bad Request", response);
          }
        });
  }

  ~RequestHandler() {
    redis_subscriber_.disconnect();
    redis_connection_.disconnect();
  }

  /**
   * Creates an endpoint that listens for requests to add new prediction
   * applications to Clipper.
   *
   * JSON format:
   * {
   *  "name" := string,
   *  "candidate_models" := [{"model_name" := string, "model_version" := int}],
   *  "input_type" := "integers" | "bytes" | "floats" | "doubles" | "strings",
   *  "output_type" := "double" | "int",
   *  "selection_policy" := string,
   *  "latency_slo_micros" := int
   * }
   */
  std::string add_application(const std::string& json) {
    std::istringstream is(json);
    ptree pt;
    read_json(is, pt);
    std::string app_name = pt.get<std::string>("name");
    std::vector<VersionedModelId> candidate_models =
        parse_candidate_models(pt, "candidate_models");
    InputType input_type =
        clipper::parse_input_type(pt.get<std::string>("input_type"));
    std::string output_type = pt.get<std::string>("output_type");
    if (!(output_type == "int" || output_type == "double")) {
      throw std::invalid_argument(output_type + " is invalid output type");
    }
    std::string selection_policy = pt.get<std::string>("selection_policy");
    int latency_slo_micros = pt.get<int>("latency_slo_micros");
    if (clipper::redis::add_application(
            redis_connection_, app_name, candidate_models, input_type,
            output_type, selection_policy, latency_slo_micros)) {
      return "Success!";
    } else {
      return "Error adding application to Redis.";
    }
  }

  /**
   * Creates an endpoint that listens for requests to add new models to Clipper.
   *
   * JSON format:
   * {
   *  "model_name" := string,
   *  "model_version" := int,
   *  "labels" := [string]
   *  "input_type" := "integers" | "bytes" | "floats" | "doubles" | "strings",
   *  "output_type" := "double" | "int",
   * }
   */
  std::string add_model(const std::string& json) {
    std::istringstream is(json);
    ptree pt;
    read_json(is, pt);

    VersionedModelId model_id = std::make_pair(
        pt.get<std::string>("model_name"), pt.get<int>("model_version"));
    std::vector<std::string> labels = as_vector<std::string>(pt, "labels");
    InputType input_type =
        clipper::parse_input_type(pt.get<std::string>("input_type"));
    std::string output_type = pt.get<std::string>("output_type");
    if (!(output_type == "int" || output_type == "double")) {
      throw std::invalid_argument(output_type + " is invalid output type");
    }
    if (clipper::redis::add_model(redis_connection_, model_id, input_type,
                                  output_type, labels)) {
      return "Success!";
    } else {
      return "Error adding model to Redis.";
    }
  }

  void start_listening() {
    // std::thread server_thread([this]() { server_.start(); });
    server_.start();

    // server_thread.join();
  }

 private:
  HttpServer server_;
  redox::Redox redis_connection_;
  redox::Subscriber redis_subscriber_;
};

}  // namespace management
