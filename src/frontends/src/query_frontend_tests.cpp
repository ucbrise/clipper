#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <boost/thread.hpp>
#define BOOST_SPIRIT_THREADSAFE

#include <clipper/datatypes.hpp>
#include <clipper/query_processor.hpp>

#include "query_frontend.hpp"

using namespace clipper;

namespace {

class MockQueryProcessor {
 public:
  MockQueryProcessor() = default;
  boost::future<Response> predict(Query query);
  boost::future<FeedbackAck> update(FeedbackQuery feedback);
};

class QueryFrontendTest : public ::testing::Test {
 public:
  RequestHandler<MockQueryProcessor> rh_;
  // MockQueryProcessor qp_;

  QueryFrontendTest() : rh_("0.0.0.0", 1337, 8) {}
};

TEST_F(QueryFrontendTest, TestDecodeCorrectInput) { ASSERT_TRUE(false); }

TEST_F(QueryFrontendTest, TestDecodeMalformedInput) { ASSERT_TRUE(false); }

TEST_F(QueryFrontendTest, TestDecodeCorrectOutpu) { ASSERT_TRUE(false); }

TEST_F(QueryFrontendTest, TestDecodeMalformedOutput) { ASSERT_TRUE(false); }

TEST_F(QueryFrontendTest, TestAddOneApplication) { ASSERT_TRUE(false); }

TEST_F(QueryFrontendTest, TestAddManyApplications) { ASSERT_TRUE(false); }

// MATCHER_P(QueryEqual, expected_query, "") {
//   std::shared_ptr<Input> arg_input = arg.input_;
//   std::shared_ptr<Input> expected_input = expected_query.input_;
//   // For now compare serialized bytes of Inputs
//   EXPECT_THAT(arg_input->serialize(),
//               testing::ContainerEq(expected_input->serialize()));
//
//   #<{(| Test for equality of other instance variables |)}>#
//   EXPECT_EQ(arg.label_, expected_query.label_);
//   EXPECT_EQ(arg.user_id_, expected_query.user_id_);
//   EXPECT_EQ(arg.latency_micros_, expected_query.latency_micros_);
//   EXPECT_EQ(arg.selection_policy_, expected_query.selection_policy_);
//   EXPECT_THAT(arg.candidate_models_,
//               testing::ContainerEq(expected_query.candidate_models_));
//   return true;
// }

// MATCHER_P(FeedbackQueryEqual, expected_fq, "") {
//   #<{(| Compare Input and Output |)}>#
//   std::shared_ptr<Input> arg_input = arg.feedback_.first;
//   std::shared_ptr<Input> expected_input = expected_fq.feedback_.first;
//   Output arg_output = arg.feedback_.second;
//   Output expected_output = expected_fq.feedback_.second;
//   EXPECT_EQ(arg_output.y_hat_, expected_output.y_hat_);
//   EXPECT_EQ(arg_output.versioned_model_, expected_output.versioned_model_);
//   // For now compare serialized bytes of Inputs
//   EXPECT_THAT(arg_input->serialize(),
//               testing::ContainerEq(expected_input->serialize()));
//
//   #<{(| Test for equality of other instance variables |)}>#
//   EXPECT_EQ(arg.label_, expected_fq.label_);
//   EXPECT_EQ(arg.user_id_, expected_fq.user_id_);
//   EXPECT_EQ(arg.selection_policy_, expected_fq.selection_policy_);
//   EXPECT_THAT(arg.candidate_models_,
//               testing::ContainerEq(expected_fq.candidate_models_));
//   return true;
// }

// TEST_F(RestApiTests, BasicInfoTest) {
//   // Variables for testing
//   std::string app_name = "app";
//   long uid = 1;
//   std::vector<VersionedModelId> models = {std::make_pair("m", 1),
//                                           std::make_pair("n", 2)};
//   VersionedModelId model_to_update = std::make_pair("m", 1);
//   InputType input_type = double_vec;
//   OutputType output_type = double_val;
//   std::string selection_policy = "most_recent";
//   long latency_micros = 20000;
//   std::shared_ptr<Input> input =
//       std::make_shared<DoubleVector>(std::vector<double>{1.1, 2.2, 3.3,
//       4.4});
//   Output output = Output(2.0, model_to_update);
//   Feedback feedback = std::make_pair(input, output);
//
//   // Make expected Query and FeedbackAck
//   std::string predict_json = "{\"uid\": 1, \"input\": [1.1, 2.2, 3.3, 4.4]}";
//   Query expected_query =
//       Query(app_name, uid, input, latency_micros, selection_policy, models);
//   std::string update_json =
//       "{\"uid\": 1, \"input\": [1.1, 2.2, 3.3, 4.4], \"label\": 2.0, "
//       "\"model_name\": \"m\", \"model_version\": 1}";
//   FeedbackQuery expected_fq =
//       FeedbackQuery(app_name, uid, feedback, selection_policy, models);
//
//   rh_.add_application(app_name, models, input_type, output_type,
//                       selection_policy, latency_micros);
//   // Handle predict and update requests
//   EXPECT_CALL(qp_, predict(QueryEqual(expected_query)));
//   rh_.decode_and_handle_predict(predict_json, qp_, app_name, models,
//                                 selection_policy, latency_micros,
//                                 input_type);
//   EXPECT_CALL(qp_, update(FeedbackQueryEqual(expected_fq)));
//   rh_.decode_and_handle_update(update_json, qp_, app_name, models,
//                                selection_policy, input_type, output_type);
// }

}  // namespace
