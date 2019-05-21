#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <boost/optional.hpp>

#include <folly/futures/Future.h>

#include <clipper/config.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/query_processor.hpp>

#include "query_frontend.hpp"

using namespace clipper;
using namespace clipper::redis;
using namespace query_frontend;

namespace {

class MockQueryProcessor {
 public:
  MockQueryProcessor() = default;
  folly::Future<Response> predict(Query query) {
    Response response(query, 3, 5, Output("-1.0", {VersionedModelId("m", "1")}),
                      false, boost::optional<std::string>{});
    return folly::makeFuture(response);
  }
  folly::Future<FeedbackAck> update(FeedbackQuery /*feedback*/) {
    return folly::makeFuture(true);
  }

  std::shared_ptr<StateDB> get_state_table() const {
    return std::shared_ptr<StateDB>();
  }
};

class QueryFrontendTest : public ::testing::Test {
 public:
  RequestHandler<MockQueryProcessor> rh_;
  // MockQueryProcessor qp_;
  std::shared_ptr<redox::Redox> redis_;
  std::shared_ptr<redox::Subscriber> subscriber_;

  QueryFrontendTest()
      : rh_("0.0.0.0",
            QUERY_FRONTEND_PORT,
            DEFAULT_THREAD_POOL_SIZE,
            DEFAULT_TIMEOUT_REQUEST,
            DEFAULT_TIMEOUT_CONTENT),
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

  virtual ~QueryFrontendTest() {
    subscriber_->disconnect();
    redis_->disconnect();
  }
};

TEST_F(QueryFrontendTest, TestDecodeCorrectInputInts) {
  std::string test_json_ints = "{\"input\": [1,2,3,4]}";
  std::vector<folly::Try<Response>> responses =
      rh_.decode_and_handle_predict(test_json_ints, "test", "test_policy",
                                    30000, InputType::Ints)
          .get();
  Response response = responses[0].value();

  Query parsed_query = response.query_;
  std::shared_ptr<IntVector> parsed_input =
      std::dynamic_pointer_cast<IntVector>(parsed_query.input_);
  int* data = get_data(parsed_input).get();
  std::vector<int> parsed_input_data(
      data + parsed_input->start(),
      data + parsed_input->start() + parsed_input->size());

  std::vector<int> expected_input_data{1, 2, 3, 4};
  EXPECT_EQ(parsed_input_data, expected_input_data);
  EXPECT_EQ(parsed_query.label_, "test");
  EXPECT_EQ(parsed_query.latency_budget_micros_, 30000);
  EXPECT_EQ(parsed_query.selection_policy_, "test_policy");
}

TEST_F(QueryFrontendTest, TestDecodeCorrectInputIntsBatch) {
  std::string test_json_ints =
      "{\"input_batch\": [[1, 2], [10, 20], [100, 200]]}";
  std::vector<std::vector<int>> expected_input_data{
      {1, 2}, {10, 20}, {100, 200}};
  std::vector<folly::Try<Response>> responses =
      rh_.decode_and_handle_predict(test_json_ints, "test", "test_policy",
                                    30000, InputType::Ints)
          .get();
  for (size_t index = 0; index < responses.size(); ++index) {
    Response response = responses[index].value();
    Query parsed_query = response.query_;

    std::shared_ptr<IntVector> parsed_input =
        std::dynamic_pointer_cast<IntVector>(parsed_query.input_);
    int* data = get_data(parsed_input).get();
    std::vector<int> parsed_input_data(
        data + parsed_input->start(),
        data + parsed_input->start() + parsed_input->size());

    EXPECT_EQ(parsed_input_data, expected_input_data[index]);
    EXPECT_EQ(parsed_query.label_, "test");
    EXPECT_EQ(parsed_query.latency_budget_micros_, 30000);
    EXPECT_EQ(parsed_query.selection_policy_, "test_policy");
  }
}

TEST_F(QueryFrontendTest, TestDecodeCorrectInputDoubles) {
  std::string test_json_doubles = "{\"input\": [1.4,2.23,3.243242,0.3223424]}";
  std::vector<folly::Try<Response>> responses =
      rh_.decode_and_handle_predict(test_json_doubles, "test", "test_policy",
                                    30000, InputType::Doubles)
          .get();
  Response response = responses[0].value();

  Query parsed_query = response.query_;

  std::shared_ptr<DoubleVector> parsed_input =
      std::dynamic_pointer_cast<DoubleVector>(parsed_query.input_);
  double* data = get_data(parsed_input).get();
  std::vector<double> parsed_input_data(
      data + parsed_input->start(),
      data + parsed_input->start() + parsed_input->size());

  std::vector<double> expected_input_data{1.4, 2.23, 3.243242, 0.3223424};
  EXPECT_EQ(parsed_input_data, expected_input_data);
  EXPECT_EQ(parsed_query.label_, "test");
  EXPECT_EQ(parsed_query.latency_budget_micros_, 30000);
  EXPECT_EQ(parsed_query.selection_policy_, "test_policy");
}

TEST_F(QueryFrontendTest, TestDecodeCorrectInputDoublesBatch) {
  std::string test_json_doubles =
      "{\"input_batch\": [[1.1, 2.2], [10.1, 20.2], [100.1, 200.2]]}";
  std::vector<std::vector<double>> expected_input_data{
      {1.1, 2.2}, {10.1, 20.2}, {100.1, 200.2}};
  std::vector<folly::Try<Response>> responses =
      rh_.decode_and_handle_predict(test_json_doubles, "test", "test_policy",
                                    30000, InputType::Doubles)
          .get();
  for (size_t index = 0; index < responses.size(); ++index) {
    Response response = responses[index].value();
    Query parsed_query = response.query_;

    std::shared_ptr<DoubleVector> parsed_input =
        std::dynamic_pointer_cast<DoubleVector>(parsed_query.input_);
    double* data = get_data(parsed_input).get();
    std::vector<double> parsed_input_data(
        data + parsed_input->start(),
        data + parsed_input->start() + parsed_input->size());

    EXPECT_EQ(parsed_input_data, expected_input_data[index]);
    EXPECT_EQ(parsed_query.label_, "test");
    EXPECT_EQ(parsed_query.latency_budget_micros_, 30000);
    EXPECT_EQ(parsed_query.selection_policy_, "test_policy");
  }
}

TEST_F(QueryFrontendTest, TestDecodeCorrectInputString) {
  std::string test_json_string =
      "{\"input\": \"hello world. This is a test string with "
      "punctionation!@#$Y#;}#\"}";
  std::vector<folly::Try<Response>> responses =
      rh_.decode_and_handle_predict(test_json_string, "test", "test_policy",
                                    30000, InputType::Strings)
          .get();
  Response response = responses[0].value();

  Query parsed_query = response.query_;

  std::shared_ptr<SerializableString> parsed_input =
      std::dynamic_pointer_cast<SerializableString>(parsed_query.input_);
  char* data = get_data(parsed_input).get();
  std::string parsed_input_data(
      data + parsed_input->start(),
      data + parsed_input->start() + parsed_input->size());

  std::string expected_input_data(
      "hello world. This is a test string with punctionation!@#$Y#;}#");
  EXPECT_EQ(parsed_input_data, expected_input_data);
  EXPECT_EQ(parsed_query.label_, "test");
  EXPECT_EQ(parsed_query.latency_budget_micros_, 30000);
  EXPECT_EQ(parsed_query.selection_policy_, "test_policy");
}

TEST_F(QueryFrontendTest, TestDecodeCorrectInputStringBatch) {
  std::string test_json_strings =
      "{\"input_batch\": [ \"this\", \"is\", \"a\", \"test\" ]}";
  std::vector<std::string> expected_input_data{"this", "is", "a", "test"};
  std::vector<folly::Try<Response>> responses =
      rh_.decode_and_handle_predict(test_json_strings, "test", "test_policy",
                                    30000, InputType::Strings)
          .get();
  for (size_t index = 0; index < responses.size(); ++index) {
    Response response = responses[index].value();
    Query parsed_query = response.query_;

    std::shared_ptr<SerializableString> parsed_input =
        std::dynamic_pointer_cast<SerializableString>(parsed_query.input_);
    char* data = get_data(parsed_input).get();
    std::string parsed_input_data(
        data + parsed_input->start(),
        data + parsed_input->start() + parsed_input->size());

    EXPECT_EQ(parsed_input_data, expected_input_data[index]);
    EXPECT_EQ(parsed_query.label_, "test");
    EXPECT_EQ(parsed_query.latency_budget_micros_, 30000);
    EXPECT_EQ(parsed_query.selection_policy_, "test_policy");
  }
}

TEST_F(QueryFrontendTest, TestDecodeMalformedJSON) {
  std::string gibberish_string1 =
      "{\"uid\": 2hkdshfdshffhkj32kjhh{dskjfh32r\"3r32";

  std::string gibberish_string2 =
      "dshfdshffhkj32fsd32jk huf32h, 3 } 24j dskjfh32r\"3r32";

  ASSERT_THROW(
      rh_.decode_and_handle_predict(gibberish_string1, "test", "test_policy",
                                    30000, InputType::Doubles),
      json_parse_error);
  ASSERT_THROW(
      rh_.decode_and_handle_predict(gibberish_string2, "test", "test_policy",
                                    30000, InputType::Strings),
      json_parse_error);
}

TEST_F(QueryFrontendTest, TestDecodeMissingJsonField) {
  std::string json_missing_field =
      "{\"other_field\": [1.4,2.23,3.243242,0.3223424]}";
  ASSERT_THROW(
      rh_.decode_and_handle_predict(json_missing_field, "test", "test_policy",
                                    30000, InputType::Doubles),
      json_semantic_error);
}

TEST_F(QueryFrontendTest, TestDecodeWrongInputType) {
  std::string test_json_doubles =
      "{\"uid\": 23, \"input\": [1.4,2.23,3.243242,0.3223424]}";
  ASSERT_THROW(
      rh_.decode_and_handle_predict(test_json_doubles, "test", "test_policy",
                                    30000, InputType::Ints),
      json_semantic_error);
}

TEST_F(QueryFrontendTest, TestDecodeWrongInputTypeInBatch) {
  std::string test_json_doubles =
      "{\"uid\": 23, \"input_batch\": [[1,2], [3.243242,0.3223424]]}";
  ASSERT_THROW(
      rh_.decode_and_handle_predict(test_json_doubles, "test", "test_policy",
                                    30000, InputType::Ints),
      json_semantic_error);
}

TEST_F(QueryFrontendTest, TestDecodeCorrectUpdate) {
  std::string update_json =
      "{\"uid\": 23, \"input\": [1.4,2.23,3.243242,0.3223424], \"label\": 1.0}";
  FeedbackAck ack =
      rh_.decode_and_handle_update(update_json, "test", {}, "test_policy",
                                   InputType::Doubles)
          .get();

  EXPECT_TRUE(ack);
}

TEST_F(QueryFrontendTest, TestDecodeUpdateMissingField) {
  std::string update_json =
      "{\"uid\": 23, \"input\": [1.4,2.23,3.243242,0.3223424]}";
  ASSERT_THROW(rh_.decode_and_handle_update(update_json, "test", {},
                                            "test_policy", InputType::Doubles),
               json_semantic_error);
}

TEST_F(QueryFrontendTest, TestAddOneApplication) {
  size_t no_apps = rh_.num_applications();
  EXPECT_EQ(no_apps, (size_t)0);
  rh_.add_application("test_app_1", InputType::Doubles, "test_policy", "0.4",
                      30000);
  size_t one_app = rh_.num_applications();
  EXPECT_EQ(one_app, (size_t)1);
}

TEST_F(QueryFrontendTest, TestAddManyApplications) {
  size_t no_apps = rh_.num_applications();
  EXPECT_EQ(no_apps, (size_t)0);

  for (int i = 0; i < 500; ++i) {
    std::string cur_name = "test_app_" + std::to_string(i);
    rh_.add_application(cur_name, InputType::Doubles, "test_policy", "0.4",
                        30000);
  }

  size_t apps = rh_.num_applications();
  EXPECT_EQ(apps, (size_t)500);
}

TEST_F(QueryFrontendTest, TestDeleteOneApplication) {
  size_t no_apps = rh_.num_applications();
  EXPECT_EQ(no_apps, (size_t)0);

  rh_.add_application("test_app_1", InputType::Doubles, "test_policy", "0.4",
                      30000);
  rh_.delete_application("test_app_1");
  no_apps = rh_.num_applications();
  EXPECT_EQ(no_apps, (size_t)0);
}

TEST_F(QueryFrontendTest, TestDeleteManyApplications) {
  size_t no_apps = rh_.num_applications();
  EXPECT_EQ(no_apps, (size_t)0);

  // Add 500 applications and delete half of them
  for (int i = 0; i < 500; ++i) {
    std::string cur_name = "test_app_" + std::to_string(i);
    rh_.add_application(cur_name, InputType::Doubles, "test_policy", "0.4",
                        30000);
  }
  for (int i = 0; i < 250; ++i) {
    std::string cur_name = "test_app_" + std::to_string(i);
    rh_.delete_application(cur_name);
  }

  size_t apps = rh_.num_applications();
  EXPECT_EQ(apps, (size_t)250);
}

TEST_F(QueryFrontendTest,
       TestJsonResponseForSuccessfulPredictionFormattedCorrectly) {
  std::string test_json = "{\"uid\": 1, \"input\": [1,2,3]}";
  std::vector<folly::Try<Response>> responses =
      rh_.decode_and_handle_predict(test_json, "test", "test_policy", 30000,
                                    InputType::Ints)
          .get();
  Response response = responses[0].value();

  std::string json_response = rh_.get_prediction_response_content(response);
  rapidjson::Document parsed_response;
  json::parse_json(json_response, parsed_response);
  ASSERT_TRUE(parsed_response.IsObject());
  ASSERT_TRUE(
      parsed_response.GetObject().HasMember(PREDICTION_RESPONSE_KEY_QUERY_ID));
  ASSERT_TRUE(
      parsed_response.GetObject().HasMember(PREDICTION_RESPONSE_KEY_OUTPUT));
  ASSERT_TRUE(parsed_response.GetObject().HasMember(
      PREDICTION_RESPONSE_KEY_USED_DEFAULT));
  ASSERT_TRUE(parsed_response.GetObject()
                  .FindMember(PREDICTION_RESPONSE_KEY_QUERY_ID)
                  ->value.IsInt());
  ASSERT_TRUE(parsed_response.GetObject()
                  .FindMember(PREDICTION_RESPONSE_KEY_OUTPUT)
                  ->value.IsFloat());
  ASSERT_TRUE(parsed_response.GetObject()
                  .FindMember(PREDICTION_RESPONSE_KEY_USED_DEFAULT)
                  ->value.IsBool());
}

TEST_F(QueryFrontendTest,
       TestJsonResponseForFailedPredictionFormattedCorrectly) {
  std::string test_json = "{\"uid\": 1, \"input\": [1,}";
  try {
    rh_.decode_and_handle_predict(test_json, "test", "test_policy", 30000,
                                  InputType::Ints)
        .get();
    FAIL() << "Expected an error parsing malformed json: " << test_json;
  } catch (json_parse_error& e) {
    std::string json_error_response = rh_.get_prediction_error_response_content(
        PREDICTION_ERROR_NAME_JSON, e.what());
    rapidjson::Document parsed_error_response;
    json::parse_json(json_error_response, parsed_error_response);
    ASSERT_TRUE(parsed_error_response.IsObject());
    ASSERT_TRUE(
        parsed_error_response.HasMember(PREDICTION_ERROR_RESPONSE_KEY_ERROR));
    ASSERT_TRUE(
        parsed_error_response.HasMember(PREDICTION_ERROR_RESPONSE_KEY_CAUSE));
    ASSERT_TRUE(
        parsed_error_response.FindMember(PREDICTION_ERROR_RESPONSE_KEY_ERROR)
            ->value.IsString());
    ASSERT_TRUE(
        parsed_error_response.FindMember(PREDICTION_ERROR_RESPONSE_KEY_CAUSE)
            ->value.IsString());
  }
}

TEST_F(QueryFrontendTest, TestReadApplicationsAtStartup) {
  // Add a few applications
  std::string name = "my_app_name";
  InputType input_type = InputType::Doubles;
  std::string policy = "exp3_policy";
  std::string default_output = "1.0";
  int latency_slo_micros = 10000;
  ASSERT_TRUE(add_application(*redis_, name, input_type, policy, default_output,
                              latency_slo_micros));
  std::string name2 = "my_app_name_2";
  InputType input_type2 = InputType::Doubles;
  std::string policy2 = "exp4_policy";
  int latency_slo_micros2 = 50000;
  std::string default_output2 = "1.0";
  ASSERT_TRUE(add_application(*redis_, name2, input_type2, policy2,
                              default_output2, latency_slo_micros2));

  RequestHandler<MockQueryProcessor> rh2_("127.0.0.1",
                                          QUERY_FRONTEND_PORT,
                                          DEFAULT_THREAD_POOL_SIZE,
                                          DEFAULT_TIMEOUT_REQUEST,
                                          DEFAULT_TIMEOUT_CONTENT);
  size_t two_apps = rh2_.num_applications();
  EXPECT_EQ(two_apps, (size_t)2);
}

TEST_F(QueryFrontendTest, TestReadModelsAtStartup) {
  // Add multiple models (some with multiple versions)
  std::vector<std::string> labels{"ads", "images", "experimental", "other",
                                  "labels"};
  VersionedModelId model1 = VersionedModelId("m", "1");
  std::string container_name = "clipper/test_container";
  std::string model_path = "/tmp/models/m/1";
  ASSERT_TRUE(add_model(*redis_, model1, InputType::Ints, labels,
                        container_name, model_path, DEFAULT_BATCH_SIZE));
  VersionedModelId model2 = VersionedModelId("m", "2");
  std::string model_path2 = "/tmp/models/m/2";
  ASSERT_TRUE(add_model(*redis_, model2, InputType::Ints, labels,
                        container_name, model_path2, DEFAULT_BATCH_SIZE));
  VersionedModelId model3 = VersionedModelId("n", "3");
  std::string model_path3 = "/tmp/models/n/3";
  ASSERT_TRUE(add_model(*redis_, model3, InputType::Ints, labels,
                        container_name, model_path3, DEFAULT_BATCH_SIZE));

  // Set m@v2 and n@v3 as current model versions
  set_current_model_version(*redis_, "m", "2");
  set_current_model_version(*redis_, "n", "3");
  std::unordered_map<std::string, std::string> expected_models = {{"m", "2"},
                                                                  {"n", "3"}};

  RequestHandler<MockQueryProcessor> rh2_("127.0.0.1",
                                          QUERY_FRONTEND_PORT,
                                          DEFAULT_THREAD_POOL_SIZE,
                                          DEFAULT_TIMEOUT_REQUEST,
                                          DEFAULT_TIMEOUT_CONTENT);
  EXPECT_EQ(rh2_.get_current_model_versions(), expected_models);
}

TEST_F(QueryFrontendTest, TestReadModelLinksAtStartup) {
  // Add a few applications
  std::string app_name_1 = "my_app_name";
  std::string app_name_2 = "my_app_name_2";
  InputType input_type = InputType::Doubles;
  std::string policy = "exp3_policy";
  std::string default_output = "1.0";
  int latency_slo_micros = 10000;
  ASSERT_TRUE(add_application(*redis_, app_name_1, input_type, policy,
                              default_output, latency_slo_micros));
  ASSERT_TRUE(add_application(*redis_, app_name_2, input_type, policy,
                              default_output, latency_slo_micros));

  // Give some candidate model names to app with `app_name_1`
  add_model_links(*redis_, app_name_1, {"m1"});
  add_model_links(*redis_, app_name_1, {"m2", "m3"});

  std::vector<std::string> expected_app1_linked_models = {"m1", "m2", "m3"};

  RequestHandler<MockQueryProcessor> rh2_("127.0.0.1",
                                          QUERY_FRONTEND_PORT,
                                          DEFAULT_THREAD_POOL_SIZE,
                                          DEFAULT_TIMEOUT_REQUEST,
                                          DEFAULT_TIMEOUT_CONTENT);

  std::vector<std::string> app1_linked_models =
      rh2_.get_linked_models_for_app(app_name_1);
  std::sort(app1_linked_models.begin(), app1_linked_models.end());
  std::sort(expected_app1_linked_models.begin(),
            expected_app1_linked_models.end());
  EXPECT_EQ(expected_app1_linked_models, app1_linked_models);

  // App with name `app_name_2` shouldn't have any linked models
  EXPECT_EQ(rh2_.get_linked_models_for_app(app_name_2),
            std::vector<std::string>{});
}

TEST_F(QueryFrontendTest, TestReadInvalidModelVersionAtStartup) {
  std::vector<std::string> labels{"ads", "images", "experimental", "other",
                                  "labels"};
  VersionedModelId model1 = VersionedModelId("m", "1");
  std::string container_name = "clipper/test_container";
  std::string model_path = "/tmp/models/m/1";
  ASSERT_TRUE(add_model(*redis_, model1, InputType::Ints, labels,
                        container_name, model_path, DEFAULT_BATCH_SIZE));
  // Not setting the version number will cause get_current_model_version()
  // to return -1, and the RequestHandler should then throw a runtime_error.
  ASSERT_THROW(RequestHandler<MockQueryProcessor>("127.0.0.1",
                                                  QUERY_FRONTEND_PORT,
                                                  DEFAULT_THREAD_POOL_SIZE,
                                                  DEFAULT_TIMEOUT_REQUEST,
                                                  DEFAULT_TIMEOUT_CONTENT),
               std::runtime_error);
}

}  // namespace
