#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <boost/thread.hpp>

#include <clipper/config.hpp>
#include <clipper/constants.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/json_util.hpp>
#include <clipper/query_processor.hpp>

#include "management_frontend.hpp"

using namespace clipper;
using namespace clipper::redis;
using namespace clipper::json;
using namespace management;

namespace {

class ManagementFrontendTest : public ::testing::Test {
 public:
  ManagementFrontendTest()
      : rh_(MANAGEMENT_FRONTEND_PORT, 1),
        redis_(std::make_shared<redox::Redox>()),
        subscriber_(std::make_shared<redox::Subscriber>()) {
    Config& conf = get_config();
    redis_->connect(conf.get_redis_address(), conf.get_redis_port());
    subscriber_->connect(conf.get_redis_address(), conf.get_redis_port());

    // delete all keys
    send_cmd_no_reply<std::string>(*redis_, {"FLUSHALL"});

    send_cmd_no_reply<std::string>(
        *redis_, {"CONFIG", "SET", "notify-keyspace-events", "AKE"});
  }

  virtual ~ManagementFrontendTest() {
    subscriber_->disconnect();
    redis_->disconnect();
  }

  void set_add_app_request_doc(rapidjson::Document& d, std::string& name,
                               std::vector<std::string>& candidate_model_names,
                               std::string& input_type,
                               std::string& default_output,
                               int latency_slo_micros) {
    d.SetObject();
    add_string(d, "name", name);
    add_string_array(d, "candidate_model_names", candidate_model_names);
    add_string(d, "input_type", input_type);
    add_string(d, "default_output", default_output);
    add_int(d, "latency_slo_micros", latency_slo_micros);
  }

  std::string get_app_json_request_string(std::string& name) {
    rapidjson::Document d;
    d.SetObject();
    add_string(d, "name", name);
    return to_json_string(d);
  }

  RequestHandler rh_;
  std::shared_ptr<redox::Redox> redis_;
  std::shared_ptr<redox::Subscriber> subscriber_;
};

TEST_F(ManagementFrontendTest, TestAddApplicationCorrect) {
  std::string add_app_json = R"(
  {
    "name": "myappname",
    "candidate_model_names": ["image_model"],
    "input_type": "integers",
    "default_output": "4.3",
    "latency_slo_micros": 10000
  }
  )";

  ASSERT_EQ(rh_.add_application(add_app_json), "Success!");
  auto result = get_application(*redis_, "myappname");
  // The application table has 5 fields, so we expect to get back a map with 5
  // entries in it (see add_application() in redis.cpp for details on what the
  // fields are).
  ASSERT_EQ(result.size(), static_cast<size_t>(5));
}

TEST_F(ManagementFrontendTest, TestAddDuplicateApplication) {
  std::string add_app_json = R"(
  {
    "name": "myappname",
    "candidate_model_names": ["image_model"],
    "input_type": "integers",
    "default_output": "4.3",
    "latency_slo_micros": 10000
  }
  )";

  ASSERT_EQ(rh_.add_application(add_app_json), "Success!");
  auto result = get_application(*redis_, "myappname");
  // The application table has 5 fields, so we expect to get back a map with 5
  // entries in it (see add_application() in redis.cpp for details on what the
  // fields are).
  ASSERT_EQ(result.size(), static_cast<size_t>(5));

  std::string add_dup_app_json = R"(
  {
    "name": "myappname",
    "candidate_model_names": ["k", "m"],
    "input_type": "integers",
    "default_output": "4.3",
    "latency_slo_micros": 120000
  }
  )";

  ASSERT_THROW(rh_.add_application(add_dup_app_json), std::invalid_argument);
}

TEST_F(ManagementFrontendTest, TestAddApplicationMissingField) {
  std::string add_app_json = R"(
  {
    "name": "myappname",
    "input_type": "integers",
    "latency_slo_micros": 10000
  }
  )";

  ASSERT_THROW(rh_.add_application(add_app_json), json_semantic_error);
}

TEST_F(ManagementFrontendTest, TestAddApplicationMalformedJson) {
  std::string add_app_json = R"(
  {
    "name": "myappname,
    "input_type "integers",
    "selection_policy":,
    "latency_slo_micros": 10000
  )";

  ASSERT_THROW(rh_.add_application(add_app_json), json_parse_error);
}

TEST_F(ManagementFrontendTest, TestGetApplicationCorrect) {
  std::string name = "my_app_name";
  std::string input_type = "doubles";
  std::string default_output = "1.0";
  int latency_slo_micros = 10000;
  // For now, we only support adding one candidate model
  std::vector<std::string> candidate_model_names{"music_random_features"};

  rapidjson::Document add_app_json_document;
  set_add_app_request_doc(add_app_json_document, name, candidate_model_names,
                          input_type, default_output, latency_slo_micros);
  std::string add_app_json_string = to_json_string(add_app_json_document);

  ASSERT_EQ(rh_.add_application(add_app_json_string), "Success!");

  std::string get_app_json = get_app_json_request_string(name);

  std::string json_response = rh_.get_application(get_app_json);

  rapidjson::Document response_doc;
  response_doc.SetObject();
  parse_json(json_response, response_doc);

  // The JSON provided for adding the app contains the same name-value
  // attribute pairs that should be returned by `get_application`.
  ASSERT_EQ(add_app_json_document, response_doc);
}

TEST_F(ManagementFrontendTest, TestGetNonexistentApplicationCorrect) {
  std::string list_apps_json = R"(
  {
    "name": "nonexistent_app"
  }
  )";
  std::string json_response = rh_.get_application(list_apps_json);
  std::string expected_response = "{}";
  ASSERT_EQ(json_response, expected_response);
}

TEST_F(ManagementFrontendTest, TestGetApplicationMalformedJson) {
  std::string list_apps_json = R"(
  {
    "app": not a string
  }
  )";
  ASSERT_THROW(rh_.get_application(list_apps_json), json_parse_error);
}

TEST_F(ManagementFrontendTest, TestGetAllApplicationsVerboseCorrect) {
  std::string name1 = "my_app_name1";
  std::string name2 = "my_app_name2";
  std::string input_type = "doubles";
  std::string default_output = "1.0";
  int latency_slo_micros = 10000;
  // For now, we only support adding one candidate model
  std::vector<std::string> candidate_model_names{"music_random_features"};

  rapidjson::Document add_app1_json_document;
  set_add_app_request_doc(add_app1_json_document, name1, candidate_model_names,
                          input_type, default_output, latency_slo_micros);
  std::string add_app1_json_string = to_json_string(add_app1_json_document);

  rapidjson::Document add_app2_json_document;
  set_add_app_request_doc(add_app2_json_document, name2, candidate_model_names,
                          input_type, default_output, latency_slo_micros);
  std::string add_app2_json_string = to_json_string(add_app2_json_document);

  ASSERT_EQ(rh_.add_application(add_app1_json_string), "Success!");
  ASSERT_EQ(rh_.add_application(add_app2_json_string), "Success!");

  std::string get_apps_verbose_json = R"(
  {
    "verbose": true
  }
  )";

  std::string json_response = rh_.get_all_applications(get_apps_verbose_json);

  rapidjson::Document response_doc;
  response_doc.SetArray();
  parse_json(json_response, response_doc);

  rapidjson::Value app1_response_doc(response_doc[0].GetObject());
  rapidjson::Value app2_response_doc(response_doc[1].GetObject());
  rapidjson::Value temp;

  // Assign app_1_response_doc and app_2_response_doc to documentes of
  // apps with names `name1`, `name2`, respectively.
  if (get_string(app2_response_doc, "name") == name1) {
    temp = app1_response_doc;
    app1_response_doc = app2_response_doc;
    app2_response_doc = temp;
  }

  // Confirm that the JSON response provided app metadata that
  // matches the information provided upon their registration
  ASSERT_EQ(add_app1_json_document, app1_response_doc);
  ASSERT_EQ(add_app2_json_document, app2_response_doc);
}

TEST_F(ManagementFrontendTest,
       TestGetAllApplicationsVerboseNoneRegisteredCorrect) {
  std::string get_apps_verbose_json = R"(
  {
    "verbose": true
  }
  )";
  std::string json_response = rh_.get_all_applications(get_apps_verbose_json);
  std::string expected_response = "[]";
  ASSERT_EQ(json_response, expected_response);
}

TEST_F(ManagementFrontendTest,
       TestGetAllApplicationsNotVerboseNoneRegisteredCorrect) {
  std::string get_apps_verbose_json = R"(
  {
    "verbose": false
  }
  )";
  std::string json_response = rh_.get_all_applications(get_apps_verbose_json);
  std::string expected_response = "[]";
  ASSERT_EQ(json_response, expected_response);
}

TEST_F(ManagementFrontendTest, TestGetAllApplicationsNotVerboseCorrect) {
  std::string name1 = "my_app_name1";
  std::string name2 = "my_app_name2";
  std::string input_type = "doubles";
  std::string default_output = "1.0";
  int latency_slo_micros = 10000;
  // For now, we only support adding one candidate model
  std::vector<std::string> candidate_model_names{"music_random_features"};

  rapidjson::Document add_app1_json_document;
  set_add_app_request_doc(add_app1_json_document, name1, candidate_model_names,
                          input_type, default_output, latency_slo_micros);
  std::string add_app1_json_string = to_json_string(add_app1_json_document);

  rapidjson::Document add_app2_json_document;
  set_add_app_request_doc(add_app2_json_document, name2, candidate_model_names,
                          input_type, default_output, latency_slo_micros);
  std::string add_app2_json_string = to_json_string(add_app2_json_document);

  ASSERT_EQ(rh_.add_application(add_app1_json_string), "Success!");
  ASSERT_EQ(rh_.add_application(add_app2_json_string), "Success!");

  std::string get_apps_json = R"(
  {
    "verbose": false
  }
  )";

  std::string json_response = rh_.get_all_applications(get_apps_json);

  rapidjson::Document d;
  d.SetArray();
  parse_json(json_response, d);
  std::string el1 = d[0].GetString();
  std::string el2 = d[1].GetString();
  bool has_name_1 = (el1 == name1 || el2 == name1);
  bool has_name_2 = (el1 == name2 || el2 == name2);

  // The JSON response should contain the names of the two apps
  // that were registered.
  ASSERT_TRUE(has_name_1 && has_name_2);
}

TEST_F(ManagementFrontendTest, TestGetAllApplicationsMalformedJson) {
  std::string get_apps_json = R"(
   {
     "verbose": flalse
   }
   )";
  ASSERT_THROW(rh_.get_all_applications(get_apps_json), json_parse_error);
}

TEST_F(ManagementFrontendTest, TestAddModelCorrect) {
  std::string add_model_json = R"(
  {
    "model_name": "mymodelname",
    "model_version": 4,
    "labels": ["label1", "label2", "label3"],
    "input_type": "integers",
    "container_name": "clipper/sklearn_cifar",
    "model_data_path": "/tmp/model/repo/m/1"
  }
  )";

  ASSERT_EQ(rh_.add_model(add_model_json), "Success!");
  std::string model_name = "mymodelname";
  int model_version = 4;
  auto result = get_model(*redis_, std::make_pair(model_name, model_version));
  // The model table has 7 fields, so we expect to get back a map with 7
  // entries in it (see add_model() in redis.cpp for details on what the
  // fields are).
  ASSERT_EQ(result.size(), static_cast<size_t>(7));

  // Make sure that the current model version has been updated
  // appropriately.
  ASSERT_EQ(get_current_model_version(*redis_, model_name), model_version);
}

TEST_F(ManagementFrontendTest, TestAddDuplicateModelVersion) {
  std::string add_model_json = R"(
  {
    "model_name": "mymodelname",
    "model_version": 4,
    "labels": ["label1", "label2", "label3"],
    "input_type": "integers",
    "container_name": "clipper/sklearn_cifar",
    "model_data_path": "/tmp/model/repo/m/4"
  }
  )";

  ASSERT_EQ(rh_.add_model(add_model_json), "Success!");
  std::string model_name = "mymodelname";
  int model_version = 4;
  auto result = get_model(*redis_, std::make_pair(model_name, model_version));
  // The model table has 7 fields, so we expect to get back a map with 7
  // entries in it (see add_model() in redis.cpp for details on what the
  // fields are).
  ASSERT_EQ(result.size(), static_cast<size_t>(7));

  // Make sure that the current model version has been updated
  // appropriately.
  ASSERT_EQ(get_current_model_version(*redis_, model_name), model_version);

  std::string add_dup_model_json = R"(
  {
    "model_name": "mymodelname",
    "model_version": 4,
    "labels": ["label1", "label5"],
    "input_type": "doubles",
    "container_name": "clipper/other_container",
    "model_data_path": "/tmp/model/repo/m/4"
  }
  )";

  ASSERT_THROW(rh_.add_model(add_dup_model_json), std::invalid_argument);
}

TEST_F(ManagementFrontendTest, TestAddModelMissingField) {
  std::string add_model_json = R"(
  {
    "model_name": "mymodelname",
    "model_version": 4,
    "labels": ["label1", "label2", "label3"]
  }
  )";

  ASSERT_THROW(rh_.add_model(add_model_json), json_semantic_error);
}

TEST_F(ManagementFrontendTest, TestAddModelMalformedJson) {
  std::string add_model_json = R"(
    "model_name": "mymodelname
    "model_version": 4
    "labels": ["label1", "label2", "label3"
  )";
  ASSERT_THROW(rh_.add_model(add_model_json), json_parse_error);
}

TEST_F(ManagementFrontendTest, TestSetModelVersionCorrect) {
  std::string v1_json = R"(
  {
    "model_name": "m",
    "model_version": 1,
    "labels": ["ads", "images"],
    "input_type": "ints",
    "container_name": "clipper/test_container",
    "model_data_path": "/tmp/models/m/1"
  }
  )";
  ASSERT_EQ(rh_.add_model(v1_json), "Success!");

  std::string v2_json = R"(
  {
    "model_name": "m",
    "model_version": 2,
    "labels": ["ads", "images"],
    "input_type": "ints",
    "container_name": "clipper/test_container",
    "model_data_path": "/tmp/models/m/2"
  }
  )";
  ASSERT_EQ(rh_.add_model(v2_json), "Success!");

  std::string v4_json = R"(
  {
    "model_name": "m",
    "model_version": 4,
    "labels": ["ads", "images"],
    "input_type": "ints",
    "container_name": "clipper/test_container",
    "model_data_path": "/tmp/models/m/4"
  }
  )";
  ASSERT_EQ(rh_.add_model(v4_json), "Success!");

  ASSERT_EQ(get_current_model_version(*redis_, "m"), 4);
  ASSERT_TRUE(rh_.set_model_version("m", 2));
  ASSERT_EQ(get_current_model_version(*redis_, "m"), 2);
}

TEST_F(ManagementFrontendTest, TestSetModelInvalidVersion) {
  std::string v1_json = R"(
  {
    "model_name": "m",
    "model_version": 1,
    "labels": ["ads", "images"],
    "input_type": "ints",
    "container_name": "clipper/test_container",
    "model_data_path": "/tmp/models/m/1"
  }
  )";
  ASSERT_EQ(rh_.add_model(v1_json), "Success!");

  std::string v2_json = R"(
  {
    "model_name": "m",
    "model_version": 2,
    "labels": ["ads", "images"],
    "input_type": "ints",
    "container_name": "clipper/test_container",
    "model_data_path": "/tmp/models/m/2"
  }
  )";
  ASSERT_EQ(rh_.add_model(v2_json), "Success!");

  std::string v4_json = R"(
  {
    "model_name": "m",
    "model_version": 4,
    "labels": ["ads", "images"],
    "input_type": "ints",
    "container_name": "clipper/test_container",
    "model_data_path": "/tmp/models/m/4"
  }
  )";
  ASSERT_EQ(rh_.add_model(v4_json), "Success!");

  ASSERT_EQ(get_current_model_version(*redis_, "m"), 4);
  ASSERT_FALSE(rh_.set_model_version("m", 11));
  ASSERT_EQ(get_current_model_version(*redis_, "m"), 4);
}

}  // namespace
