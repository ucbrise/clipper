#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <boost/thread.hpp>
#define BOOST_SPIRIT_THREADSAFE
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <clipper/constants.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/query_processor.hpp>
#include <clipper/test_constants.hpp>

#include "management_frontend.hpp"

using namespace clipper;
using namespace clipper_test;
using namespace clipper::redis;
using namespace management;
using namespace boost::property_tree;

namespace {

class ManagementFrontendTest : public ::testing::Test {
 public:
  ManagementFrontendTest()
      : rh_(MANAGEMENT_FRONTEND_PORT, 1),
        redis_(std::make_shared<redox::Redox>()),
        subscriber_(std::make_shared<redox::Subscriber>()) {
    redis_->connect("localhost", REDIS_TEST_PORT);
    subscriber_->connect("localhost", REDIS_TEST_PORT);

    // delete all keys
    send_cmd_no_reply<std::string>(*redis_, {"FLUSHALL"});

    send_cmd_no_reply<std::string>(
        *redis_, {"CONFIG", "SET", "notify-keyspace-events", "AKE"});
  }

  virtual ~ManagementFrontendTest() {
    redis_->disconnect();
    subscriber_->disconnect();
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
        {"model_name": "m", "model_version": 4},
        {"model_name": "image_model", "model_version": 3}],
    "input_type": "integers",
    "output_type": "double",
    "selection_policy": "sample_policy",
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
    "selection_policy": "sample_policy",
    "latency_slo_micros": 10000
  }
  )";

  ASSERT_THROW(rh_.add_application(add_app_json), ptree_error);
}

TEST_F(ManagementFrontendTest, TestAddApplicationMalformedJson) {
  std::string add_app_json = R"(
  {
    "name": "myappname,
    "input_type": "integers",
    "selection_policy":,
    "latency_slo_micros": 10000
  )";

  ASSERT_THROW(rh_.add_application(add_app_json), ptree_error);
}

TEST_F(ManagementFrontendTest, TestAddModelCorrect) {
  std::string add_model_json = R"(
  {
    "model_name": "mymodelname",
    "model_version": 4,
    "labels": ["label1", "label2", "label3"],
    "input_type": "integers",
    "output_type": "double"
  }
  )";

  ASSERT_EQ(rh_.add_model(add_model_json), "Success!");
  auto result = get_model(*redis_, std::make_pair("mymodelname", 4));
  // The model table has 6 fields, so we expect to get back a map with 6
  // entries in it (see add_model() in redis.cpp for details on what the
  // fields are).
  ASSERT_EQ(result.size(), static_cast<size_t>(6));
}

TEST_F(ManagementFrontendTest, TestAddModelMissingField) {
  std::string add_model_json = R"(
  {
    "model_name": "mymodelname",
    "model_version": 4,
    "labels": ["label1", "label2", "label3"],
    "output_type": "double"
  }
  )";

  ASSERT_THROW(rh_.add_model(add_model_json), ptree_error);
}

TEST_F(ManagementFrontendTest, TestAddModelMalformedJson) {
  std::string add_model_json = R"(
    "model_name": "mymodelname
    "model_version": 4
    "labels": ["label1", "label2", "label3"
    "output_type": "double",
  )";
  ASSERT_THROW(rh_.add_model(add_model_json), ptree_error);
}

}  // namespace
