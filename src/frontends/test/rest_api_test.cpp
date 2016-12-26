#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <boost/thread.hpp>
#define BOOST_SPIRIT_THREADSAFE
#include <boost/property_tree/ptree.hpp>

#include <clipper/datatypes.hpp>
#include <clipper/query_processor.hpp>
#include <frontends/rest.hpp>

using namespace boost::property_tree;
using clipper::DoubleVector;
using clipper::Feedback;
using clipper::FeedbackAck;
using clipper::FeedbackQuery;
using clipper::Input;
using clipper::Output;
using clipper::Query;
using clipper::QueryProcessorBase;
using clipper::Response;
using clipper::VersionedModelId;

namespace {

class MockQueryProcessor : public QueryProcessorBase {
 public:
  MOCK_METHOD1(predict, boost::future<Response>(Query query));
  MOCK_METHOD1(update, boost::future<FeedbackAck>(FeedbackQuery query));
};

class RestApiTests : public ::testing::Test {
 public:
  RequestHandler rh_;
  MockQueryProcessor qp_;

  RestApiTests() : rh_(qp_, "0.0.0.0", 1337, 8) {}
};

MATCHER_P(QueryEqual, expected_query, "") {
  auto arg_input = std::dynamic_pointer_cast<DoubleVector>(arg.input_);
  auto expected_input =
      std::dynamic_pointer_cast<DoubleVector>(expected_query.input_);
  EXPECT_THAT(arg_input->get_data(),
              testing::ContainerEq(expected_input->get_data()));

  /* Test for equality of other instance variables */
  EXPECT_EQ(arg.label_, expected_query.label_);
  EXPECT_EQ(arg.user_id_, expected_query.user_id_);
  EXPECT_EQ(arg.latency_micros_, expected_query.latency_micros_);
  EXPECT_EQ(arg.selection_policy_, expected_query.selection_policy_);
  EXPECT_THAT(arg.candidate_models_,
              testing::ContainerEq(expected_query.candidate_models_));
  return true;
}

MATCHER_P(FeedbackQueryEqual, expected_fq, "") {
  /* Compare Input and Output */
  auto arg_input = std::dynamic_pointer_cast<DoubleVector>(arg.feedback_.first);
  auto expected_input =
      std::dynamic_pointer_cast<DoubleVector>(expected_fq.feedback_.first);
  Output arg_output = arg.feedback_.second;
  Output expected_output = expected_fq.feedback_.second;
  EXPECT_THAT(arg_input->get_data(),
              testing::ContainerEq(expected_input->get_data()));
  EXPECT_EQ(arg_output.y_hat_, expected_output.y_hat_);
  EXPECT_EQ(arg_output.versioned_model_, expected_output.versioned_model_);

  /* Test for equality of other instance variables */
  EXPECT_EQ(arg.label_, expected_fq.label_);
  EXPECT_EQ(arg.user_id_, expected_fq.user_id_);
  EXPECT_EQ(arg.selection_policy_, expected_fq.selection_policy_);
  EXPECT_THAT(arg.candidate_models_,
              testing::ContainerEq(expected_fq.candidate_models_));
  return true;
}

TEST_F(RestApiTests, BasicInfoTest) {
  // Variables for testing
  std::string app_name = "app";
  long uid = 1;
  std::vector<VersionedModelId> models = {std::make_pair("m", 1),
                                          std::make_pair("n", 2)};
  VersionedModelId model_to_update = std::make_pair("m", 1);
  InputType input_type = double_vec;
  OutputType output_type = double_val;
  std::string selection_policy = "most_recent";
  long latency_micros = 20000;
  std::shared_ptr<Input> input =
      std::make_shared<DoubleVector>(std::vector<double>{1.1, 2.2, 3.3, 4.4});
  Output output = Output(2.0, model_to_update);
  Feedback feedback = std::make_pair(input, output);

  // Make expected Query and FeedbackAck
  std::string predict_json = "{\"uid\": 1, \"input\": [1.1, 2.2, 3.3, 4.4]}";
  Query expected_query =
      Query(app_name, uid, input, latency_micros, selection_policy, models);
  std::string update_json =
      "{\"uid\": 1, \"input\": [1.1, 2.2, 3.3, 4.4], \"label\": 2.0, "
      "\"model_name\": \"m\", \"model_version\": 1}";
  FeedbackQuery expected_fq =
      FeedbackQuery(app_name, uid, feedback, selection_policy, models);

  // Handle predict and update requests
  EXPECT_CALL(qp_, predict(QueryEqual(expected_query)));
  rh_.decode_and_handle_predict(predict_json, qp_, app_name, models,
                                selection_policy, latency_micros, input_type);
  EXPECT_CALL(qp_, update(FeedbackQueryEqual(expected_fq)));
  rh_.decode_and_handle_update(update_json, qp_, app_name, models,
                               selection_policy, input_type, output_type);
}

TEST_F(RestApiTests, MalformedJsonTest) {
  std::string app_name = "app";
  std::vector<VersionedModelId> models = {std::make_pair("m", 1),
                                          std::make_pair("n", 2)};
  InputType input_type = double_vec;
  OutputType output_type = double_val;
  std::string selection_policy = "most_recent";
  long latency_micros = 20000;

  std::string empty_str = "";
  std::string malformed_json = "{\"uid\": 1, input\": [1.1, 2.2, 3.3, 4.4]}";

  try {
    rh_.decode_and_handle_predict(empty_str, qp_, app_name, models,
                                  selection_policy, latency_micros, input_type);
    FAIL() << "predict function failed to throw exception on empty json";
  } catch (const ptree_error& e) {
  }

  try {
    rh_.decode_and_handle_predict(malformed_json, qp_, app_name, models,
                                  selection_policy, latency_micros, input_type);
    FAIL() << "predict function failed to throw exception on malformed json";
  } catch (const ptree_error& e) {
  }

  try {
    rh_.decode_and_handle_update(empty_str, qp_, app_name, models,
                                 selection_policy, input_type, output_type);
    FAIL() << "update function failed to throw exception on empty json";
  } catch (const ptree_error& e) {
  }

  try {
    rh_.decode_and_handle_update(malformed_json, qp_, app_name, models,
                                 selection_policy, input_type, output_type);
    FAIL() << "update function failed to throw exception on malformed json";
  } catch (const ptree_error& e) {
  }
}

std::string kv_pairs_to_json(
    std::vector<std::pair<std::string, std::string>> kv_pairs) {
  std::stringstream ss;
  ss << "{";
  for (std::size_t i = 0; i < kv_pairs.size(); i++) {
    auto kv_pair = kv_pairs[i];
    ss << kv_pair.first << ": " << kv_pair.second;
    if (i < kv_pairs.size() - 1) {
      ss << ", ";
    }
  }
  ss << "}";
  return ss.str();
}

std::vector<std::string> generate_missing_json_subsets(
    std::vector<std::pair<std::string, std::string>> kv_pairs) {
  auto json_strings = std::vector<std::string>();
  // create all possible subsets containing one less pair than total
  for (std::size_t i = 0; i < kv_pairs.size(); i++) {
    auto temp_json_pairs = std::vector<std::pair<std::string, std::string>>();
    for (std::size_t j = 0; j < kv_pairs.size(); j++) {
      if (j != i) {
        temp_json_pairs.push_back(kv_pairs[j]);
      }
    }
    std::string missing_json_string = kv_pairs_to_json(temp_json_pairs);
    json_strings.push_back(missing_json_string);
  }
  return json_strings;
}

TEST_F(RestApiTests, MissingKeysTest) {
  std::string app_name = "app";
  std::vector<VersionedModelId> models = {std::make_pair("m", 1),
                                          std::make_pair("n", 2)};
  InputType input_type = double_vec;
  OutputType output_type = double_val;
  std::string selection_policy = "most_recent";
  long latency_micros = 20000;

  std::vector<std::pair<std::string, std::string>> predict_kv_pairs = {
      std::make_pair("\"uid\"", "1"),
      std::make_pair("\"input\"", "[1.1, 2.2, 3.3, 4.4]"),
  };
  std::vector<std::pair<std::string, std::string>> update_kv_pairs = {
      std::make_pair("\"uid\"", "1"),
      std::make_pair("\"input\"", "[1.1, 2.2, 3.3, 4.4]"),
      std::make_pair("\"label\"", "2.0"),
      std::make_pair("\"model_name\"", "\"m\""),
      std::make_pair("\"model_version\"", "1")};

  // These two calls should pass since the json is well-formatted
  EXPECT_CALL(qp_, predict(testing::_));
  rh_.decode_and_handle_predict(kv_pairs_to_json(predict_kv_pairs), qp_,
                                app_name, models, selection_policy,
                                latency_micros, input_type);
  EXPECT_CALL(qp_, update(testing::_));
  rh_.decode_and_handle_update(kv_pairs_to_json(update_kv_pairs), qp_, app_name,
                               models, selection_policy, input_type,
                               output_type);

  auto predict_json_strings = generate_missing_json_subsets(predict_kv_pairs);
  for (std::size_t i = 0; i < predict_json_strings.size(); i++) {
    std::string predict_json = predict_json_strings[i];
    try {
      rh_.decode_and_handle_predict(predict_json, qp_, app_name, models,
                                    selection_policy, latency_micros,
                                    input_type);
      FAIL() << "predict function failed missing keys test";
    } catch (const ptree_error& e) {
    }
  }

  auto update_json_strings = generate_missing_json_subsets(update_kv_pairs);
  for (std::size_t i = 0; i < update_json_strings.size(); i++) {
    std::string update_json = update_json_strings[i];
    try {
      rh_.decode_and_handle_update(update_json, qp_, app_name, models,
                                   selection_policy, input_type, output_type);
      FAIL() << "update function failed missing keys test";
    } catch (const ptree_error& e) {
    }
  }
}

std::pair<std::string, std::string> replace_kv_pair(
    std::vector<std::pair<std::string, std::string>>& kv_pairs, std::string key,
    std::string value) {
  for (std::size_t i = 0; i < kv_pairs.size(); i++) {
    if (kv_pairs[i].first.compare(key) == 0) {
      std::pair<std::string, std::string> saved_pair = kv_pairs[i];
      kv_pairs[i] = std::make_pair(key, value);
      return saved_pair;
    }
  }
  return std::make_pair("", "");
}

TEST_F(RestApiTests, MismatchedTypesTest) {
  std::string app_name = "app";
  std::vector<VersionedModelId> models = {std::make_pair("m", 1),
                                          std::make_pair("n", 2)};
  InputType input_type = double_vec;
  OutputType output_type = double_val;
  std::string selection_policy = "most_recent";
  long latency_micros = 20000;

  std::vector<std::pair<std::string, std::string>> predict_kv_pairs = {
      std::make_pair("\"uid\"", "1"),
      std::make_pair("\"input\"", "[1.1, 2.2, 3.3, 4.4]")};
  std::vector<std::pair<std::string, std::string>> update_kv_pairs = {
      std::make_pair("\"uid\"", "1"),
      std::make_pair("\"input\"", "[1.1, 2.2, 3.3, 4.4]"),
      std::make_pair("\"label\"", "2.0"),
      std::make_pair("\"model_name\"", "\"m\""),
      std::make_pair("\"model_version\"", "1")};

  // uid: string instead of int
  // input: wrong types in array
  // input: not array
  // label: not float, string
  // model_name: int instead of string
  // model_version: string instead of int
  // TODO: make the commented tests pass
  std::vector<std::pair<std::string, std::string>> predict_test_cases = {
      std::make_pair("\"uid\"", "\"not a valid uid\""),
      std::make_pair("\"input\"", "[1.1, 2.2, \"elem 3\", 4.4]"),
      // std::make_pair("\"input\"", "1234")
  };
  std::vector<std::pair<std::string, std::string>> update_test_cases = {
      std::make_pair("\"uid\"", "\"not a valid uid\""),
      std::make_pair("\"input\"", "[1.1, 2.2, \"elem 3\", 4.4]"),
      // std::make_pair("\"input\"", "1234"),
      std::make_pair("\"label\"", "\"not a label\""),
      // std::make_pair("\"model_name\"", "1234"),
      std::make_pair("\"model_version\"", "\"m\"")};

  for (std::size_t i = 0; i < predict_test_cases.size(); i++) {
    auto test_pair = predict_test_cases[i];
    auto saved_pair =
        replace_kv_pair(predict_kv_pairs, test_pair.first, test_pair.second);
    std::string predict_json = kv_pairs_to_json(predict_kv_pairs);
    try {
      rh_.decode_and_handle_predict(predict_json, qp_, app_name, models,
                                    selection_policy, latency_micros,
                                    input_type);
      FAIL() << "predict function failed mismatched keys test";
    } catch (const ptree_error& e) {
    }
    replace_kv_pair(predict_kv_pairs, saved_pair.first, saved_pair.second);
  }

  for (std::size_t i = 0; i < update_test_cases.size(); i++) {
    auto test_pair = update_test_cases[i];
    auto saved_pair =
        replace_kv_pair(update_kv_pairs, test_pair.first, test_pair.second);
    std::string update_json = kv_pairs_to_json(update_kv_pairs);
    try {
      rh_.decode_and_handle_update(update_json, qp_, app_name, models,
                                   selection_policy, input_type, output_type);
      FAIL() << "update function failed mismatched keys test";
    } catch (const ptree_error& e) {
    }
    replace_kv_pair(update_kv_pairs, saved_pair.first, saved_pair.second);
  }
}

}  // namespace
