#include <gtest/gtest.h>
#include <chrono>
#include <ctime>
#include <thread>

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_real_distribution.hpp>

// #include <boost/property_tree/json_parser.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/selection_policies.hpp>

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
class Exp3Test : public ::testing::Test {
 public:
  virtual void SetUp() {
    models.emplace_back(model_1);  // good
    models.emplace_back(model_2);  // so-so
    models.emplace_back(model_3);  // bad
    state = Exp3Policy::initialize(models);
  }
  std::vector<VersionedModelId> models;
  VersionedModelId model_1 = std::make_pair("classification", 0);
  VersionedModelId model_2 = std::make_pair("regression", 1);
  VersionedModelId model_3 = std::make_pair("random_forest", 2);
  int times = 1000;
  PolicyState state;
};

TEST_F(Exp3Test, UpdateTest) {

  auto feedback = Utility::create_feedback(20);
  std::vector<Output> predictions;
  while (times > 0) {
    auto y_hat = rand() % 100;
    if (y_hat < 33) {
      predictions = Utility::create_predictions(model_1, y_hat);
    } else if (y_hat > 66) {
      predictions = Utility::create_predictions(model_3, y_hat);
    } else {
      predictions = Utility::create_predictions(model_2, y_hat);
    }
    state = Exp3Policy::process_feedback(state, feedback, predictions);
    times -= 1;
  }
  ASSERT_GT(state.model_map_[model_1]["weight"],
            state.model_map_[model_2]["weight"]);
  ASSERT_GT(state.model_map_[model_2]["weight"],
            state.model_map_[model_3]["weight"]);
}

TEST_F(Exp3Test, SelectionTest) {
  auto query = Utility::create_query(models);
  auto tasks = Exp3Policy::select_predict_tasks(state, query, 1000);
  ASSERT_NE(model_3.second, tasks.front().model_.second);
}

TEST_F(Exp3Test, SerializationTest) {
  auto bytes = Exp3Policy::serialize_state(state);
  auto new_state = Exp3Policy::deserialize_state(bytes);
  ASSERT_EQ(state.weight_sum_, new_state.weight_sum_);
}

// ********
// * EXP4 *
// ********
class Exp4Test : public ::testing::Test {
 public:
  virtual void SetUp() {
    models.emplace_back(model_1);  // good
    models.emplace_back(model_2);  // so-so
    models.emplace_back(model_3);  // bad
    state = Exp4Policy::initialize(models);
  }
  std::vector<VersionedModelId> models;
  VersionedModelId model_1 = std::make_pair("classification", 0);
  VersionedModelId model_2 = std::make_pair("regression", 1);
  VersionedModelId model_3 = std::make_pair("random_forest", 2);
  int times = 1000;
  PolicyState state;
};

TEST_F(Exp4Test, UpdateTest) {

  auto feedback = Utility::create_feedback(20);
  std::vector<Output> predictions;
  while (times > 0) {
    auto y_hat = rand() % 100;
    if (y_hat < 33) {
      predictions = Utility::create_predictions(model_1, y_hat);
    } else if (y_hat > 66) {
      predictions = Utility::create_predictions(model_3, y_hat);
    } else {
      predictions = Utility::create_predictions(model_2, y_hat);
    }
    state = Exp4Policy::process_feedback(state, feedback, predictions);
    times -= 1;
  }
  ASSERT_GT(state.model_map_[model_1]["weight"],
            state.model_map_[model_2]["weight"]);
  ASSERT_GT(state.model_map_[model_2]["weight"],
            state.model_map_[model_3]["weight"]);
}

TEST_F(Exp4Test, SelectionTest) {
  auto query = Utility::create_query(models);
  auto tasks = Exp4Policy::select_predict_tasks(state, query, 1000);
  ASSERT_NE(model_3.second, tasks.front().model_.second);
}

TEST_F(Exp4Test, SerializationTest) {
  auto bytes = Exp4Policy::serialize_state(state);
  auto new_state = Exp4Policy::deserialize_state(bytes);
  ASSERT_EQ(state.weight_sum_, new_state.weight_sum_);
}

// *****************
// * EpsilonGreedy *
// *****************
class EpsilonGreedyTest : public ::testing::Test {
 public:
  virtual void SetUp() {
    models.emplace_back(model_1);  // good
    models.emplace_back(model_2);  // so-so
    models.emplace_back(model_3);  // bad
    state = EpsilonGreedyPolicy::initialize(models);
  }
  std::vector<VersionedModelId> models;
  VersionedModelId model_1 = std::make_pair("classification", 0);
  VersionedModelId model_2 = std::make_pair("regression", 1);
  VersionedModelId model_3 = std::make_pair("random_forest", 2);
  int times = 1000;
  PolicyState state;
};

TEST_F(EpsilonGreedyTest, UpdateTest) {

  auto feedback = Utility::create_feedback(20);
  std::vector<Output> predictions;
  while (times > 0) {
    auto y_hat = rand() % 100;
    if (y_hat < 33) {
      predictions = Utility::create_predictions(model_1, y_hat);
    } else if (y_hat > 66) {
      predictions = Utility::create_predictions(model_3, y_hat);
    } else {
      predictions = Utility::create_predictions(model_2, y_hat);
    }
    state = EpsilonGreedyPolicy::process_feedback(state, feedback, predictions);
    times -= 1;
  }
  ASSERT_GT(state.model_map_[model_3]["expected_loss"],
            state.model_map_[model_2]["expected_loss"]);
  ASSERT_GT(state.model_map_[model_2]["expected_loss"],
            state.model_map_[model_1]["expected_loss"]);
}

TEST_F(EpsilonGreedyTest, SerializationTest) {
  auto bytes = EpsilonGreedyPolicy::serialize_state(state);
  auto new_state = EpsilonGreedyPolicy::deserialize_state(bytes);
  ASSERT_EQ(state.weight_sum_, new_state.weight_sum_);
}

// ********
// * UCB *
// ********
class UCBTest : public ::testing::Test {
 public:
  virtual void SetUp() {
    models.emplace_back(model_1);  // good
    models.emplace_back(model_2);  // so-so
    models.emplace_back(model_3);  // bad
    state = UCBPolicy::initialize(models);
  }
  std::vector<VersionedModelId> models;
  VersionedModelId model_1 = std::make_pair("classification", 0);
  VersionedModelId model_2 = std::make_pair("regression", 1);
  VersionedModelId model_3 = std::make_pair("random_forest", 2);
  int times = 1000;
  PolicyState state;
};

TEST_F(UCBTest, UpdateTest) {

  auto feedback = Utility::create_feedback(20);
  std::vector<Output> predictions;
  while (times > 0) {
    auto y_hat = rand() % 100;
    if (y_hat < 33) {
      predictions = Utility::create_predictions(model_1, y_hat);
    } else if (y_hat > 66) {
      predictions = Utility::create_predictions(model_3, y_hat);
    } else {
      predictions = Utility::create_predictions(model_2, y_hat);
    }
    state = UCBPolicy::process_feedback(state, feedback, predictions);
    times -= 1;
  }
  ASSERT_GT(state.model_map_[model_3]["expected_loss"],
            state.model_map_[model_2]["expected_loss"]);
  ASSERT_GT(state.model_map_[model_2]["expected_loss"],
            state.model_map_[model_1]["expected_loss"]);
}

TEST_F(UCBTest, SelectionTest) {
  auto query = Utility::create_query(models);
  auto tasks = UCBPolicy::select_predict_tasks(state, query, 1000);
  ASSERT_EQ(model_1.second, tasks.front().model_.second);
}

TEST_F(UCBTest, SerializationTest) {
  auto bytes = UCBPolicy::serialize_state(state);
  auto new_state = UCBPolicy::deserialize_state(bytes);
  ASSERT_EQ(state.weight_sum_, new_state.weight_sum_);
}

}  // namespace

