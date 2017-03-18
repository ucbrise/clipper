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
  RequestHandler rh_;
  std::shared_ptr<redox::Redox> redis_;
  std::shared_ptr<redox::Subscriber> subscriber_;
};

TEST_F(ManagementFrontendTest, TestAddApplicationCorrect) {
  std::string add_app_json = R"(
  {
    "name": "myappname",
    "candidate_models": [
        {"model_name": "image_model", "model_version": 3}],
    "input_type": "integers",
    "selection_policy": "sample_policy",
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
