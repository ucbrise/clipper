#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <boost/thread.hpp>
#define BOOST_SPIRIT_THREADSAFE
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <clipper/constants.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/query_processor.hpp>

#include "query_frontend.hpp"

using namespace clipper;
using namespace query_frontend;
using namespace boost::property_tree;

namespace {

class MockQueryProcessor {
 public:
  MockQueryProcessor() = default;
  boost::future<Response> predict(Query query) {
    Response response(query, 3, 5, Output(-1.0, std::make_pair("m", 1)), {});
    return boost::make_ready_future(response);
  }
  boost::future<FeedbackAck> update(FeedbackQuery /*feedback*/) {
    return boost::make_ready_future(true);
  }
};

class QueryFrontendTest : public ::testing::Test {
 public:
  RequestHandler<MockQueryProcessor> rh_;
  // MockQueryProcessor qp_;

  QueryFrontendTest() : rh_("0.0.0.0", 1337, 8) {}
};

TEST_F(QueryFrontendTest, TestDecodeCorrectInputInts) {
  std::string test_json_ints = "{\"uid\": 23, \"input\": [1,2,3,4]}";
  Response response =
      rh_.decode_and_handle_predict(test_json_ints, "test", {}, "test_policy",
                                    30000, InputType::Ints)
          .get();

  Query parsed_query = response.query_;

  EXPECT_EQ(parsed_query.user_id_, 23);
  const std::vector<int>& parsed_input =
      std::static_pointer_cast<IntVector>(parsed_query.input_)->get_data();
  std::vector<int> expected_input{1, 2, 3, 4};
  EXPECT_EQ(parsed_input, expected_input);
  EXPECT_EQ(parsed_query.label_, "test");
  EXPECT_EQ(parsed_query.latency_micros_, 30000);
  EXPECT_EQ(parsed_query.selection_policy_, "test_policy");
}

TEST_F(QueryFrontendTest, TestDecodeCorrectInputDoubles) {
  std::string test_json_doubles =
      "{\"uid\": 23, \"input\": [1.4,2.23,3.243242,0.3223424]}";
  Response response =
      rh_.decode_and_handle_predict(test_json_doubles, "test", {},
                                    "test_policy", 30000, InputType::Doubles)
          .get();

  Query parsed_query = response.query_;

  EXPECT_EQ(parsed_query.user_id_, 23);
  const std::vector<double>& parsed_input =
      std::static_pointer_cast<DoubleVector>(parsed_query.input_)->get_data();
  std::vector<double> expected_input{1.4, 2.23, 3.243242, 0.3223424};
  EXPECT_EQ(parsed_input, expected_input);
  EXPECT_EQ(parsed_query.label_, "test");
  EXPECT_EQ(parsed_query.latency_micros_, 30000);
  EXPECT_EQ(parsed_query.selection_policy_, "test_policy");
}

TEST_F(QueryFrontendTest, TestDecodeCorrectInputString) {
  std::string test_json_string =
      "{\"uid\": 23, \"input\": \"hello world. This is a test string with "
      "punctionation!@#$Y#;}#\"}";
  Response response =
      rh_.decode_and_handle_predict(test_json_string, "test", {}, "test_policy",
                                    30000, InputType::Strings)
          .get();

  Query parsed_query = response.query_;

  EXPECT_EQ(parsed_query.user_id_, 23);
  const std::string& parsed_input =
      std::static_pointer_cast<SerializableString>(parsed_query.input_)
          ->get_data();
  std::string expected_input(
      "hello world. This is a test string with punctionation!@#$Y#;}#");
  EXPECT_EQ(parsed_input, expected_input);
  EXPECT_EQ(parsed_query.label_, "test");
  EXPECT_EQ(parsed_query.latency_micros_, 30000);
  EXPECT_EQ(parsed_query.selection_policy_, "test_policy");
}

TEST_F(QueryFrontendTest, TestDecodeMalformedJSON) {
  std::string gibberish_string1 =
      "{\"uid\": 2hkdshfdshffhkj32kjhh{dskjfh32r\"3r32";

  std::string gibberish_string2 =
      "dshfdshffhkj32fsd32jk huf32h, 3 } 24j dskjfh32r\"3r32";

  ASSERT_THROW(
      rh_.decode_and_handle_predict(gibberish_string1, "test", {},
                                    "test_policy", 30000, InputType::Doubles),
      ptree_error);
  ASSERT_THROW(
      rh_.decode_and_handle_predict(gibberish_string2, "test", {},
                                    "test_policy", 30000, InputType::Strings),
      ptree_error);
}

TEST_F(QueryFrontendTest, TestDecodeMissingJsonField) {
  std::string json_missing_field = "{\"input\": [1.4,2.23,3.243242,0.3223424]}";
  ASSERT_THROW(
      rh_.decode_and_handle_predict(json_missing_field, "test", {},
                                    "test_policy", 30000, InputType::Doubles),
      ptree_error);
}

TEST_F(QueryFrontendTest, TestDecodeWrongInputType) {
  std::string test_json_doubles =
      "{\"uid\": 23, \"input\": [1.4,2.23,3.243242,0.3223424]}";
  ASSERT_THROW(
      rh_.decode_and_handle_predict(test_json_doubles, "test", {},
                                    "test_policy", 30000, InputType::Ints),
      ptree_bad_data);
}

TEST_F(QueryFrontendTest, TestDecodeCorrectUpdate) {
  std::string update_json =
      "{\"uid\": 23, \"input\": [1.4,2.23,3.243242,0.3223424], \"model_name\": "
      "\"m\", \"model_version\": 1, \"label\": 1.0}";
  FeedbackAck ack =
      rh_.decode_and_handle_update(update_json, "test", {}, "test_policy",
                                   InputType::Doubles, OutputType::Double)
          .get();

  EXPECT_TRUE(ack);
}

TEST_F(QueryFrontendTest, TestDecodeUpdateMissingField) {
  std::string update_json =
      "{\"uid\": 23, \"input\": [1.4,2.23,3.243242,0.3223424], \"model_name\": "
      "\"m\", \"label\": 1.0}";
  ASSERT_THROW(
      rh_.decode_and_handle_update(update_json, "test", {}, "test_policy",
                                   InputType::Doubles, OutputType::Double),
      ptree_error);
}

TEST_F(QueryFrontendTest, TestAddOneApplication) {
  size_t no_apps = rh_.num_applications();
  EXPECT_EQ(no_apps, (size_t)0);
  rh_.add_application("test_app_1", {}, InputType::Doubles, OutputType::Double,
                      "test_policy", 30000);
  size_t one_app = rh_.num_applications();
  EXPECT_EQ(one_app, (size_t)1);
}

TEST_F(QueryFrontendTest, TestAddManyApplications) {
  size_t no_apps = rh_.num_applications();
  EXPECT_EQ(no_apps, (size_t)0);

  for (int i = 0; i < 500; ++i) {
    std::string cur_name = "test_app_" + std::to_string(i);
    rh_.add_application(cur_name, {}, InputType::Doubles, OutputType::Double,
                        "test_policy", 30000);
  }

  size_t apps = rh_.num_applications();
  EXPECT_EQ(apps, (size_t)500);
}

}  // namespace
