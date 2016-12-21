#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <boost/thread.hpp>
#define BOOST_SPIRIT_THREADSAFE

#include <clipper/datatypes.hpp>
#include <clipper/query_processor.hpp>
#include <frontends/rest.hpp>

#include <client_http.hpp>

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
using HttpClient = SimpleWeb::Client<SimpleWeb::HTTP>;

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
  return arg.label_.compare(expected_query.label_) == 0;
}

MATCHER_P(FeedbackQueryEqual, expected_fq,"") {
  return arg.label_.compare(expected_fq.label_) == 0;
}

TEST_F(RestApiTests, BasicInfoTest) {
  std::thread server_thread([this](){
    //Start server
    rh_.start_listening();
  });
  // Wait for server to start
  std::this_thread::sleep_for(std::chrono::seconds(1));

  std::string app_name = "app";
  long uid = 1;
  std::vector<VersionedModelId> models = {std::make_pair("m", 1), std::make_pair("n", 2)};
  VersionedModelId model_to_update = std::make_pair("m", 1);
  InputType input_type = double_vec;
  OutputType output_type = double_val;
  std::string selection_policy = "most_recent";
  long latency_micros = 20000;
  std::shared_ptr<Input> input =
    std::make_shared<DoubleVector>(std::vector<double>{1.1, 2.2, 3.3, 4.4});
  Output output = Output(2.0, model_to_update);
  Feedback feedback = std::make_pair(input, output);

  rh_.add_application(app_name, models, input_type, output_type, selection_policy, latency_micros);
  HttpClient client("localhost:1337");
  // Send predict and update requests
  std::string predict_json = "{\"uid\": 1, \"input\": [1.1, 2.2, 3.3, 4.4]}";
  Query expected_query = Query(app_name, uid, input, latency_micros, selection_policy, models);
  EXPECT_CALL(qp_, predict(QueryEqual(expected_query)));
  client.request("POST", "/app/predict", predict_json);

  std::string update_json = "{\"uid\": 1, \"input\": [1.1, 2.2, 3.3, 4.4], \"label\": 2.0, \"model_name\": \"m\", \"model_version\": 1}";
  FeedbackQuery expected_fq = FeedbackQuery(app_name, uid, feedback, selection_policy, models);
  EXPECT_CALL(qp_, update(FeedbackQueryEqual(expected_fq)));
  client.request("POST", "/app/update", update_json);
}

} // namespace
