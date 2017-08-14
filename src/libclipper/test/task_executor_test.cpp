#include <gtest/gtest.h>
#include <chrono>

#include <boost/optional.hpp>

#include <clipper/containers.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/task_executor.hpp>

using namespace clipper;

namespace {

/**
 * Creates a predict task with the specified query_id.
 * Specification of this id is used to disambiguate
 * predict tasks for testing (i.e. task queuing order stability)
 */
PredictTask create_predict_task(long query_id, long latency_slo_millis) {
  std::vector<double> data;
  VersionedModelId model_id = VersionedModelId("test", "1");
  data.push_back(1.0);
  std::shared_ptr<Input> input = std::make_shared<DoubleVector>(data);
  PredictTask task(input, model_id, 1.0, query_id, latency_slo_millis);
  return task;
}

TEST(TaskExecutorTests, TestDeadlineComparisonsWorkCorrectly) {
  Deadline current_time = std::chrono::system_clock::now();
  Deadline earlier = current_time - std::chrono::hours(1);
  Deadline later = current_time + std::chrono::hours(1);

  PredictTask task = create_predict_task(1, 10000);
  std::pair<Deadline, PredictTask> deadline_task_pair_current =
      std::make_pair(current_time, task);
  std::pair<Deadline, PredictTask> deadline_task_pair_earlier =
      std::make_pair(earlier, task);
  std::pair<Deadline, PredictTask> deadline_task_pair_later =
      std::make_pair(later, task);

  DeadlineCompare deadline_compare;
  bool current_greater_than_earlier =
      deadline_compare(deadline_task_pair_current, deadline_task_pair_earlier);
  bool earlier_greater_than_current =
      deadline_compare(deadline_task_pair_earlier, deadline_task_pair_current);
  ASSERT_TRUE(current_greater_than_earlier);
  ASSERT_FALSE(earlier_greater_than_current);

  bool current_greater_than_later =
      deadline_compare(deadline_task_pair_current, deadline_task_pair_later);
  bool later_greater_than_current =
      deadline_compare(deadline_task_pair_later, deadline_task_pair_current);
  ASSERT_FALSE(current_greater_than_later);
  ASSERT_TRUE(later_greater_than_current);

  bool earlier_greater_than_later =
      deadline_compare(deadline_task_pair_earlier, deadline_task_pair_later);
  bool later_greater_than_earlier =
      deadline_compare(deadline_task_pair_later, deadline_task_pair_earlier);
  ASSERT_FALSE(earlier_greater_than_later);
  ASSERT_TRUE(later_greater_than_earlier);
}

TEST(ModelQueueTests, TestGetBatchQueueNotEmpty) {
  PredictTask task_a = create_predict_task(1, 10000);
  PredictTask task_b = create_predict_task(2, 10000);
  PredictTask task_c = create_predict_task(3, 10000);

  ModelQueue model_queue;

  model_queue.add_task(task_a);
  model_queue.add_task(task_b);
  model_queue.add_task(task_c);

  std::vector<PredictTask> tasks =
      model_queue.get_batch([](Deadline) { return 3; });

  // Because we added tasks a through c in alphabetical
  // order with the same latency slos, we expect the model
  // queue's get_batch() function to return them in the same order
  ASSERT_EQ(tasks[0].query_id_, task_a.query_id_);
  ASSERT_EQ(tasks[1].query_id_, task_b.query_id_);
  ASSERT_EQ(tasks[2].query_id_, task_c.query_id_);
}

TEST(ModelQueueTests, TestGetBatchOrdersByEarliestDeadline) {
  PredictTask task_a = create_predict_task(1, 20000);
  PredictTask task_b = create_predict_task(2, 10000);
  PredictTask task_c = create_predict_task(3, 30000);

  ModelQueue model_queue;

  model_queue.add_task(task_a);
  model_queue.add_task(task_b);
  model_queue.add_task(task_c);

  std::vector<PredictTask> tasks =
      model_queue.get_batch([](Deadline) { return 3; });

  // Because we added tasks a through c in alphabetical
  // order with the same latency slos, we expect the model
  // queue's get_batch() function to return them in the same order
  ASSERT_EQ(tasks[0].query_id_, task_b.query_id_);
  ASSERT_EQ(tasks[1].query_id_, task_a.query_id_);
  ASSERT_EQ(tasks[2].query_id_, task_c.query_id_);
}

TEST(ModelQueueTests, TestGetBatchRemovesTasksWithElapsedDeadline) {
  PredictTask task_a = create_predict_task(1, 0);
  PredictTask task_b = create_predict_task(2, 0);
  PredictTask task_c = create_predict_task(3, 10000);

  ModelQueue model_queue;

  model_queue.add_task(task_a);
  model_queue.add_task(task_b);
  model_queue.add_task(task_c);

  std::vector<PredictTask> tasks =
      model_queue.get_batch([](Deadline) { return 3; });

  ASSERT_EQ(tasks.size(), (size_t)1);
  ASSERT_EQ(tasks[0].query_id_, task_c.query_id_);
}

TEST(PredictionCacheTests,
     TestIncompleteFuturesForEvictedEntryAreCompletedOnPut) {
  size_t cache_size_elements = 1;
  PredictionCache cache(cache_size_elements);
  VersionedModelId model_id("TEST", "1");
  std::shared_ptr<Input> first_input =
      std::make_shared<IntVector>(std::vector<int>{1, 2, 3});
  Output first_output("1", std::vector<VersionedModelId>{});

  auto first_output_future = cache.fetch(model_id, first_input);
  for (int i = 0; i < 10; ++i) {
    std::shared_ptr<Input> input =
        std::make_shared<IntVector>(std::vector<int>{i});
    Output output(std::to_string(i), std::vector<VersionedModelId>{});
    cache.put(model_id, input, output);
  }
  cache.put(model_id, first_input, first_output);
  ASSERT_TRUE(first_output_future.is_ready());
  ASSERT_EQ(first_output_future.get().y_hat_, first_output.y_hat_);
  // After the entry's associated futures were completed, the entry should
  // have been removed from the cache
  ASSERT_FALSE(cache.fetch(model_id, first_input).is_ready());
}

TEST(PredictionCacheTests,
     TestEvictionPolicyConsistentWithClockMultipleFetchesAndPuts) {
  size_t cache_size_elements = 4;
  PredictionCache cache(cache_size_elements);
  VersionedModelId model_id("TEST", "1");
  std::vector<std::shared_ptr<Input>> inputs;
  std::vector<Output> outputs;
  for (int i = 0; i < 6; ++i) {
    inputs.push_back(std::make_shared<IntVector>(std::vector<int>{i}));
    outputs.push_back(
        Output(std::to_string(i), std::vector<VersionedModelId>{}));
  }
  // Insert a completed entry into page slot 0, advance clock hand to slot 1
  cache.put(model_id, inputs[0], outputs[0]);
  // Insert a completed entry into page slot 1, advance clock hand to slot 2
  cache.put(model_id, inputs[1], outputs[1]);
  // Fetching elements that are already in the cache should not
  // change page state
  auto first_input_future = cache.fetch(model_id, inputs[0]);
  ASSERT_TRUE(first_input_future.is_ready());
  ASSERT_EQ(first_input_future.get().y_hat_, outputs[0].y_hat_);
  auto second_input_future = cache.fetch(model_id, inputs[1]);
  ASSERT_TRUE(second_input_future.is_ready());
  ASSERT_EQ(second_input_future.get().y_hat_, outputs[1].y_hat_);

  // Insert an incomplete entry into page slot 2, advance clock hand to slot 3
  auto third_input_future = cache.fetch(model_id, inputs[2]);
  // Insert an incomplete entry into page slot 3, advance clock hand to slot 0
  auto fourth_input_future = cache.fetch(model_id, inputs[3]);
  // Cycle through all slots until all used bits are 0 and replace the completed
  // entry in
  // page slot 0 with an incomplete entry, advance clock hand to slot 1
  auto fifth_input_future = cache.fetch(model_id, inputs[4]);

  // Fetching the entry in page slot 1 sets its used bit to 1
  ASSERT_TRUE(cache.fetch(model_id, inputs[1]).is_ready());
  // Set slot 1's used bit to 0, advance the clock hand to slot 2, and
  // replace the incomplete entry for inputs[2] in page slot 2 with an
  // incomplete entry
  // corresponding to inputs[0].
  ASSERT_FALSE(cache.fetch(model_id, inputs[0]).is_ready());

  // Complete the preexisting, incomplete cache entries
  // corresponding to the inputs at indices 0, 3, and 4.
  // This will not change page state
  cache.put(model_id, inputs[3], outputs[3]);
  cache.put(model_id, inputs[4], outputs[4]);
  cache.put(model_id, inputs[0], outputs[0]);

  std::vector<int> in_cache_indices{0, 1, 3, 4};
  std::vector<int> not_in_cache_indices{2, 5};

  for (int index : in_cache_indices) {
    ASSERT_TRUE(cache.fetch(model_id, inputs[index]).is_ready());
    ASSERT_EQ(cache.fetch(model_id, inputs[index]).get().y_hat_,
              outputs[index].y_hat_);
  }

  for (int index : not_in_cache_indices) {
    ASSERT_FALSE(cache.fetch(model_id, inputs[index]).is_ready());
  }
}
}
