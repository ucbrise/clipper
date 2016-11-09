#include <memory>
#include <random>

#include <clipper/task_executor.hpp>

namespace clipper {

PredictTask::PredictTask(std::shared_ptr<Input> input, VersionedModelId model,
                         float utility, QueryId query_id,
                         long latency_slo_micros)
    : input_(std::move(input)),
      model_(model),
      utility_(utility),
      query_id_(query_id),
      latency_slo_micros_(latency_slo_micros) {}

FeedbackTask::FeedbackTask(Feedback feedback, VersionedModelId model,
                           QueryId query_id, long latency_slo_micros)
    : feedback_(feedback),
      model_(model),
      query_id_(query_id),
      latency_slo_micros_(latency_slo_micros) {}

std::vector<boost::future<Output>> BatchingTaskExecutor::schedule_predictions(
    const std::vector<PredictTask>& tasks) {}

std::vector<boost::future<FeedbackAck>> BatchingTaskExecutor::schedule_feedback(
    const std::vector<FeedbackTask> tasks) {}

int ModelContainer::get_queue_size() const { return request_queue_.size(); }

ModelContainer& assign_container(
    const PredictTask& task, std::vector<ModelContainer>& containers) const {
  if (containers.size() > 1) {
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<> dist(0, containers.size());
    int first_choice = dist(generator);
    int second_choice = first_choice;
    while (second_choice == first_choice) {
      second_choice = dist(generator);
    }
    if (containers[first_choice].size() > containers[second_choice].size()) {
      return &containers[second_choice];
    } else {
      return &containers[first_choice];
    }
  }
}

}  // namespace clipper
