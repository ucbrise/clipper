#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <clipper/config.hpp>
#include <clipper/constants.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/exceptions.hpp>
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
      : rh_("0.0.0.0", MANAGEMENT_FRONTEND_PORT),
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
                               std::string& input_type,
                               std::string& default_output,
                               int latency_slo_micros) {
    d.SetObject();
    add_string(d, "name", name);
    add_string(d, "input_type", input_type);
    add_string(d, "default_output", default_output);
    add_int(d, "latency_slo_micros", latency_slo_micros);
  }

  std::string get_add_app_request_json(std::string& name,
                                       std::string& input_type,
                                       std::string& default_output,
                                       int latency_slo_micros) {
    rapidjson::Document d;
    set_add_app_request_doc(d, name, input_type, default_output,
                            latency_slo_micros);
    return to_json_string(d);
  }

  void set_add_model_request_doc(rapidjson::Document& d,
                                 std::string& model_name,
                                 std::string& model_version,
                                 std::string& input_type,
                                 std::vector<std::string>& labels,
                                 std::string& container_name,
                                 std::string& model_data_path) {
    d.SetObject();
    add_string(d, "model_name", model_name);
    add_string(d, "model_version", model_version);
    add_string_array(d, "labels", labels);
    add_string(d, "input_type", input_type);
    add_int(d, "batch_size", DEFAULT_BATCH_SIZE);
    add_string(d, "container_name", container_name);
    add_string(d, "model_data_path", model_data_path);
  }

  std::string get_add_model_request_json(std::string& model_name,
                                         std::string& model_version,
                                         std::string& input_type,
                                         std::vector<std::string>& labels,
                                         std::string& container_name,
                                         std::string& model_data_path) {
    rapidjson::Document d;
    set_add_model_request_doc(d, model_name, model_version, input_type, labels,
                              container_name, model_data_path);
    return to_json_string(d);
  }

  void set_add_model_links_request_doc(rapidjson::Document& d,
                                       std::string& app_name,
                                       std::vector<std::string>& model_names) {
    d.SetObject();
    add_string(d, "app_name", app_name);
    add_string_array(d, "model_names", model_names);
  }

  void set_set_model_version_request_doc(rapidjson::Document& d,
                                         std::string& model_name,
                                         std::string& new_model_version) {
    d.SetObject();
    add_string(d, "model_name", model_name);
    add_string(d, "model_version", new_model_version);
  }

  std::string get_set_model_version_request_json(
      std::string& model_name, std::string& new_model_version) {
    rapidjson::Document d;
    set_set_model_version_request_doc(d, model_name, new_model_version);
    return to_json_string(d);
  }

  std::string get_add_model_links_request_json(
      std::string& name, std::vector<std::string>& model_names) {
    rapidjson::Document d;
    set_add_model_links_request_doc(d, name, model_names);
    return to_json_string(d);
  }

  std::string get_app_json_request_string(std::string& name) {
    rapidjson::Document d;
    d.SetObject();
    add_string(d, "name", name);
    return to_json_string(d);
  }

  std::string get_linked_models_json_request_string(std::string& name) {
    rapidjson::Document d;
    d.SetObject();
    add_string(d, "app_name", name);
    return to_json_string(d);
  }

  RequestHandler rh_;
  std::shared_ptr<redox::Redox> redis_;
  std::shared_ptr<redox::Subscriber> subscriber_;
};

TEST_F(ManagementFrontendTest, TestAddApplicationCorrect) {
  std::string app_name = "myappname";
  std::string input_type = "integers";
  std::string default_output = "4.3";
  std::string add_app_json =
      get_add_app_request_json(app_name, input_type, default_output, 1000);

  ASSERT_NO_THROW(rh_.add_application(add_app_json));
  auto result = get_application(*redis_, app_name);
  // The application table has 4 fields, so we expect to get back a map with 4
  // entries in it (see add_application() in redis.cpp for details on what the
  // fields are).
  ASSERT_EQ(result.size(), static_cast<size_t>(4));
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

TEST_F(ManagementFrontendTest, TestAddDuplicateApplication) {
  std::string app_name = "myappname";
  std::string input_type = "integers";
  std::string default_output = "4.3";
  std::string add_app_json =
      get_add_app_request_json(app_name, input_type, default_output, 1000);

  ASSERT_NO_THROW(rh_.add_application(add_app_json));
  auto result = get_application(*redis_, "myappname");
  // The application table has 4 fields, so we expect to get back a map with 4
  // entries in it (see add_application() in redis.cpp for details on what the
  // fields are).
  ASSERT_EQ(result.size(), static_cast<size_t>(4));

  ASSERT_THROW(rh_.add_application(add_app_json),
               clipper::ManagementOperationError);
}

TEST_F(ManagementFrontendTest, TestGetApplicationCorrect) {
  // Register the application
  std::string name = "my_app_name";
  std::string input_type = "doubles";
  std::string default_output = "my_default_output";
  int latency_slo_micros = 10000;
  std::string add_app_json = get_add_app_request_json(
      name, input_type, default_output, latency_slo_micros);
  ASSERT_NO_THROW(rh_.add_application(add_app_json));

  std::string get_app_json = get_app_json_request_string(name);
  std::string json_response = rh_.get_application(get_app_json);

  rapidjson::Document response_doc;
  response_doc.SetObject();
  parse_json(json_response, response_doc);

  std::string response_name = get_string(response_doc, "name");
  std::string response_input_type = get_string(response_doc, "input_type");
  std::string response_default_output =
      get_string(response_doc, "default_output");
  int response_latency_slo_micros = get_int(response_doc, "latency_slo_micros");
  auto initial_linked_models = get_string_array(response_doc, "linked_models");

  // Confirm that get_application results match values submitted with
  // registration request
  ASSERT_EQ(response_name, name);
  ASSERT_EQ(response_input_type, input_type);
  ASSERT_EQ(response_default_output, default_output);
  ASSERT_EQ(response_latency_slo_micros, latency_slo_micros);
  ASSERT_EQ(initial_linked_models, std::vector<std::string>{});

  std::string model_name = "mymodelname";
  std::string model_version = "4";
  std::string container_name = "container/name";
  std::string model_data_path = "tmp/model";
  std::vector<std::string> labels = {"label1", "label2"};
  std::string add_model_json =
      get_add_model_request_json(model_name, model_version, input_type, labels,
                                 container_name, model_data_path);
  ASSERT_NO_THROW(rh_.add_model(add_model_json));

  // Link models
  std::vector<std::string> model_names = std::vector<std::string>{model_name};
  std::string add_links_json =
      get_add_model_links_request_json(name, model_names);
  ASSERT_NO_THROW(rh_.add_model_links(add_links_json));

  json_response = rh_.get_application(get_app_json);
  response_doc.SetObject();
  parse_json(json_response, response_doc);

  // Confirm that get_application results show linked models
  auto response_linked_models = get_string_array(response_doc, "linked_models");
  std::sort(response_linked_models.begin(), response_linked_models.end());
  std::sort(model_names.begin(), model_names.end());
  ASSERT_EQ(model_names, response_linked_models);
}

TEST_F(ManagementFrontendTest, TestDeleteApplicationCorrect) {
  std::string app_name = "myappname";
  std::string input_type = "integers";
  std::string default_output = "4.3";
  std::string add_app_json =
      get_add_app_request_json(app_name, input_type, default_output, 1000);
  rh_.add_application(add_app_json);
  std::string del_app_json = get_app_json_request_string(app_name);
  ASSERT_NO_THROW(rh_.delete_application(del_app_json));

  // Check that subsequent calls to application return empty JSON
  std::string json_response = rh_.get_application(del_app_json);
  std::string expected_response = "{}";
  ASSERT_EQ(json_response, expected_response);
}

TEST_F(ManagementFrontendTest, TestGetNonexistentApplicationCorrect) {
  std::string nonexistent_app_name = "nonexistent_app";
  std::string list_apps_json =
      get_app_json_request_string(nonexistent_app_name);
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

  std::string add_app1_json_string = get_add_app_request_json(
      name1, input_type, default_output, latency_slo_micros);
  std::string add_app2_json_string = get_add_app_request_json(
      name2, input_type, default_output, latency_slo_micros);

  ASSERT_NO_THROW(rh_.add_application(add_app1_json_string));
  ASSERT_NO_THROW(rh_.add_application(add_app2_json_string));

  // Add the model to link to
  std::string model_name = "mymodelname";
  std::string model_version = "4";
  std::string container_name = "container/name";
  std::string model_data_path = "tmp/model";
  std::vector<std::string> labels = {"label1", "label2"};
  std::string add_model_json =
      get_add_model_request_json(model_name, model_version, input_type, labels,
                                 container_name, model_data_path);
  ASSERT_NO_THROW(rh_.add_model(add_model_json));

  // Link models to app with name app1
  std::vector<std::string> model_names = std::vector<std::string>{model_name};
  std::string add_links_json =
      get_add_model_links_request_json(name1, model_names);
  ASSERT_NO_THROW(rh_.add_model_links(add_links_json));

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

  std::string app1_response_name = get_string(app1_response_doc, "name");
  std::string app1_response_input_type =
      get_string(app1_response_doc, "input_type");
  std::string app1_response_default_output =
      get_string(app1_response_doc, "default_output");
  int app1_response_latency_slo_micros =
      get_int(app1_response_doc, "latency_slo_micros");
  auto app1_linked_models =
      get_string_array(app1_response_doc, "linked_models");

  std::string app2_response_name = get_string(app2_response_doc, "name");
  std::string app2_response_input_type =
      get_string(app2_response_doc, "input_type");
  std::string app2_response_default_output =
      get_string(app2_response_doc, "default_output");
  int app2_response_latency_slo_micros =
      get_int(app2_response_doc, "latency_slo_micros");
  auto app2_linked_models =
      get_string_array(app2_response_doc, "linked_models");

  ASSERT_EQ(app1_response_name, name1);
  ASSERT_EQ(app1_response_input_type, input_type);
  ASSERT_EQ(app1_response_default_output, default_output);
  ASSERT_EQ(app1_response_latency_slo_micros, latency_slo_micros);
  std::sort(app1_linked_models.begin(), app1_linked_models.end());
  std::sort(model_names.begin(), model_names.end());
  ASSERT_EQ(model_names, app1_linked_models);

  ASSERT_EQ(app2_response_name, name2);
  ASSERT_EQ(app2_response_input_type, input_type);
  ASSERT_EQ(app2_response_default_output, default_output);
  ASSERT_EQ(app2_response_latency_slo_micros, latency_slo_micros);
  ASSERT_EQ(app2_linked_models, std::vector<std::string>{});
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

  std::string add_app1_json_string = get_add_app_request_json(
      name1, input_type, default_output, latency_slo_micros);
  std::string add_app2_json_string = get_add_app_request_json(
      name2, input_type, default_output, latency_slo_micros);

  ASSERT_NO_THROW(rh_.add_application(add_app1_json_string));
  ASSERT_NO_THROW(rh_.add_application(add_app2_json_string));

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
    "model_version": "4",
    "labels": ["label1", "label2", "label3"],
    "input_type": "integers",
    "batch_size": -1,
    "container_name": "clipper/sklearn_cifar",
    "model_data_path": "/tmp/model/repo/m/1"
  }
  )";

  ASSERT_NO_THROW(rh_.add_model(add_model_json));
  std::string model_name = "mymodelname";
  std::string model_version = "4";
  auto result = get_model(*redis_, VersionedModelId(model_name, model_version));
  // The model table has 9 fields, so we expect to get back a map with 9
  // entries in it (see add_model() in redis.cpp for details on what the
  // fields are).
  ASSERT_EQ(result.size(), static_cast<size_t>(9));

  // Make sure that the current model version has been updated
  // appropriately.
  ASSERT_EQ(*get_current_model_version(*redis_, model_name), model_version);
}

TEST_F(ManagementFrontendTest, TestAddLinkedModelCompatibleInputType) {
  std::string app_name = "myappname";
  std::string input_type = "integers";
  std::string default_output = "4.3";
  std::string add_app_json =
      get_add_app_request_json(app_name, input_type, default_output, 1000);

  ASSERT_NO_THROW(rh_.add_application(add_app_json));

  std::string add_model_json = R"(
  {
    "model_name": "mymodelname",
    "model_version": "4",
    "labels": ["label1", "label2", "label3"],
    "input_type": "integers",
    "batch_size": -1,
    "container_name": "clipper/sklearn_cifar",
    "model_data_path": "/tmp/model/repo/m/4"
  }
  )";

  ASSERT_NO_THROW(rh_.add_model(add_model_json));
  std::string model_name = "mymodelname";
  std::string model_version = "4";
  auto result = get_model(*redis_, VersionedModelId(model_name, model_version));
  // The model table has 9 fields, so we expect to get back a map with 9
  // entries in it (see add_model() in redis.cpp for details on what the
  // fields are).
  ASSERT_EQ(result.size(), static_cast<size_t>(9));

  // Make sure that the current model version has been updated
  // appropriately.
  ASSERT_EQ(*get_current_model_version(*redis_, model_name), model_version);
  std::vector<std::string> model_names{model_name};

  std::string add_links_json =
      get_add_model_links_request_json(app_name, model_names);

  ASSERT_NO_THROW(rh_.add_model_links(add_links_json));

  std::string add_new_model_json = R"(
  {
    "model_name": "mymodelname",
    "model_version": "6",
    "labels": ["label1", "label5"],
    "input_type": "ints",
    "batch_size": -1,
    "container_name": "clipper/other_container",
    "model_data_path": "/tmp/model/repo/m/4"
  }
  )";
  ASSERT_NO_THROW(rh_.add_model(add_new_model_json));
}

TEST_F(ManagementFrontendTest, TestAddDuplicateModelVersion) {
  std::string add_model_json = R"(
  {
    "model_name": "mymodelname",
    "model_version": "4",
    "labels": ["label1", "label2", "label3"],
    "input_type": "integers",
    "batch_size": -1,
    "container_name": "clipper/sklearn_cifar",
    "model_data_path": "/tmp/model/repo/m/4"
  }
  )";

  ASSERT_NO_THROW(rh_.add_model(add_model_json));
  std::string model_name = "mymodelname";
  std::string model_version = "4";
  auto result = get_model(*redis_, VersionedModelId(model_name, model_version));
  // The model table has 9 fields, so we expect to get back a map with 9
  // entries in it (see add_model() in redis.cpp for details on what the
  // fields are).
  ASSERT_EQ(result.size(), static_cast<size_t>(9));

  // Make sure that the current model version has been updated
  // appropriately.
  ASSERT_EQ(*get_current_model_version(*redis_, model_name), model_version);

  std::string add_dup_model_json = R"(
  {
    "model_name": "mymodelname",
    "model_version": "4",
    "labels": ["label1", "label5"],
    "input_type": "doubles",
    "batch_size": -1,
    "container_name": "clipper/other_container",
    "model_data_path": "/tmp/model/repo/m/4"
  }
  )";

  ASSERT_THROW(rh_.add_model(add_dup_model_json),
               clipper::ManagementOperationError);
}

TEST_F(ManagementFrontendTest, TestAddModelMissingField) {
  std::string add_model_json = R"(
  {
    "model_name": "mymodelname",
    "model_version": "4",
    "labels": ["label1", "label2", "label3"]
  }
  )";

  ASSERT_THROW(rh_.add_model(add_model_json), json_semantic_error);
}

TEST_F(ManagementFrontendTest, TestAddModelProhibitedChars) {
  // Valid input values
  std::string model_name = "my_app_name";
  std::string model_version = "my_model_version";
  std::string input_type = "doubles";
  std::vector<std::string> labels = {"label1", "label2"};
  std::string container_name = "example/container_name";
  std::string model_data_path = "/example/model/data/path";

  // Invalid input values
  std::string bad_model_name = model_name + ITEM_PART_CONCATENATOR;
  std::string bad_model_version = ITEM_DELIMITER + model_version;
  std::vector<std::string> bad_labels = {
      "label1", ITEM_PART_CONCATENATOR + "label" + ITEM_DELIMITER};

  // Test adding model with invalid model_name value
  rapidjson::Document doc;
  set_add_model_request_doc(doc, bad_model_name, model_version, input_type,
                            labels, container_name, model_data_path);
  std::string add_model_json_string = to_json_string(doc);
  ASSERT_THROW(rh_.add_model(add_model_json_string),
               clipper::ManagementOperationError);
  add_string(doc, "model_name", model_name);

  // Test adding model with invalid model_version value
  add_string(doc, "model_version", bad_model_version);
  add_model_json_string = to_json_string(doc);
  ASSERT_THROW(rh_.add_model(add_model_json_string),
               clipper::ManagementOperationError);
  add_string(doc, "model_version", model_version);

  // Test adding model with invalid labels value
  add_string_array(doc, "labels", bad_labels);
  add_model_json_string = to_json_string(doc);
  ASSERT_THROW(rh_.add_model(add_model_json_string),
               clipper::ManagementOperationError);
  add_string_array(doc, "labels", labels);

  // Confirm that without these invalid inputs, add_model succeeds
  add_model_json_string = to_json_string(doc);
  ASSERT_NO_THROW(rh_.add_model(add_model_json_string));
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
  std::string model_name = "m";
  std::string model_version_1 = "1";
  std::string model_version_2 = "2";
  std::string model_version_4 = "4";
  std::string input_type = "ints";
  std::vector<std::string> labels = {"ads", "images"};
  std::string container_name = "clipper/test_container";
  std::string model_data_path = "/tmp/models/m/1";

  std::string v1_json =
      get_add_model_request_json(model_name, model_version_1, input_type,
                                 labels, container_name, model_data_path);
  std::string v2_json =
      get_add_model_request_json(model_name, model_version_2, input_type,
                                 labels, container_name, model_data_path);
  std::string v4_json =
      get_add_model_request_json(model_name, model_version_4, input_type,
                                 labels, container_name, model_data_path);
  ASSERT_NO_THROW(rh_.add_model(v1_json));
  ASSERT_NO_THROW(rh_.add_model(v2_json));
  ASSERT_NO_THROW(rh_.add_model(v4_json));

  ASSERT_EQ(*get_current_model_version(*redis_, model_name), model_version_4);

  std::string set_model_version_json =
      get_set_model_version_request_json(model_name, model_version_2);
  ASSERT_NO_THROW(rh_.set_model_version(set_model_version_json));
  ASSERT_EQ(*get_current_model_version(*redis_, model_name), model_version_2);
}

TEST_F(ManagementFrontendTest, TestSetVersionNonexistentModel) {
  std::string model_name = "m";
  std::string model_version = "1";

  std::string set_model_version_json =
      get_set_model_version_request_json(model_name, model_version);
  ASSERT_THROW(rh_.set_model_version(set_model_version_json),
               clipper::ManagementOperationError);
}

TEST_F(ManagementFrontendTest, TestSetModelInvalidVersion) {
  std::string model_name = "m";
  std::string model_version_1 = "1";
  std::string model_version_2 = "2";
  std::string model_version_4 = "4";
  std::string input_type = "ints";
  std::vector<std::string> labels = {"ads", "images"};
  std::string container_name = "clipper/test_container";
  std::string model_data_path = "/tmp/models/m/1";

  std::string v1_json =
      get_add_model_request_json(model_name, model_version_1, input_type,
                                 labels, container_name, model_data_path);
  std::string v2_json =
      get_add_model_request_json(model_name, model_version_2, input_type,
                                 labels, container_name, model_data_path);
  std::string v4_json =
      get_add_model_request_json(model_name, model_version_4, input_type,
                                 labels, container_name, model_data_path);
  ASSERT_NO_THROW(rh_.add_model(v1_json));
  ASSERT_NO_THROW(rh_.add_model(v2_json));
  ASSERT_NO_THROW(rh_.add_model(v4_json));

  ASSERT_EQ(*get_current_model_version(*redis_, model_name), model_version_4);

  std::string nonexistent_model_version = "11";
  std::string set_model_version_json =
      get_set_model_version_request_json(model_name, nonexistent_model_version);
  ASSERT_THROW(rh_.set_model_version(set_model_version_json),
               clipper::ManagementOperationError);
  ASSERT_EQ(*get_current_model_version(*redis_, model_name), model_version_4);
}

TEST_F(ManagementFrontendTest, TestAddModelLinkCorrect) {
  std::string app_name = "myappname";
  std::string input_type = "integers";
  std::string default_output = "4.3";
  std::string add_app_json =
      get_add_app_request_json(app_name, input_type, default_output, 1000);
  ASSERT_NO_THROW(rh_.add_application(add_app_json));

  std::string model_name = "mymodelname";
  std::string model_version = "4";
  std::string container_name = "container/name";
  std::string model_data_path = "tmp/model";
  std::vector<std::string> labels = {"l1", "l2"};
  std::string add_model_json =
      get_add_model_request_json(model_name, model_version, input_type, labels,
                                 container_name, model_data_path);
  ASSERT_NO_THROW(rh_.add_model(add_model_json));

  std::vector<std::string> model_names = std::vector<std::string>{model_name};
  std::string add_links_json =
      get_add_model_links_request_json(app_name, model_names);

  ASSERT_NO_THROW(rh_.add_model_links(add_links_json));
  std::vector<std::string> result = get_linked_models(*redis_, app_name);

  // We're storing model names as a set, so redis doesn't guarantee order
  // Sorting will matter once we allow having multiple links per app
  std::sort(model_names.begin(), model_names.end());
  std::sort(result.begin(), result.end());
  ASSERT_EQ(model_names, result);
}

TEST_F(ManagementFrontendTest, TestAddModelLinkIncompatibleInputType) {
  std::string app_name = "myappname";
  std::string app_input_type = "integers";
  std::string default_output = "4.3";
  std::string add_app_json =
      get_add_app_request_json(app_name, app_input_type, default_output, 1000);
  ASSERT_NO_THROW(rh_.add_application(add_app_json));

  std::string model_name = "mymodelname";
  std::string model_version = "4";
  std::string model_input_type = "doubles";
  std::string container_name = "container/name";
  std::string model_data_path = "tmp/model";
  std::vector<std::string> labels = {"l1", "l2"};
  std::string add_model_json =
      get_add_model_request_json(model_name, model_version, model_input_type,
                                 labels, container_name, model_data_path);
  ASSERT_NO_THROW(rh_.add_model(add_model_json));

  std::vector<std::string> model_names = std::vector<std::string>{model_name};
  std::string add_links_json =
      get_add_model_links_request_json(app_name, model_names);

  ASSERT_THROW(rh_.add_model_links(add_links_json),
               clipper::ManagementOperationError);
}

TEST_F(ManagementFrontendTest, TestAddNewLinkedModelIncompatibleInputType) {
  std::string app_name = "myappname";
  std::string input_type = "integers";
  std::string default_output = "4.3";
  std::string add_app_json =
      get_add_app_request_json(app_name, input_type, default_output, 1000);
  ASSERT_NO_THROW(rh_.add_application(add_app_json));

  std::string model_name = "mymodelname";
  std::string model_version = "4";
  std::string container_name = "container/name";
  std::string model_data_path = "tmp/model";
  std::vector<std::string> labels = {"l1", "l2"};
  std::string add_compatible_model_json =
      get_add_model_request_json(model_name, model_version, input_type, labels,
                                 container_name, model_data_path);
  ASSERT_NO_THROW(rh_.add_model(add_compatible_model_json));

  std::vector<std::string> model_names = std::vector<std::string>{model_name};
  std::string add_links_json =
      get_add_model_links_request_json(app_name, model_names);

  ASSERT_NO_THROW(rh_.add_model_links(add_links_json));

  std::string incompatible_model_input_type = "doubles";
  std::string add_incompatible_model_json = get_add_model_request_json(
      model_name, model_version, incompatible_model_input_type, labels,
      container_name, model_data_path);
  ASSERT_THROW(rh_.add_model(add_incompatible_model_json),
               clipper::ManagementOperationError);
}

TEST_F(ManagementFrontendTest,
       TestSetModellVersionForLinkedModelIncompatibleInputType) {
  std::string app_name = "myappname";
  std::string input_type = "integers";
  std::string default_output = "4.3";
  std::string add_app_json =
      get_add_app_request_json(app_name, input_type, default_output, 1000);
  ASSERT_NO_THROW(rh_.add_application(add_app_json));

  std::string incompatible_model_input_type = "doubles";
  std::string compatible_model_version = "2";
  std::string model_name = "mymodelname";
  std::string incompatible_model_version = "1";
  std::string container_name = "container/name";
  std::string model_data_path = "tmp/model";
  std::vector<std::string> labels = {"l1", "l2"};

  std::string add_incompatible_model_json = get_add_model_request_json(
      model_name, incompatible_model_version, incompatible_model_input_type,
      labels, container_name, model_data_path);
  std::string add_compatible_model_json = get_add_model_request_json(
      model_name, compatible_model_version, input_type, labels, container_name,
      model_data_path);
  ASSERT_NO_THROW(rh_.add_model(add_incompatible_model_json));
  ASSERT_NO_THROW(rh_.add_model(add_compatible_model_json));

  std::vector<std::string> model_names = std::vector<std::string>{model_name};
  std::string add_links_json =
      get_add_model_links_request_json(app_name, model_names);
  ASSERT_NO_THROW(rh_.add_model_links(add_links_json));

  std::string set_model_version_json = get_set_model_version_request_json(
      model_name, incompatible_model_version);
  ASSERT_THROW(rh_.set_model_version(set_model_version_json),
               clipper::ManagementOperationError);
}

TEST_F(ManagementFrontendTest, TestAddModelLinkWithNonexistentModel) {
  std::string app_name = "myappname";
  std::string input_type = "integers";
  std::string default_output = "4.3";
  std::string add_app_json =
      get_add_app_request_json(app_name, input_type, default_output, 1000);
  ASSERT_NO_THROW(rh_.add_application(add_app_json));

  std::string model_name = "mymodelname";

  std::vector<std::string> model_names = std::vector<std::string>{model_name};
  std::string add_links_json =
      get_add_model_links_request_json(app_name, model_names);

  ASSERT_THROW(rh_.add_model_links(add_links_json),
               clipper::ManagementOperationError);
}

TEST_F(ManagementFrontendTest, TestAddModelLinkWhenAlreadyExists) {
  std::string app_name = "myappname";
  std::string input_type = "integers";
  std::string default_output = "4.3";
  std::string add_app_json =
      get_add_app_request_json(app_name, input_type, default_output, 1000);
  ASSERT_NO_THROW(rh_.add_application(add_app_json));

  std::string model_name = "mymodelname";
  std::string model_version = "4";
  std::string container_name = "container/name";
  std::string model_data_path = "tmp/model";
  std::vector<std::string> labels = {"label1", "label2"};
  std::string add_model_json =
      get_add_model_request_json(model_name, model_version, input_type, labels,
                                 container_name, model_data_path);
  ASSERT_NO_THROW(rh_.add_model(add_model_json));

  std::vector<std::string> model_names = std::vector<std::string>{model_name};
  std::string add_links_json =
      get_add_model_links_request_json(app_name, model_names);
  ASSERT_NO_THROW(rh_.add_model_links(add_links_json));

  // Attempts to re-add already existing links should fail
  ASSERT_THROW(rh_.add_model_links(add_links_json),
               clipper::ManagementOperationError);

  std::vector<std::string> new_model_names =
      std::vector<std::string>{"mymodelname2"};
  std::string add_new_links_json =
      get_add_model_links_request_json(app_name, new_model_names);

  // When adding a new, second link, it should break
  ASSERT_THROW(rh_.add_model_links(add_new_links_json),
               clipper::ManagementOperationError);
}

TEST_F(ManagementFrontendTest, TestAddModelLinkToNonexistentApp) {
  std::string model_name = "mymodelname";
  std::string model_version = "4";
  std::string container_name = "container/name";
  std::string model_data_path = "tmp/model";
  std::string input_type = "integers";
  std::vector<std::string> labels = {"label1", "label2"};
  std::string add_model_json =
      get_add_model_request_json(model_name, model_version, input_type, labels,
                                 container_name, model_data_path);
  ASSERT_NO_THROW(rh_.add_model(add_model_json));

  std::string app_name = "myappname";
  std::vector<std::string> model_names = std::vector<std::string>{model_name};
  std::string add_links_json =
      get_add_model_links_request_json(app_name, model_names);

  ASSERT_THROW(rh_.add_model_links(add_links_json),
               clipper::ManagementOperationError);
}

TEST_F(ManagementFrontendTest, TestAddModelLinkMalformedJson) {
  std::string malformed_add_links_json = R"(
    {
      "app_name": "myappname"
      "model_names": ["mymodelname"]
    }
  )";
  ASSERT_THROW(rh_.add_model_links(malformed_add_links_json), json_parse_error);
}

TEST_F(ManagementFrontendTest, TestAddModelLinkMissingField) {
  std::string missing_field_add_links_json = R"(
    {
      "app_name": "myappname"
    }
  )";
  ASSERT_THROW(rh_.add_model_links(missing_field_add_links_json),
               json_semantic_error);
}

TEST_F(ManagementFrontendTest, TestGetModelLinks) {
  // Add the app
  std::string app_name = "myappname";
  std::string input_type = "integers";
  std::string default_output = "4.3";
  std::string add_app_json =
      get_add_app_request_json(app_name, input_type, default_output, 1000);
  ASSERT_NO_THROW(rh_.add_application(add_app_json));

  // Confirm that no links exist
  std::string get_linked_models_json =
      get_linked_models_json_request_string(app_name);
  auto result = rh_.get_linked_models(get_linked_models_json);
  ASSERT_EQ("[]", result);

  // Add the model
  std::string model_name = "mymodelname";
  std::string model_version = "4";
  std::string container_name = "container/name";
  std::string model_data_path = "tmp/model";
  std::vector<std::string> labels = {"label1", "label2"};
  std::string add_model_json =
      get_add_model_request_json(model_name, model_version, input_type, labels,
                                 container_name, model_data_path);
  ASSERT_NO_THROW(rh_.add_model(add_model_json));

  // Add the link
  std::vector<std::string> model_names = std::vector<std::string>{model_name};
  std::string add_links_json =
      get_add_model_links_request_json(app_name, model_names);
  ASSERT_NO_THROW(rh_.add_model_links(add_links_json));

  result = rh_.get_linked_models(get_linked_models_json);
  rapidjson::Document d;
  parse_json(result, d);
  auto result_array = to_string_array(d);

  std::sort(model_names.begin(), model_names.end());
  std::sort(result_array.begin(), result_array.end());
  ASSERT_EQ(model_names, result_array);
}

TEST_F(ManagementFrontendTest, TestGetModelLinksForNonexistentApp) {
  std::string app_name = "myappname";
  std::string get_linked_models_json =
      get_linked_models_json_request_string(app_name);
  ASSERT_THROW(rh_.get_linked_models(get_linked_models_json),
               clipper::ManagementOperationError);
}

TEST_F(ManagementFrontendTest, TestGetModelLinksMalformedJson) {
  std::string malformed_get_linked_models_json = R"(
    {
      app_name : "a
    }
  )";
  ASSERT_THROW(rh_.get_linked_models(malformed_get_linked_models_json),
               json_parse_error);
}

TEST_F(ManagementFrontendTest, TestDeleteVersionedModelCorrect) {
  std::string add_model_json = R"(
  {
    "model_name": "test_delete_versioned_model_correct",
    "model_version": "1",
    "labels": ["label1", "label2", "label3"],
    "input_type": "integers",
    "batch_size": -1,
    "container_name": "clipper/sklearn_cifar",
    "model_data_path": "/tmp/model/repo/m/1"
  }
  )";

  ASSERT_NO_THROW(rh_.add_model(add_model_json));
  std::string model_name = "test_delete_versioned_model_correct";
  std::string model_version = "1";
  auto res1 = get_model(*redis_, VersionedModelId(model_name, model_version));
  // The model table has 9 fields, so we expect to get back a map with 9
  // entries in it (see add_model() in redis.cpp for details on what the
  // fields are).
  ASSERT_EQ(res1.size(), static_cast<size_t>(9));

  // Make sure that the current model version has been updated
  // appropriately.
  ASSERT_EQ(*get_current_model_version(*redis_, model_name), model_version);

  std::string delete_versioned_model_json = R"(
  {
    "model_name": "test_delete_versioned_model_correct",
    "model_version": "1"
  }
  )";

  ASSERT_NO_THROW(rh_.delete_versioned_model(delete_versioned_model_json));

  // Check that the model is invalid or not.
  auto res2 = get_model(*redis_, VersionedModelId(model_name, model_version));
  ASSERT_EQ(res2.find("valid"), res2.end());
}

TEST_F(ManagementFrontendTest, TestDeleteVersionedModelMissingField) {
  std::string add_model_json = R"(
  {
    "model_name": "test_delete_versioned_model_missing_field",
    "model_version": "1",
    "labels": ["label1", "label2", "label3"],
    "input_type": "integers",
    "batch_size": -1,
    "container_name": "clipper/sklearn_cifar",
    "model_data_path": "/tmp/model/repo/m/1"
  }
  )";

  ASSERT_NO_THROW(rh_.add_model(add_model_json));
  std::string model_name = "test_delete_versioned_model_missing_field";
  std::string model_version = "1";
  auto result = get_model(*redis_, VersionedModelId(model_name, model_version));
  // The model table has 9 fields, so we expect to get back a map with 9
  // entries in it (see add_model() in redis.cpp for details on what the
  // fields are).
  ASSERT_EQ(result.size(), static_cast<size_t>(9));

  // Make sure that the current model version has been updated
  // appropriately.
  ASSERT_EQ(*get_current_model_version(*redis_, model_name), model_version);

  // model_version is omitted.
  std::string delete_versioned_model_json = R"(
  {
    "model_name": "test_delete_versioned_model_missing_field"
  }
  )";

  ASSERT_THROW(rh_.delete_versioned_model(delete_versioned_model_json),
               json_semantic_error);
}

TEST_F(ManagementFrontendTest, TestDeleteVersionedModelForNonexistentModel) {
  std::string add_model_json = R"(
  {
    "model_name": "test_delete_versioned_model_for_nonexistent_model",
    "model_version": "1",
    "labels": ["label1", "label2", "label3"],
    "input_type": "integers",
    "batch_size": -1,
    "container_name": "clipper/sklearn_cifar",
    "model_data_path": "/tmp/model/repo/m/1"
  }
  )";

  ASSERT_NO_THROW(rh_.add_model(add_model_json));
  std::string model_name = "test_delete_versioned_model_for_nonexistent_model";
  std::string model_version = "1";
  auto result = get_model(*redis_, VersionedModelId(model_name, model_version));
  // The model table has 9 fields, so we expect to get back a map with 9
  // entries in it (see add_model() in redis.cpp for details on what the
  // fields are).
  ASSERT_EQ(result.size(), static_cast<size_t>(9));

  // Make sure that the current model version has been updated
  // appropriately.
  ASSERT_EQ(*get_current_model_version(*redis_, model_name), model_version);

  // Try to delete wrong versioned model.
  std::string delete_versioned_model_json = R"(
  {
    "model_name": "test_delete_versioned_model_for_nonexistent_model",
    "model_version": "2"
  }
  )";

  ASSERT_THROW(rh_.delete_versioned_model(delete_versioned_model_json),
               clipper::ManagementOperationError);
}

TEST_F(ManagementFrontendTest, TestDeleteVersionedModelForInvalidModel) {
  std::string add_model_json = R"(
  {
    "model_name": "test_delete_versioned_model_for_invalid_model",
    "model_version": "1",
    "labels": ["label1", "label2", "label3"],
    "input_type": "integers",
    "batch_size": -1,
    "container_name": "clipper/sklearn_cifar",
    "model_data_path": "/tmp/model/repo/m/1"
  }
  )";

  ASSERT_NO_THROW(rh_.add_model(add_model_json));
  std::string model_name = "test_delete_versioned_model_for_invalid_model";
  std::string model_version = "1";
  auto result = get_model(*redis_, VersionedModelId(model_name, model_version));
  // The model table has 9 fields, so we expect to get back a map with 9
  // entries in it (see add_model() in redis.cpp for details on what the
  // fields are).
  ASSERT_EQ(result.size(), static_cast<size_t>(9));

  // Make sure that the current model version has been updated
  // appropriately.
  ASSERT_EQ(*get_current_model_version(*redis_, model_name), model_version);

  std::string delete_versioned_model_json = R"(
  {
    "model_name": "test_delete_versioned_model_for_invalid_model",
    "model_version": "1"
  }
  )";

  // Delete the versioned model.
  ASSERT_NO_THROW(rh_.delete_versioned_model(delete_versioned_model_json));

  // Try to delete the same model immediately.
  ASSERT_THROW(rh_.delete_versioned_model(delete_versioned_model_json),
               clipper::ManagementOperationError);
}

}  // namespace
