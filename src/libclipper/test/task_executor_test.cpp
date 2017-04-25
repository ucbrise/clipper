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
  VersionedModelId model_id = std::make_pair<std::string, int>("test", 1);
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

TEST(TaskExecutorTests, ModelQueueOrdersElementsOnEarliestDeadline) {
  PredictTask task_a = create_predict_task(1, 10000);
  PredictTask task_b = create_predict_task(2, 10000);
  PredictTask task_c = create_predict_task(3, 10000);

  ModelQueue model_queue;

  model_queue.add_task(task_a);
  model_queue.add_task(task_b);
  model_queue.add_task(task_c);

  PredictTask first_task = model_queue.get_batch(1)[0];
  PredictTask second_task = model_queue.get_batch(1)[0];
  PredictTask third_task = model_queue.get_batch(1)[0];

  // Because we added tasks a through c in alphabetical
  // order with the same latency slos, we expect the model
  // queue's get_batch() function to return them in the same order
  ASSERT_EQ(first_task.query_id_, task_a.query_id_);
  ASSERT_EQ(second_task.query_id_, task_b.query_id_);
  ASSERT_EQ(third_task.query_id_, task_c.query_id_);

  std::vector<PredictTask> curr_batch = model_queue.get_batch(1);
  ASSERT_TRUE(curr_batch.empty());

  model_queue.add_task(task_c);
  model_queue.add_task(task_b);
  model_queue.add_task(task_a);

  first_task = model_queue.get_batch(1)[0];
  second_task = model_queue.get_batch(1)[0];
  third_task = model_queue.get_batch(1)[0];

  // Because we added tasks a through c in reverse alphabetical
  // order with the same latency slos, we expect the model
  // queue's get_batch() function to return them in reverse alphabetical order
  ASSERT_EQ(first_task.query_id_, task_c.query_id_);
  ASSERT_EQ(second_task.query_id_, task_b.query_id_);
  ASSERT_EQ(third_task.query_id_, task_a.query_id_);
}

TEST(TaskExecutorTests, ModelQueueGetBatchRemovesTasksWithElapsedDeadlines) {
  PredictTask task_a = create_predict_task(1, 0);
  PredictTask task_b = create_predict_task(2, 0);
  PredictTask task_c = create_predict_task(3, 100000);

  ModelQueue model_queue;

  model_queue.add_task(task_a);
  model_queue.add_task(task_b);
  model_queue.add_task(task_c);

  std::vector<PredictTask> tasks = model_queue.get_batch(3);
  // Tasks A and B have elapsed deadlines, so the model queue
  // should return a batch containing only Task C
  ASSERT_EQ(tasks.size(), 1);
  ASSERT_EQ(tasks[0].query_id_, task_c.query_id_);
}

TEST(TaskExecutorTests,
     ModelQueueGetEarliestDeadlineRemovesTasksWithElapsedDeadlines) {
  PredictTask task_a = create_predict_task(1, 0);
  PredictTask task_b = create_predict_task(2, 0);
  PredictTask task_c = create_predict_task(3, 100000);

  ModelQueue model_queue;

  model_queue.add_task(task_a);
  model_queue.add_task(task_b);

  // Tasks A and B have elapsed deadlines, so the model queue
  // should indicate that there is no earliest deadline
  boost::optional<Deadline> earliest_deadline =
      model_queue.get_earliest_deadline();
  ASSERT_FALSE(earliest_deadline);

  model_queue.add_task(task_c);
  earliest_deadline = model_queue.get_earliest_deadline();
  // Task C's deadline has not elapsed, so the model queue
  // should return an earliest deadline
  ASSERT_TRUE(earliest_deadline);
}
}
