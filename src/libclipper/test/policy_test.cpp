#include <gtest/gtest.h>
#include <chrono>
#include <thread>

#include <boost/property_tree/json_parser.hpp>
#include <clipper/selection_policies.hpp>
#include <clipper/datatypes.hpp>

using namespace clipper;

namespace {

class Utility {
  public:
  
  // Helper Function
  template <typename T>
  static std::vector<T> as_vector(boost::property_tree::ptree const& pt, boost::property_tree::ptree::key_type const& key) {
    std::vector<T> r;
    for (auto& item : pt.get_child(key)) r.push_back(item.second.get_value<T>());
    return r;
  }
  
  // Input
  static std::shared_ptr<Input> create_input() {
    boost::property_tree::ptree pt;
    std::vector<double> inputs = Utility::as_vector<double>(pt, "input");
    std::shared_ptr<Input> input = std::make_shared<DoubleVector>(inputs);
    return input;
  }
  
  // Feedback
  static Feedback create_feedback(double y) {
    auto input = create_input();
    return Feedback(input, y);
  }
  
  // Predictions
  static std::vector<Output> create_predictions(VersionedModelId model, double y_hat) {
    std::vector<Output> predictions = {Output(y_hat, {model})};
    return predictions;
  }
  
  // Query
  static Query create_query(std::vector<VersionedModelId> models) {
    auto input = create_input();
    Query query("label", 1000, input, 1000, "EXP3", models);
    return query;
  }
};
  
class PolicyTests : public ::testing::Test {
  public:
    virtual void SetUp() {
      models.emplace_back(model_1); // good
      models.emplace_back(model_2); // so-so
      models.emplace_back(model_3); // bad
      exp3state = Exp3Policy::initialize(models);
    }
    std::vector<VersionedModelId> models;
    VersionedModelId model_1 = std::make_pair("classification", 0);
    VersionedModelId model_2 = std::make_pair("regression", 1);
    VersionedModelId model_3 = std::make_pair("random_forest", 2);
    Exp3State exp3state;
};
  

// Exp3
TEST_F(PolicyTests, Exp3Test) {
  // Test initiate
  ASSERT_EQ(3, exp3state.first);
  // Update many times
  auto feedback = Utility::create_feedback(20);
  std::vector<Output> predictions;
  auto times = 100;
  while (times > 0) {
    auto y_hat = rand()%100;
    if (y_hat < 33) {
      predictions = Utility::create_predictions(model_1, y_hat);
    } else if (y_hat > 66) {
      predictions = Utility::create_predictions(model_2, y_hat);
    } else {
      predictions = Utility::create_predictions(model_3, y_hat);
    }
    exp3state = Exp3Policy::process_feedback(exp3state, feedback, predictions);
    times -= 1;
  }
  
  // Test weights are different
  ASSERT_GT(exp3state.second[model_1]["weight"], exp3state.second[model_2]["weight"]);
  ASSERT_GT(exp3state.second[model_2]["weight"], exp3state.second[model_3]["weight"]);
  
  // Select
  auto query = Utility::create_query(models);
  auto tasks = Exp3Policy::select_predict_tasks(exp3state, query, 1000);
  ASSERT_NE(model_1.second, tasks.front().model_.second);
}

} // namespace
