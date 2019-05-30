#include <cassert>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <boost/optional.hpp>
#include <redox.hpp>
#include <server_http.hpp>

#include "rapidjson/document.h"

#include <clipper/config.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/exceptions.hpp>
#include <clipper/json_util.hpp>
#include <clipper/logging.hpp>
#include <clipper/persistent_state.hpp>
#include <clipper/redis.hpp>
#include <clipper/selection_policies.hpp>
#include <clipper/util.hpp>

using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;
using clipper::InputType;
using clipper::VersionedModelId;
using clipper::json::add_bool;
using clipper::json::add_string;
using clipper::json::add_string_array;
using clipper::json::get_bool;
using clipper::json::get_candidate_models;
using clipper::json::get_int;
using clipper::json::get_string;
using clipper::json::get_string_array;
using clipper::json::json_parse_error;
using clipper::json::json_semantic_error;
using clipper::json::parse_json;
using clipper::json::redis_app_metadata_to_json;
using clipper::json::redis_container_metadata_to_json;
using clipper::json::redis_model_metadata_to_json;
using clipper::json::set_string_array;
using clipper::json::to_json_string;
using clipper::redis::prohibited_group_strings;

namespace management {

const std::string LOGGING_TAG_MANAGEMENT_FRONTEND = "MGMTFRNTD";

const std::string ADMIN_PATH = "^/admin";
const std::string ADD_APPLICATION = ADMIN_PATH + "/add_app$";
const std::string DELETE_APPLICATION = ADMIN_PATH + "/delete_app$";
const std::string ADD_MODEL_LINKS = ADMIN_PATH + "/add_model_links$";
const std::string ADD_MODEL = ADMIN_PATH + "/add_model$";
const std::string DELETE_VERSIONED_MODEL = ADMIN_PATH + "/delete_versioned_model";
const std::string SET_MODEL_VERSION = ADMIN_PATH + "/set_model_version$";

// const std::string ADD_CONTAINER = ADMIN_PATH + "/add_container$";
const std::string GET_METRICS = ADMIN_PATH + "/metrics$";
const std::string GET_SELECTION_STATE = ADMIN_PATH + "/get_state$";
const std::string GET_ALL_APPLICATIONS = ADMIN_PATH + "/get_all_applications$";
const std::string GET_APPLICATION = ADMIN_PATH + "/get_application$";
const std::string GET_LINKED_MODELS = ADMIN_PATH + "/get_linked_models";
const std::string GET_ALL_MODELS = ADMIN_PATH + "/get_all_models$";
const std::string GET_MODEL = ADMIN_PATH + "/get_model$";
const std::string GET_ALL_CONTAINERS = ADMIN_PATH + "/get_all_containers$";
const std::string GET_CONTAINER = ADMIN_PATH + "/get_container$";
const std::string DELETE_MODEL_LINKS = ADMIN_PATH + "/delete_model_links$";

const std::string PING = ADMIN_PATH + "/ping$";

const std::string ADD_APPLICATION_JSON_SCHEMA = R"(
  {
   "name" := string,
   "input_type" := "integers" | "bytes" | "floats" | "doubles" | "strings",
   "default_output" := string,
   "latency_slo_micros" := int
  }
)";

const std::string ADD_MODEL_LINKS_JSON_SCHEMA = R"(
  {
    "app_name" := string,
    "model_names" := [string]
  }
)";

const std::string DELETE_MODEL_LINKS_JSON_SCHEMA = R"(
  {
  "app_name" := string,
  "model_names" := [string]
  }
)";

const std::string GET_LINKED_MODELS_REQUESTS_SCHEMA = R"(
  {
    "app_name" := string
  }
)";

const std::string VERBOSE_OPTION_JSON_SCHEMA = R"(
  {
    "verbose" := bool
  }
)";

const std::string GET_APPLICATION_REQUESTS_SCHEMA = R"(
  {
    "name" := string
  }
)";

const std::string GET_MODEL_REQUESTS_SCHEMA = R"(
  {
    "model_name" := string,
    "model_version" := string
  }
)";

const std::string GET_CONTAINER_REQUESTS_SCHEMA = R"(
  {
    "model_name" := string,
    "model_version" := string,
    "replica_id" := int
  }
)";

const std::string ADD_MODEL_JSON_SCHEMA = R"(
  {
   "model_name" := string,
   "model_version" := string,
   "labels" := [string],
   "input_type" := "integers" | "bytes" | "floats" | "doubles" | "strings",
   "container_name" := string,
   "model_data_path" := string
  }
)";

const std::string DELETE_VERSIONED_MODEL_JSON_SCHEMA = R"(
  {
   "model_name" := string,
   "model_version" := string,
  }
)";

const std::string SET_VERSION_JSON_SCHEMA = R"(
  {
   "model_name" := string,
   "model_version" := string,
  }
)";

const std::string SELECTION_JSON_SCHEMA = R"(
  {
   "app_name" := string,
   "uid" := int,
  }
)";

void respond_http(std::string content, std::string message,
                  std::shared_ptr<HttpServer::Response> response) {
  *response << "HTTP/1.1 " << message << "\r\nContent-Type: application/json"
            << "\r\nContent-Length: " << content.length() << "\r\n\r\n"
            << content;
}

/* Generate a user-facing error message containing the exception
 * content and the expected JSON schema. */
std::string json_error_msg(const std::string& exception_msg,
                           const std::string& expected_schema) {
  std::stringstream ss;
  ss << "Error parsing JSON: " << exception_msg << ". "
     << "Expected JSON schema: " << expected_schema;
  return ss.str();
}

class RequestHandler {
 public:
  RequestHandler(std::string address, int portno)
      : server_(address, portno), state_db_{} {
    clipper::Config& conf = clipper::get_config();
    while (!redis_connection_.connect(conf.get_redis_address(),
                                      conf.get_redis_port())) {
      clipper::log_error(LOGGING_TAG_MANAGEMENT_FRONTEND,
                         "Management frontend failed to connect to Redis",
                         "Retrying in 1 second...");
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    while (!redis_subscriber_.connect(conf.get_redis_address(),
                                      conf.get_redis_port())) {
      clipper::log_error(
          LOGGING_TAG_MANAGEMENT_FRONTEND,
          "Management frontend subscriber failed to connect to Redis",
          "Retrying in 1 second...");
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    server_.add_endpoint(
        ADD_APPLICATION, "POST",
        [this](std::shared_ptr<HttpServer::Response> response,
               std::shared_ptr<HttpServer::Request> request) {
          try {
            clipper::log_info(LOGGING_TAG_MANAGEMENT_FRONTEND,
                              "Add application POST request");
            std::string result = add_application(request->content.string());
            respond_http(result, "200 OK", response);
          } catch (const json_parse_error& e) {
            std::string err_msg =
                json_error_msg(e.what(), ADD_APPLICATION_JSON_SCHEMA);
            respond_http(err_msg, "400 Bad Request", response);
          } catch (const json_semantic_error& e) {
            std::string err_msg =
                json_error_msg(e.what(), ADD_APPLICATION_JSON_SCHEMA);
            respond_http(err_msg, "400 Bad Request", response);
          } catch (const clipper::ManagementOperationError& e) {
            respond_http(e.what(), "400 Bad Request", response);
          }
        });
    server_.add_endpoint(
        DELETE_APPLICATION, "POST",
        [this](std::shared_ptr<HttpServer::Response> response,
               std::shared_ptr<HttpServer::Request> request) {
          try {
            clipper::log_info(LOGGING_TAG_MANAGEMENT_FRONTEND,
                              "Delete application POST request");
            std::string result = delete_application(request->content.string());
            respond_http(result, "200 OK", response);
          } catch (const json_parse_error& e) {
            std::string err_msg =
                json_error_msg(e.what(), ADD_APPLICATION_JSON_SCHEMA);
            respond_http(err_msg, "400 Bad Request", response);
          } catch (const json_semantic_error& e) {
            std::string err_msg =
                json_error_msg(e.what(), ADD_APPLICATION_JSON_SCHEMA);
            respond_http(err_msg, "400 Bad Request", response);
          } catch (const clipper::ManagementOperationError& e) {
            respond_http(e.what(), "400 Bad Request", response);
          }
        });
    server_.add_endpoint(
        ADD_MODEL_LINKS, "POST",
        [this](std::shared_ptr<HttpServer::Response> response,
               std::shared_ptr<HttpServer::Request> request) {
          try {
            clipper::log_info(LOGGING_TAG_MANAGEMENT_FRONTEND,
                              "Add application links POST request");
            std::string result = add_model_links(request->content.string());
            respond_http(result, "200 OK", response);
          } catch (const json_parse_error& e) {
            std::string err_msg =
                json_error_msg(e.what(), ADD_MODEL_LINKS_JSON_SCHEMA);
            respond_http(err_msg, "400 Bad Request", response);
          } catch (const json_semantic_error& e) {
            std::string err_msg =
                json_error_msg(e.what(), ADD_MODEL_LINKS_JSON_SCHEMA);
            respond_http(err_msg, "400 Bad Request", response);
          } catch (const clipper::ManagementOperationError& e) {
            respond_http(e.what(), "400 Bad Request", response);
          }
        });
    server_.add_endpoint(
        DELETE_MODEL_LINKS, "POST",
        [this](std::shared_ptr<HttpServer::Response> response,
               std::shared_ptr<HttpServer::Request> request) {
          try {
            clipper::log_info(LOGGING_TAG_MANAGEMENT_FRONTEND,
                              "Remove application links POST request");
            std::string result = delete_model_links(request->content.string());
            respond_http(result, "200 OK", response);
          } catch (const json_parse_error& e) {
            std::string err_msg =
                json_error_msg(e.what(), DELETE_MODEL_LINKS_JSON_SCHEMA);
            respond_http(err_msg, "400 Bad Request", response);
          } catch (const json_semantic_error& e) {
            std::string err_msg =
                json_error_msg(e.what(), DELETE_MODEL_LINKS_JSON_SCHEMA);
            respond_http(err_msg, "400 Bad Request", response);
          } catch (const std::invalid_argument& e) {
            respond_http(e.what(), "400 Bad Request", response);
          }
        });
    server_.add_endpoint(
        ADD_MODEL, "POST",
        [this](std::shared_ptr<HttpServer::Response> response,
               std::shared_ptr<HttpServer::Request> request) {
          try {
            clipper::log_info(LOGGING_TAG_MANAGEMENT_FRONTEND,
                              "Add model POST request");
            std::string result = add_model(request->content.string());
            respond_http(result, "200 OK", response);
          } catch (const json_parse_error& e) {
            std::string err_msg =
                json_error_msg(e.what(), ADD_MODEL_JSON_SCHEMA);
            respond_http(err_msg, "400 Bad Request", response);
          } catch (const json_semantic_error& e) {
            std::string err_msg =
                json_error_msg(e.what(), ADD_MODEL_JSON_SCHEMA);
            respond_http(err_msg, "400 Bad Request", response);
          } catch (const clipper::ManagementOperationError& e) {
            respond_http(e.what(), "400 Bad Request", response);
          }
        });
    server_.add_endpoint(
        SET_MODEL_VERSION, "POST",
        [this](std::shared_ptr<HttpServer::Response> response,
               std::shared_ptr<HttpServer::Request> request) {
          try {
            clipper::log_info(LOGGING_TAG_MANAGEMENT_FRONTEND,
                              "Set model version POST request");
            std::string result = set_model_version(request->content.string());
            respond_http(result, "200 OK", response);
          } catch (const json_parse_error& e) {
            std::string err_msg =
                json_error_msg(e.what(), SET_VERSION_JSON_SCHEMA);
            respond_http(err_msg, "400 Bad Request", response);
          } catch (const json_semantic_error& e) {
            std::string err_msg =
                json_error_msg(e.what(), SET_VERSION_JSON_SCHEMA);
            respond_http(err_msg, "400 Bad Request", response);
          } catch (const clipper::ManagementOperationError& e) {
            respond_http(e.what(), "400 Bad Request", response);
          }
        });
    server_.add_endpoint(
        GET_ALL_APPLICATIONS, "POST",
        [this](std::shared_ptr<HttpServer::Response> response,
               std::shared_ptr<HttpServer::Request> request) {
          try {
            clipper::log_info(LOGGING_TAG_MANAGEMENT_FRONTEND,
                              "Get all applications POST request");
            std::string result =
                get_all_applications(request->content.string());
            respond_http(result, "200 OK", response);
          } catch (const json_parse_error& e) {
            std::string err_msg =
                json_error_msg(e.what(), VERBOSE_OPTION_JSON_SCHEMA);
            respond_http(err_msg, "400 Bad Request", response);
          } catch (const json_semantic_error& e) {
            std::string err_msg =
                json_error_msg(e.what(), VERBOSE_OPTION_JSON_SCHEMA);
            respond_http(err_msg, "400 Bad Request", response);
          } catch (const clipper::ManagementOperationError& e) {
            respond_http(e.what(), "400 Bad Request", response);
          }
        });
    server_.add_endpoint(
        GET_APPLICATION, "POST",
        [this](std::shared_ptr<HttpServer::Response> response,
               std::shared_ptr<HttpServer::Request> request) {
          try {
            clipper::log_info(LOGGING_TAG_MANAGEMENT_FRONTEND,
                              "Get application info POST request");
            std::string result = get_application(request->content.string());
            respond_http(result, "200 OK", response);
          } catch (const json_parse_error& e) {
            std::string err_msg =
                json_error_msg(e.what(), GET_APPLICATION_REQUESTS_SCHEMA);
            respond_http(err_msg, "400 Bad Request", response);
          } catch (const json_semantic_error& e) {
            std::string err_msg =
                json_error_msg(e.what(), GET_APPLICATION_REQUESTS_SCHEMA);
            respond_http(err_msg, "400 Bad Request", response);
          } catch (const clipper::ManagementOperationError& e) {
            respond_http(e.what(), "400 Bad Request", response);
          }
        });
    server_.add_endpoint(
        GET_LINKED_MODELS, "POST",
        [this](std::shared_ptr<HttpServer::Response> response,
               std::shared_ptr<HttpServer::Request> request) {
          try {
            clipper::log_info(LOGGING_TAG_MANAGEMENT_FRONTEND,
                              "Get application links POST request");
            std::string result = get_linked_models(request->content.string());
            respond_http(result, "200 OK", response);
          } catch (const json_parse_error& e) {
            std::string err_msg =
                json_error_msg(e.what(), GET_LINKED_MODELS_REQUESTS_SCHEMA);
            respond_http(err_msg, "400 Bad Request", response);
          } catch (const json_semantic_error& e) {
            std::string err_msg =
                json_error_msg(e.what(), GET_LINKED_MODELS_REQUESTS_SCHEMA);
            respond_http(err_msg, "400 Bad Request", response);
          } catch (const clipper::ManagementOperationError& e) {
            respond_http(e.what(), "400 Bad Request", response);
          }
        });
    server_.add_endpoint(
        GET_ALL_MODELS, "POST",
        [this](std::shared_ptr<HttpServer::Response> response,
               std::shared_ptr<HttpServer::Request> request) {
          try {
            clipper::log_info(LOGGING_TAG_MANAGEMENT_FRONTEND,
                              "Get all models POST request");
            std::string result = get_all_models(request->content.string());
            respond_http(result, "200 OK", response);
          } catch (const json_parse_error& e) {
            std::string err_msg =
                json_error_msg(e.what(), VERBOSE_OPTION_JSON_SCHEMA);
            respond_http(err_msg, "400 Bad Request", response);
          } catch (const json_semantic_error& e) {
            std::string err_msg =
                json_error_msg(e.what(), VERBOSE_OPTION_JSON_SCHEMA);
            respond_http(err_msg, "400 Bad Request", response);
          } catch (const clipper::ManagementOperationError& e) {
            respond_http(e.what(), "400 Bad Request", response);
          }
        });
    server_.add_endpoint(
        GET_MODEL, "POST",
        [this](std::shared_ptr<HttpServer::Response> response,
               std::shared_ptr<HttpServer::Request> request) {
          try {
            clipper::log_info(LOGGING_TAG_MANAGEMENT_FRONTEND,
                              "Get model info POST request");
            std::string result = get_model(request->content.string());
            respond_http(result, "200 OK", response);
          } catch (const json_parse_error& e) {
            std::string err_msg =
                json_error_msg(e.what(), GET_MODEL_REQUESTS_SCHEMA);
            respond_http(err_msg, "400 Bad Request", response);
          } catch (const json_semantic_error& e) {
            std::string err_msg =
                json_error_msg(e.what(), GET_MODEL_REQUESTS_SCHEMA);
            respond_http(err_msg, "400 Bad Request", response);
          } catch (const clipper::ManagementOperationError& e) {
            respond_http(e.what(), "400 Bad Request", response);
          }
        });
    server_.add_endpoint(
        GET_ALL_CONTAINERS, "POST",
        [this](std::shared_ptr<HttpServer::Response> response,
               std::shared_ptr<HttpServer::Request> request) {
          try {
            clipper::log_info(LOGGING_TAG_MANAGEMENT_FRONTEND,
                              "Get all containers POST request");
            std::string result = get_all_containers(request->content.string());
            respond_http(result, "200 OK", response);
          } catch (const json_parse_error& e) {
            std::string err_msg =
                json_error_msg(e.what(), VERBOSE_OPTION_JSON_SCHEMA);
            respond_http(err_msg, "400 Bad Request", response);
          } catch (const json_semantic_error& e) {
            std::string err_msg =
                json_error_msg(e.what(), VERBOSE_OPTION_JSON_SCHEMA);
            respond_http(err_msg, "400 Bad Request", response);
          } catch (const clipper::ManagementOperationError& e) {
            respond_http(e.what(), "400 Bad Request", response);
          }
        });
    server_.add_endpoint(
        GET_CONTAINER, "POST",
        [this](std::shared_ptr<HttpServer::Response> response,
               std::shared_ptr<HttpServer::Request> request) {
          try {
            clipper::log_info(LOGGING_TAG_MANAGEMENT_FRONTEND,
                              "Get model info POST request");
            std::string result = get_container(request->content.string());
            respond_http(result, "200 OK", response);
          } catch (const json_parse_error& e) {
            std::string err_msg =
                json_error_msg(e.what(), GET_CONTAINER_REQUESTS_SCHEMA);
            respond_http(err_msg, "400 Bad Request", response);
          } catch (const json_semantic_error& e) {
            std::string err_msg =
                json_error_msg(e.what(), GET_CONTAINER_REQUESTS_SCHEMA);
            respond_http(err_msg, "400 Bad Request", response);
          } catch (const clipper::ManagementOperationError& e) {
            respond_http(e.what(), "400 Bad Request", response);
          }
        });
    server_.add_endpoint(
        GET_SELECTION_STATE, "POST",
        [this](std::shared_ptr<HttpServer::Response> response,
               std::shared_ptr<HttpServer::Request> request) {
          try {
            clipper::log_info(LOGGING_TAG_MANAGEMENT_FRONTEND,
                              "Get selection state POST request");
            std::string result = get_selection_state(request->content.string());
            respond_http(result, "200 OK", response);
          } catch (const json_parse_error& e) {
            std::string err_msg =
                json_error_msg(e.what(), SELECTION_JSON_SCHEMA);
            respond_http(err_msg, "400 Bad Request", response);
          } catch (const json_semantic_error& e) {
            std::string err_msg =
                json_error_msg(e.what(), SELECTION_JSON_SCHEMA);
            respond_http(err_msg, "400 Bad Request", response);
          } catch (const clipper::ManagementOperationError& e) {
            respond_http(e.what(), "400 Bad Request", response);
          }
        });

    // Healthcheck API
    server_.add_endpoint(
        PING, "GET", 
        [this](std::shared_ptr<HttpServer::Response> response,
               std::shared_ptr<HttpServer::Request> request) {
            respond_http("PONG", "200 OK", response);
        });

    server_.add_endpoint(
        DELETE_VERSIONED_MODEL, "POST",
        [this](std::shared_ptr<HttpServer::Response> response,
               std::shared_ptr<HttpServer::Request> request) {
          try {
            clipper::log_info(LOGGING_TAG_MANAGEMENT_FRONTEND,
                              "Delete versioned model POST request");
            std::string result = delete_versioned_model(request->content.string());
            respond_http(result, "200 OK", response);
          } catch (const json_parse_error& e) {
            std::string err_msg =
                json_error_msg(e.what(), DELETE_VERSIONED_MODEL_JSON_SCHEMA);
            respond_http(err_msg, "400 Bad Request", response);
          } catch (const json_semantic_error& e) {
            std::string err_msg =
                json_error_msg(e.what(), DELETE_VERSIONED_MODEL_JSON_SCHEMA);
            respond_http(err_msg, "400 Bad Request", response);
          } catch (const clipper::ManagementOperationError& e) {
            respond_http(e.what(), "400 Bad Request", response);
          }
        });
  }

  ~RequestHandler() {
    redis_subscriber_.disconnect();
    redis_connection_.disconnect();
  }

  /**
   * Checks `value` for prohibited characters in strings that will be grouped.
   * If it does, throws an error with message stating that input has an invalid
   * `label`.
   *
   * \throws ManagementOperationError error if `value` contains prohibited
   * characters.
   */
  void validate_group_str_for_redis(const std::string& value,
                                    const char* label) {
    if (clipper::redis::contains_prohibited_chars_for_group(value)) {
      std::stringstream ss;

      ss << "Invalid " << label << " supplied: " << value << ".";
      ss << " Contains one of: ";

      // Generate string representing list of invalid characters
      std::string prohibited_str;
      for (size_t i = 0; i != prohibited_group_strings.size() - 1; ++i) {
        prohibited_str = *(prohibited_group_strings.begin() + i);
        ss << "'" << prohibited_str << "', ";
      }
      // Add final element of `prohibited_group_strings`
      ss << "'" << *(prohibited_group_strings.end() - 1) << "'";

      throw clipper::ManagementOperationError(ss.str());
    }
  }

  /**
   * Processes a request to add links between a specified application
   * and a set of models
   *
   * JSON format:
   * {
   *  "app_name" := string,
   *  "model_names" := [string]
   * }
   *
   * \return A string describing the operation's success
   * \throws ManagementOperationError if the operation is not successful
   */
  std::string add_model_links(const std::string& json) {
    rapidjson::Document d;
    parse_json(json, d);

    std::string app_name = get_string(d, "app_name");
    std::vector<string> model_names = get_string_array(d, "model_names");

    // Confirm that the app exists
    auto app_info =
        clipper::redis::get_application(redis_connection_, app_name);
    if (app_info.size() == 0) {
      std::stringstream ss;
      ss << "No app with name "
         << "'" << app_name << "'"
         << " exists.";
      throw clipper::ManagementOperationError(ss.str());
    }

    // Confirm that the models exists and have compatible input_types
    auto app_input_type = app_info["input_type"];
    boost::optional<std::string> model_version;
    std::unordered_map<std::string, std::string> model_info;
    std::string model_input_type;
    for (auto const& model_name : model_names) {
      model_version = clipper::redis::get_current_model_version(
          redis_connection_, model_name);
      if (!model_version) {
        std::stringstream ss;
        ss << "No model with name "
           << "'" << model_name << "'"
           << " exists.";
        throw clipper::ManagementOperationError(ss.str());
      } else {
        model_info = clipper::redis::get_model(
            redis_connection_, VersionedModelId(model_name, *model_version));
        model_input_type = model_info["input_type"];
        if (model_input_type != app_input_type) {
          std::stringstream ss;
          ss << "Model with name "
             << "'" << model_name << "'"
             << " has incompatible input_type "
             << "'" << model_input_type << "'"
             << ". Requested app to link to has input_type "
             << "'" << app_input_type << "'"
             << ".";
          throw clipper::ManagementOperationError(ss.str());
        }
      }
    }

    // Confirm that the user supplied only one model_name
    if (model_names.size() != 1) {
      std::stringstream ss;
      if (model_names.size() == 0) {
        ss << "Please provide the name of the model that you want to link to "
              "the application "
           << "'" << app_name << "'";
      } else {
        ss << "Applications must be linked with at most one model. ";
        ss << "Attempted to add links to " << model_names.size() << " models.";
      }
      std::string error_msg = ss.str();
      clipper::log_error(LOGGING_TAG_MANAGEMENT_FRONTEND, error_msg);
      throw clipper::ManagementOperationError(error_msg);
    }

    // Make sure that there will only be one link
    auto existing_linked_models =
        clipper::redis::get_linked_models(redis_connection_, app_name);

    std::string new_model_name = model_names[0];

    if (existing_linked_models.size() > 0) {
      // We asserted earlier that `model_names` has size 1

      if (std::find(existing_linked_models.begin(),
                    existing_linked_models.end(),
                    new_model_name) != existing_linked_models.end()) {
        std::stringstream ss;
        ss << "The model with name "
           << "'" << new_model_name << "'"
           << " is already linked to "
           << "'" << app_name << "'";
        throw clipper::ManagementOperationError(ss.str());
      } else {
        // We guarantee that there is only one existing model
        std::string existing_model_name = existing_linked_models[0];
        std::stringstream ss;
        ss << "A model with name " << existing_model_name
           << " is already linked to "
           << "'" << app_name << "'"
           << ".";
        throw clipper::ManagementOperationError(ss.str());
      }
    }

    if (clipper::redis::add_model_links(redis_connection_, app_name,
                                        model_names)) {
      std::stringstream ss;
      ss << "Successfully linked model with name "
         << "'" << new_model_name << "'"
         << " to application "
         << "'" << app_name << "'";
      return ss.str();
    } else {
      std::stringstream ss;
      ss << "Error linking models to "
         << "'" << app_name << "'"
         << " in Redis";
      throw clipper::ManagementOperationError(ss.str());
    }
  }

  /**
   * Creates an endpoint that listens for requests to remove links between
   * apps and models
   *
   * JSON format:
   * {
   *  "app_name" := string,
   *  "model_names" := [string]
   * }
   */
  std::string delete_model_links(const std::string& json) {
    rapidjson::Document d;
    parse_json(json, d);

    std::string app_name = get_string(d, "app_name");
    std::vector<string> model_names = get_string_array(d, "model_names");

    // Confirm that the app exists
    auto app_info =
        clipper::redis::get_application(redis_connection_, app_name);
    if (app_info.size() == 0) {
      std::stringstream ss;
      ss << "No app with name " << app_name << " exists.";
      throw std::invalid_argument(ss.str());
    }

    // Confirm that the model names supplied are of linked models
    auto existing_linked_models =
        clipper::redis::get_linked_models(redis_connection_, app_name);

    for (auto const& model_name : model_names) {
      if (std::find(existing_linked_models.begin(),
                    existing_linked_models.end(),
                    model_name) == existing_linked_models.end()) {
        std::stringstream ss;
        ss << "Cannot remove nonexistent link between app " << app_name
           << " and model " << model_name;
        throw std::invalid_argument(ss.str());
      }
    }

    if (clipper::redis::delete_model_links(redis_connection_, app_name,
                                           model_names)) {
      return "Success!";
    } else {
      std::stringstream ss;
      ss << "Error deleting linked models from " << app_name << " in Redis";
      throw std::invalid_argument(ss.str());
    }
  }


  /**
   * Processes a request to add a new application to Clipper
   *
   * JSON format:
   * {
   *  "name" := string,
   *  "input_type" := "integers" | "bytes" | "floats" | "doubles" | "strings",
   *  "default_output" := string,
   *  "latency_slo_micros" := int
   * }
   *
   * \return A string describing the operation's success
   * \throws ManagementOperationError if the operation is not successful
   */
  std::string add_application(const std::string& json) {
    rapidjson::Document d;
    parse_json(json, d);

    std::string app_name = get_string(d, "name");
    InputType input_type =
        clipper::parse_input_type(get_string(d, "input_type"));
    std::string default_output = get_string(d, "default_output");

    std::string selection_policy =
        clipper::DefaultOutputSelectionPolicy::get_name();
    int latency_slo_micros = get_int(d, "latency_slo_micros");
    // check if application already exists
    std::unordered_map<std::string, std::string> existing_app_data =
        clipper::redis::get_application(redis_connection_, app_name);
    if (existing_app_data.empty()) {
      if (clipper::redis::add_application(redis_connection_, app_name,
                                          input_type, selection_policy,
                                          default_output, latency_slo_micros)) {
        std::stringstream ss;
        ss << "Successfully added application with name "
           << "'" << app_name << "'";
        return ss.str();
      } else {
        std::stringstream ss;
        ss << "Error adding application "
           << "'" << app_name << "'"
           << " to Redis";
        throw clipper::ManagementOperationError(ss.str());
      }
    } else {
      std::stringstream ss;
      ss << "application "
         << "'" << app_name << "'"
         << " already exists";
      throw clipper::ManagementOperationError(ss.str());
    }
  }

  /**
   * Processes a request to delete an application from Clipper
   *
   * JSON format:
   * {
   *  "name" := string
   * }
   *
   * \return A string describing the operation's success
   * \throws ManagementOperationError if the operation is not successful
   */

  std::string delete_application(const std::string& json) {
    rapidjson::Document d;
    parse_json(json, d);
    std::string app_name = get_string(d, "name");

    // Check if application exists
    std::unordered_map<std::string, std::string> existing_app_data =
        clipper::redis::get_application(redis_connection_, app_name);

    if (!existing_app_data.empty()) {
      if (clipper::redis::delete_application(redis_connection_, app_name)) {
        std::stringstream ss;
        ss << "Successfully deleted application with name "
           << "'" << app_name << "'";
        return ss.str();
      } else {
        std::stringstream ss;
        ss << "Error deleting application "
           << "'" << app_name << "'"
           << " to Redis";
        throw clipper::ManagementOperationError(ss.str());
      }
    } else {
      std::stringstream ss;
      ss << "Application "
         << "'" << app_name << "'"
         << " does not exist";
      throw clipper::ManagementOperationError(ss.str());
    }
  }

  /**
   * Processes a request to add a new model to Clipper
   *
   * JSON format:
   * {
   *  "model_name" := string,
   *  "model_version" := string,
   *  "labels" := [string]
   *  "input_type" := "integers" | "bytes" | "floats" | "doubles" | "strings",
   *  "container_name" := string,
   *  "model_data_path" := string
   * }
   *
   * \return A string describing the operation's success
   * \throws ManagementOperationError if the operation is not successful
   */
  std::string add_model(const std::string& json) {
    rapidjson::Document d;
    parse_json(json, d);
    std::string model_name = get_string(d, "model_name");
    std::string model_version = get_string(d, "model_version");
    VersionedModelId model_id = VersionedModelId(model_name, model_version);

    std::vector<std::string> labels = get_string_array(d, "labels");
    std::string input_type_raw = get_string(d, "input_type");
    InputType input_type = clipper::parse_input_type(input_type_raw);
    std::string container_name = get_string(d, "container_name");
    std::string model_data_path = get_string(d, "model_data_path");
    int batch_size = get_int(d, "batch_size");

    // The batch_size should be either positive or DEFAULT_BATCH_SIZE
    if (batch_size <= 0 && batch_size != DEFAULT_BATCH_SIZE) {
      std::stringstream ss;
      ss << "The batch size must be positive or DEFAULT_BATCH_SIZE, which is "
            "-1";
      throw clipper::ManagementOperationError(ss.str());
    }
    // Validate strings that will be grouped before supplying to redis
    validate_group_str_for_redis(model_name, "model name");
    validate_group_str_for_redis(model_id.get_id(), "model version");
    for (auto label : labels) {
      validate_group_str_for_redis(label, "label");
    }

    // check if this version of the model has already been deployed
    std::unordered_map<std::string, std::string> existing_model_data =
        clipper::redis::get_model(redis_connection_, model_id);

    if (!existing_model_data.empty()) {
      std::stringstream ss;
      ss << "model with name "
         << "'" << model_name << "'"
         << " and version "
         << "'" << model_version << "'"
         << " already exists";
      throw clipper::ManagementOperationError(ss.str());
    }

    check_updated_model_consistent_with_app_links(
        VersionedModelId(model_name, model_version),
        boost::optional<InputType>(input_type));

    if (clipper::redis::add_model(redis_connection_, model_id, input_type,
                                  labels, container_name, model_data_path,
                                  batch_size)) {
      attempt_model_version_update(model_id.get_name(), model_id.get_id());
      std::stringstream ss;
      ss << "Successfully added model with name "
         << "'" << model_name << "'"
         << " and input type "
         << "'" << clipper::get_readable_input_type(input_type) << "'";
      return ss.str();
    }
    std::stringstream ss;
    ss << "Error adding model " << model_id.get_name() << ":"
       << model_id.get_id() << " to Redis";
    throw clipper::ManagementOperationError(ss.str());
  }

  /**
   * During a version update, ensures that the input type associated
   * with a VersionedModelId matches the input type associated with
   * each application to which it is linked
   *
   * \param model_id The id of the model being checked
   * \param input_type (optional) The input type for the model, if it is already
   * known
   *
   * \throws ManagementOperationError If a discrepancy exists
   * between the model input type and the application type
   * to which the
   */
  void check_updated_model_consistent_with_app_links(
      VersionedModelId model_id,
      boost::optional<InputType> input_type = boost::none) {
    InputType model_input_type;
    if (input_type) {
      model_input_type = input_type.get();
    } else {
      auto model_info = clipper::redis::get_model(redis_connection_, model_id);
      model_input_type = clipper::parse_input_type(model_info["input_type"]);
    }
    auto app_names =
        clipper::redis::get_all_application_names(redis_connection_);
    std::vector<std::string> linked_models;
    std::unordered_map<std::string, std::string> app_info;
    for (auto const& app_name : app_names) {
      linked_models =
          clipper::redis::get_linked_models(redis_connection_, app_name);
      if (std::find(linked_models.begin(), linked_models.end(),
                    model_id.get_name()) != linked_models.end()) {
        app_info = clipper::redis::get_application(redis_connection_, app_name);
        clipper::InputType app_input_type =
            clipper::parse_input_type(app_info["input_type"]);
        if (model_input_type != app_input_type) {
          std::stringstream ss;
          ss << "Model with name "
             << "'" << model_id.get_name() << "'"
             << " is already linked to app "
             << "'" << app_name << "'"
             << " using input type "
             << "'" << get_readable_input_type(app_input_type) << "'"
             << ". The input type you provided for a new version of the model, "
             << "'" << get_readable_input_type(model_input_type) << "'"
             << ", is not compatible.";
          throw clipper::ManagementOperationError(ss.str());
        }
      }
    }
  }

  /**
   * Processes a request to retrieve information about all registered
   * Clipper applications
   *
   * JSON format:
   * {
   *  "verbose" := bool
   * }
   *
   * \return Returns a JSON string that encodes a list with info about
   * registered apps. If `verbose` == False, the encoded list has all registered
   * apps' names. Else, the encoded map contains objects with full app
   * information.
   */
  std::string get_all_applications(const std::string& json) {
    rapidjson::Document d;
    parse_json(json, d);

    bool verbose = get_bool(d, "verbose");

    std::vector<std::string> app_names =
        clipper::redis::get_all_application_names(redis_connection_);

    rapidjson::Document response_doc;
    response_doc.SetArray();
    std::vector<std::string> linked_models;

    if (verbose) {
      for (const string& app_name : app_names) {
        std::unordered_map<std::string, std::string> app_metadata =
            clipper::redis::get_application(redis_connection_, app_name);
        rapidjson::Document app_doc(&response_doc.GetAllocator());
        redis_app_metadata_to_json(app_doc, app_metadata);
        /* We need to add each app's name to its returned JSON object. */
        add_string(app_doc, "name", app_name);
        /* We need to add the app's linked models to its returned JSON object */
        linked_models =
            clipper::redis::get_linked_models(redis_connection_, app_name);
        add_string_array(app_doc, "linked_models", linked_models);
        response_doc.PushBack(app_doc, response_doc.GetAllocator());
      }
    } else {
      for (const string& app_name : app_names) {
        rapidjson::Value v;
        v.SetString(app_name.c_str(), app_name.length(),
                    response_doc.GetAllocator());
        response_doc.PushBack(v, response_doc.GetAllocator());
      }
    }
    return to_json_string(response_doc);
  }

  /**
   * Processes a request to retrieve information about a specified
   * Clipper application
   *
   * JSON format:
   * {
   *  "name" := string
   * }
   *
   * \return Returns a JSON string encoding a map of the specified application's
   * attribute name-value pairs.
   */
  std::string get_application(const std::string& json) {
    rapidjson::Document d;
    parse_json(json, d);

    std::string app_name = get_string(d, "name");
    std::unordered_map<std::string, std::string> app_metadata =
        clipper::redis::get_application(redis_connection_, app_name);

    rapidjson::Document response_doc;
    response_doc.SetObject();

    if (app_metadata.size() > 0) {
      /* We assume that redis::get_application returns an empty map iff no app
       * exists */
      /* If an app does exist, we need to add its name to the map. */
      redis_app_metadata_to_json(response_doc, app_metadata);
      add_string(response_doc, "name", app_name);
      auto linked_models =
          clipper::redis::get_linked_models(redis_connection_, app_name);
      add_string_array(response_doc, "linked_models", linked_models);
    }

    return to_json_string(response_doc);
  }

  /**
   * Processes a request to retrieve information about the
   * set of models linked to a specified application
   *
   * JSON format:
   * {
   *  "app_name" := string
   * }
   *
   * \return Returns a JSON string encoding a vector of the specified
   * application's
   * linked models' names.
   *
   * \throws ManagementOperationError if the operation is not successful
   */
  std::string get_linked_models(const std::string& json) {
    rapidjson::Document d;
    parse_json(json, d);

    std::string app_name = get_string(d, "app_name");

    // Confirm that the app exists
    auto app_info =
        clipper::redis::get_application(redis_connection_, app_name);
    if (app_info.size() == 0) {
      std::stringstream ss;
      ss << "No application with name "
         << "'" << app_name << "'"
         << " exists.";
      throw clipper::ManagementOperationError(ss.str());
    }

    auto model_names =
        clipper::redis::get_linked_models(redis_connection_, app_name);
    rapidjson::Document response_doc;
    set_string_array(response_doc, model_names);

    return to_json_string(response_doc);
  }

  /**
   * Processes a request to retrieve information about all
   * registered Clipper models
   *
   * JSON format:
   * {
   *  "verbose" := bool
   * }
   *
   * \return Returns a JSON string that encodes a list with info about
   * registered models. If `verbose` == False, the encoded list has all
   * registered
   * models' names. Else, the encoded map contains objects with full model
   * information.
   */
  std::string get_all_models(const std::string& json) {
    rapidjson::Document d;
    parse_json(json, d);

    bool verbose = get_bool(d, "verbose");

    std::vector<VersionedModelId> models =
        clipper::redis::get_all_models(redis_connection_);

    rapidjson::Document response_doc;
    response_doc.SetArray();
    // need to maintain references
    std::vector<std::string> model_strs;

    if (verbose) {
      for (auto model : models) {
        std::unordered_map<std::string, std::string> model_metadata =
            clipper::redis::get_model(redis_connection_, model);
        rapidjson::Document model_doc(&response_doc.GetAllocator());
        redis_model_metadata_to_json(model_doc, model_metadata);
        bool is_current_version =
            clipper::redis::get_current_model_version(
                redis_connection_, model.get_name()) == model.get_id();
        add_bool(model_doc, "is_current_version", is_current_version);
        response_doc.PushBack(model_doc, response_doc.GetAllocator());
      }
    } else {
      for (auto model : models) {
        std::string model_str = model.serialize();
        rapidjson::Value v;
        v.SetString(model_str.c_str(), model_str.length(),
                    response_doc.GetAllocator());
        response_doc.PushBack(v, response_doc.GetAllocator());
      }
    }
    std::string result = to_json_string(response_doc);
    clipper::log_info_formatted(LOGGING_TAG_MANAGEMENT_FRONTEND,
                                "get_all_models response: {}", result);
    return result;
  }

  /**
   * Processes a request to retrieve information about a specified
   * model registered with Clipper
   *
   * JSON format:
   * {
   *  "model_name" := string,
   *  "model_version" := string,
   * }
   *
   * \return Returns a JSON string encoding a map of the specified model's
   * attribute name-value pairs, including whether it is the currently deployed
   * version of the model.
   */
  std::string get_model(const std::string& json) {
    rapidjson::Document d;
    parse_json(json, d);

    std::string model_name = get_string(d, "model_name");
    std::string model_version = get_string(d, "model_version");
    VersionedModelId model = VersionedModelId(model_name, model_version);

    std::unordered_map<std::string, std::string> model_metadata =
        clipper::redis::get_model(redis_connection_, model);

    rapidjson::Document response_doc;
    response_doc.SetObject();

    if (model_metadata.size() > 0) {
      /* We assume that redis::get_model returns an empty map if no model
       * exists */
      redis_model_metadata_to_json(response_doc, model_metadata);
      bool is_current_version =
          clipper::redis::get_current_model_version(
              redis_connection_, model.get_name()) == model.get_id();
      add_bool(response_doc, "is_current_version", is_current_version);
    }

    return to_json_string(response_doc);
  }

  /**
   * Processes a request to retrieve information about all
   * model containers registered with Clipper
   *
   * JSON format:
   * {
   *  "verbose" := bool
   * }
   *
   * \return Returns a JSON string that encodes a list with info about
   * containers.
   * If `verbose` == False, the encoded list has all containers' names (model
   * name, model version, container ID).
   * Else, the encoded map contains objects with full container information.
   */
  std::string get_all_containers(const std::string& json) {
    rapidjson::Document d;
    parse_json(json, d);

    bool verbose = get_bool(d, "verbose");

    std::vector<std::pair<VersionedModelId, int>> containers =
        clipper::redis::get_all_containers(redis_connection_);

    rapidjson::Document response_doc;
    response_doc.SetArray();

    if (verbose) {
      for (auto container : containers) {
        std::unordered_map<std::string, std::string> container_metadata =
            clipper::redis::get_container(redis_connection_, container.first,
                                          container.second);
        rapidjson::Document container_doc(&response_doc.GetAllocator());
        redis_container_metadata_to_json(container_doc, container_metadata);
        response_doc.PushBack(container_doc, response_doc.GetAllocator());
      }
    } else {
      for (auto container : containers) {
        std::stringstream ss;
        ss << container.first.serialize();
        ss << ":";
        ss << container.second;
        std::string container_str = ss.str();
        rapidjson::Value v;
        v.SetString(container_str.c_str(), container_str.length(),
                    response_doc.GetAllocator());
        response_doc.PushBack(v, response_doc.GetAllocator());
      }
    }
    std::string result = to_json_string(response_doc);
    clipper::log_info_formatted(LOGGING_TAG_MANAGEMENT_FRONTEND,
                                "get_all_containers response: {}", result);
    return result;
  }

  /**
   * Processes a request to retrieve information about a
   * specified container registered with Clipper
   *
   * JSON format:
   * {
   *  "model_name" := string,
   *  "model_version" := string,
   *  "replica_id" := int
   * }
   *
   * \return Returns a JSON string encoding a map of the specified container's
   * attribute name-value pairs.
   */
  std::string get_container(const std::string& json) {
    rapidjson::Document d;
    parse_json(json, d);

    std::string model_name = get_string(d, "model_name");
    std::string model_version = get_string(d, "model_version");
    int replica_id = get_int(d, "replica_id");
    VersionedModelId model = VersionedModelId(model_name, model_version);

    std::unordered_map<std::string, std::string> container_metadata =
        clipper::redis::get_container(redis_connection_, model, replica_id);

    rapidjson::Document response_doc;
    response_doc.SetObject();

    if (container_metadata.size() > 0) {
      /* We assume that redis::get_container returns an empty map iff no
       * container exists */
      redis_container_metadata_to_json(response_doc, container_metadata);
    }

    return to_json_string(response_doc);
  }

  /**
   * Processes a request to obtain the debug string
   * for a user's selection policy state for an applicaiton.
   *
   * JSON format:
   * {
   *  "app_name" := string,
   *  "uid" := int,
   * }
   */
  std::string get_selection_state(const std::string& json) {
    rapidjson::Document d;
    parse_json(json, d);

    std::string app_name = get_string(d, "app_name");
    int uid = get_int(d, "uid");
    if (uid != clipper::DEFAULT_USER_ID) {
      clipper::log_error_formatted(LOGGING_TAG_MANAGEMENT_FRONTEND,
                                   "Personalized default outputs are not "
                                   "currently supported. Using default UID {} "
                                   "instead",
                                   clipper::DEFAULT_USER_ID);
      uid = clipper::DEFAULT_USER_ID;
    }
    auto app_metadata =
        clipper::redis::get_application(redis_connection_, app_name);
    return app_metadata["default_output"];
  }

  /**
   * Processes a request to remove the versioned model from Clipper
   *
   * JSON format:
   * {
   *  "model_name" := string,
   *  "model_version" := string
   * }
   *
   * \return A string describing the operation's success
   * \throws ManagementOperationError if the operation is not successful
   */
  std::string delete_versioned_model(const std::string& json) {
    rapidjson::Document d;
    parse_json(json, d);
    std::string model_name = get_string(d, "model_name");
    std::string model_version = get_string(d, "model_version");
    VersionedModelId model_id = VersionedModelId(model_name, model_version);

    // check if this version of the model has been existed or not
    std::unordered_map<std::string, std::string> existing_model_data =
        clipper::redis::get_model(redis_connection_, model_id);
    if (existing_model_data.empty()) {
      std::stringstream ss;
      ss << "model with name "
         << "'" << model_name << "'"
         << " and version "
         << "'" << model_version << "'"
         << " does not exist";
      throw clipper::ManagementOperationError(ss.str());
    }
    if (existing_model_data.find("valid") == existing_model_data.end()) {
      std::stringstream ss;
      ss << "model with name "
         << "'" << model_name << "'"
         << " and version "
         << "'" << model_version << "'"
         << " is already marked as 'invalid'";
      throw clipper::ManagementOperationError(ss.str());
    }

    if (clipper::redis::mark_versioned_model_for_delete(redis_connection_, model_id)) {
      std::stringstream ss;
      ss << "Successfully deleted model with name "
         << "'" << model_name << "'"
         << " and version "
         << "'" << model_version << "'";
      return ss.str();
    } else {
      std::stringstream ss;
      ss << "Error deleting model with name "
         << "'" << model_name << "'"
         << " and version "
         << "'" << model_version << "'"
         << " from Redis";
      throw clipper::ManagementOperationError(ss.str());
    }
  }

  /**
   * Processes a request to update a specified model to a
   * specified version
   *
   * JSON format:
   * {
   *  "model_name" := string,
   *  "model_version" := string,
   * }
   *
   * \return A string describing the operation's success
   * \throws ManagementOperationError if the operation is not successful
   */
  std::string set_model_version(const std::string& json) {
    rapidjson::Document d;
    parse_json(json, d);
    std::string model_name = get_string(d, "model_name");
    std::string new_model_version = get_string(d, "model_version");

    std::vector<std::string> versions =
        clipper::redis::get_model_versions(redis_connection_, model_name);

    if (versions.size() == 0) {
      std::stringstream ss;
      ss << "Cannot set version for nonexistent model "
         << "'" << model_name << "'";
      throw clipper::ManagementOperationError(ss.str());
    }

    bool version_exists = false;
    for (auto v : versions) {
      if (v == new_model_version) {
        version_exists = true;
        break;
      }
    }
    if (!version_exists) {
      std::stringstream ss;
      ss << "Cannot set non-existent version "
         << "'" << new_model_version << "'"
         << " for model with name "
         << "'" << model_name << "'";
      std::string err_msg = ss.str();
      clipper::log_error(LOGGING_TAG_MANAGEMENT_FRONTEND, err_msg);
      throw clipper::ManagementOperationError(err_msg);
    }

    check_updated_model_consistent_with_app_links(
        VersionedModelId(model_name, new_model_version));

    attempt_model_version_update(model_name, new_model_version);
    std::stringstream ss;
    ss << "Successfully set model with name "
       << "'" << model_name << "'"
       << " to version "
       << "'" << new_model_version << "'";
    return ss.str();
  }

  /**
   * Attempts to update the version of model with name `model_name` to
   * `new_model_version`.
   */
  void attempt_model_version_update(const string& model_name,
                                    const string& new_model_version) {
    if (!clipper::redis::set_current_model_version(
            redis_connection_, model_name, new_model_version)) {
      std::stringstream ss;
      ss << "Version "
         << "'" << new_model_version << "'"
         << " does not exist for model with name"
         << "'" << model_name << "'";
      throw clipper::ManagementOperationError(ss.str());
    }
  }

  void start_listening() { server_.start(); }

 private:
  HttpServer server_;
  redox::Redox redis_connection_;
  redox::Subscriber redis_subscriber_;
  clipper::StateDB state_db_;
};

}  // namespace management
