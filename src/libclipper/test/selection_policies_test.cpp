#include <gtest/gtest.h>
#include <chrono>
#include <ctime>
#include <thread>
#include <time.h>

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_real_distribution.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/selection_policies.hpp>

/* UNIT TESTS LOGISTICS
    1. Used 3 binary classifiers (good=0, so-so=1, bad=2)
    2. Set feedback as 1
    3. Good classifier has 75% chance returning 1, so-so has 50%, bad has 25%
*/
using namespace clipper;

namespace {

// Helper Functions
class Utility {

 public:
  // Input
  static std::shared_ptr<Input> create_input() {
    boost::random::mt19937 gen(std::time(0));
    int input_len = 100;
    vector<double> input;
    boost::random::uniform_real_distribution<> dist(0.0, 1.0);
    for (int i = 0; i < input_len; ++i) {
      input.push_back(dist(gen));
    }
    return std::make_shared<DoubleVector>(input);
  }

  // Feedback
  static Feedback create_feedback(double y) {
    auto input = create_input();
    return Feedback(input, y);
  }

  // Predictions
  static std::vector<Output> create_predictions(VersionedModelId model,
                                                double y_hat) {
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

// ********
// * EXP3 *
// ********
class PolicyTests : public ::testing::Test {
 public:
  virtual void SetUp() {
    models.emplace_back(model_0);  // good
    models.emplace_back(model_1);  // so-so
    models.emplace_back(model_2);  // bad
  }
  std::vector<VersionedModelId> models;
  VersionedModelId model_0 = std::make_pair("classifier0", 0);
  VersionedModelId model_1 = std::make_pair("classifier1", 1);
  VersionedModelId model_2 = std::make_pair("classifier2", 2);
  int update_times_ = 200;
  int select_times_ = 100;
};

TEST_F(PolicyTests, Exp3Test) {

  /* Update Test */
  BanditPolicyState state = Exp3Policy::initialize(models);
  auto feedback = Utility::create_feedback(1);
  std::vector<Output> predictions;
  int y_hat;
  int rand_index;
  int rand_draw;
  srand (time(NULL));
  
  for (int i=0; i<update_times_; ++i) {
    y_hat = 0;
    rand_index = rand() % models.size(); // Randomly pick model
    rand_draw = rand() % 100; // Randomly pick number to determine whether this model return 0 or 1
    if (rand_index == 0) { // good model
      if (rand_draw < 75) {
        y_hat = 1;
      }
    } else if (rand_index == 1) { // so-so model
      if (rand_draw < 50) {
        y_hat = 1;
      }
    } else {
      if (rand_draw < 25) { // bad model
        y_hat = 1;
      }
    }
    predictions = Utility::create_predictions(models[rand_index], y_hat);
    state = Exp3Policy::process_feedback(state, feedback, predictions);
  }
  // Test if model_0 weight > model_1 weight > model_2 weight
  ASSERT_GT(state.model_map_[model_0]["weight"],
            state.model_map_[model_1]["weight"]);
  ASSERT_GT(state.model_map_[model_1]["weight"],
            state.model_map_[model_2]["weight"]);

  /* Selection Test */
  auto query = Utility::create_query(models);
  int select_0 = 0;
  int select_1 = 0;
  int select_2 = 0;
  for (int i=0; i<select_times_; ++i) {
    auto tasks = Exp3Policy::select_predict_tasks(state, query, 1000);
    if (model_0.second == tasks.front().model_.second) {
      select_0 ++;
    } else if (model_1.second == tasks.front().model_.second) {
      select_1 ++;
    } else {
      select_2 ++;
    };
  }
  // Test if times selected model_0 > model_1 > model_2
  ASSERT_GT(select_0, select_1);
  ASSERT_GT(select_1, select_2);

  /* Serialization Test */
  auto bytes = Exp3Policy::serialize_state(state);
  auto new_state = Exp3Policy::deserialize_state(bytes);
  ASSERT_EQ(state.weight_sum_, new_state.weight_sum_);
}

// ********
// * EXP4 *
// ********

TEST_F(PolicyTests, Exp4Test) {
  
  /* Update Test */
  BanditPolicyState state = Exp4Policy::initialize(models);
  auto feedback = Utility::create_feedback(1);
  std::vector<Output> predictions;
  int y_hat;
  int rand_index;
  int rand_draw;
  srand (time(NULL));
  
  for (int i=0; i<update_times_; ++i) {
    y_hat = 0;
    rand_index = rand() % models.size(); // Randomly pick model
    rand_draw = rand() % 100; // Randomly pick number to determine whether this model return 0 or 1
    
    if (rand_index == 0) { // good model
      if (rand_draw < 75) {
        y_hat = 1;
      }
    } else if (rand_index == 1) { // so-so model
      if (rand_draw < 50) {
        y_hat = 1;
      }
    } else {
      if (rand_draw < 25) { // bad model
        y_hat = 1;
      }
    }
    predictions = Utility::create_predictions(models[rand_index], y_hat);
    state = Exp4Policy::process_feedback(state, feedback, predictions);
  }
  // Test if model_0 weight > model_1 weight > model_2 weight
  ASSERT_GT(state.model_map_[model_0]["weight"],
            state.model_map_[model_1]["weight"]);
  ASSERT_GT(state.model_map_[model_1]["weight"],
            state.model_map_[model_2]["weight"]);

  /* Serialization Test */
  auto bytes = Exp4Policy::serialize_state(state);
  auto new_state = Exp4Policy::deserialize_state(bytes);
  ASSERT_EQ(state.weight_sum_, new_state.weight_sum_);
}

// *****************
// * EpsilonGreedy *
// *****************

TEST_F(PolicyTests, EpsilonGreedyTest) {
  
  /* Update Test */
  BanditPolicyState state = EpsilonGreedyPolicy::initialize(models);
  auto feedback = Utility::create_feedback(1);
  std::vector<Output> predictions;
  int y_hat;
  int rand_index;
  int rand_draw;
  srand (time(NULL));
  
  for (int i=0; i<update_times_; ++i) {
    y_hat = 0;
    rand_index = rand() % models.size(); // Randomly pick model
    rand_draw = rand() % 100; // Randomly pick number to determine whether this model return 0 or 1
    
    if (rand_index == 0) { // good model
      if (rand_draw < 75) {
        y_hat = 1;
      }
    } else if (rand_index == 1) { // so-so model
      if (rand_draw < 50) {
        y_hat = 1;
      }
    } else {
      if (rand_draw < 25) { // bad model
        y_hat = 1;
      }
    }
    predictions = Utility::create_predictions(models[rand_index], y_hat);
    state = EpsilonGreedyPolicy::process_feedback(state, feedback, predictions);
  }
  // Test if the expected loss of model_2 > model_1 > model_0
  ASSERT_GT(state.model_map_[model_2]["expected_loss"],
            state.model_map_[model_1]["expected_loss"]);
  ASSERT_GT(state.model_map_[model_1]["expected_loss"],
            state.model_map_[model_0]["expected_loss"]);

  /* Selection Test */
  auto query = Utility::create_query(models);
  int select_0 = 0;
  int select_1 = 0;
  int select_2 = 0;
  for (int i=0; i<select_times_; ++i) {
    auto tasks = EpsilonGreedyPolicy::select_predict_tasks(state, query, 1000);
    if (model_0.second == tasks.front().model_.second) {
      select_0 ++;
    } else if (model_1.second == tasks.front().model_.second) {
      select_1 ++;
    } else {
      select_2 ++;
    };
  }
  // Test if times selected model_0 > model_1
  ASSERT_GT(select_0, select_1);

  /* Serialization Test */
  auto bytes = EpsilonGreedyPolicy::serialize_state(state);
  auto new_state = EpsilonGreedyPolicy::deserialize_state(bytes);
  ASSERT_EQ(state.weight_sum_, new_state.weight_sum_);
}

// ********
// * UCB *
// ********
TEST_F(PolicyTests, UCBTest) {
  
  /* Update Test */
  BanditPolicyState state = UCBPolicy::initialize(models);
  auto feedback = Utility::create_feedback(1);
  std::vector<Output> predictions;
  int y_hat;
  int rand_index;
  int rand_draw;
  srand (time(NULL));
  
  for (int i=0; i<update_times_; ++i) {
    y_hat = 0;
    rand_index = rand() % models.size(); // Randomly pick model
    rand_draw = rand() % 100; // Randomly pick number to determine whether this model return 0 or 1
    
    if (rand_index == 0) { // good model
      if (rand_draw < 75) {
        y_hat = 1;
      }
    } else if (rand_index == 1) { // so-so model
      if (rand_draw < 50) {
        y_hat = 1;
      }
    } else {
      if (rand_draw < 25) { // bad model
        y_hat = 1;
      }
    }
    predictions = Utility::create_predictions(models[rand_index], y_hat);
    state = UCBPolicy::process_feedback(state, feedback, predictions);
  }
  // Test if the expected loss of model_2 > model_1 > model_0
  ASSERT_GT(state.model_map_[model_2]["expected_loss"],
            state.model_map_[model_1]["expected_loss"]);
  ASSERT_GT(state.model_map_[model_1]["expected_loss"],
            state.model_map_[model_0]["expected_loss"]);

  /* Selection Test */
  auto query = Utility::create_query(models);
  // UCB should always select the optimal bandit
  for (int i=0; i<select_times_; ++i) {
    auto tasks = UCBPolicy::select_predict_tasks(state, query, 1000);
    ASSERT_EQ(model_0.second, tasks.front().model_.second);
  }

  /* Serialization Test */
  auto bytes = UCBPolicy::serialize_state(state);
  auto new_state = UCBPolicy::deserialize_state(bytes);
  ASSERT_EQ(state.weight_sum_, new_state.weight_sum_);
}

}  // namespace

