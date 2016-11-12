#include <memory>
#include <random>
// uncomment to disable assert()
// #define NDEBUG
#include <cassert>

#include <clipper/task_executor.hpp>
#include <clipper/util.hpp>

#define BOOST_THREAD_VERSION 3
#include <boost/thread.hpp>

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

CacheEntry::CacheEntry() { value_ = value_promise_.get_future(); }

boost::shared_future<Output> PredictionCache::fetch(
    const VersionedModelId &model, const Input &input) {
  std::unique_lock<std::mutex> l(m_);
  auto key = hash(input);
  auto entry = cache_[key];
  return shared_future<Output>(entry.value_);
}

void PredictionCache::put(const VersionedModelId &model, const Input &input,
                          const Output &output) {
  std::unique_lock<std::mutex> l(m_);
  auto key = hash(input);
  auto entry = cache_[key];
  if (!entry.completed_) {
    entry.value_promise_.set_value(output);
    entry.completed_ = true;
    cache_[key] = entry;
  }
}

// std::vector<boost::future<Output>>
// BatchingTaskExecutor::schedule_predictions(
//     const std::vector<PredictTask>& tasks) {}
//
// std::vector<boost::future<FeedbackAck>>
// BatchingTaskExecutor::schedule_feedback(
//     const std::vector<FeedbackTask> tasks) {}

ModelContainer::ModelContainer(VersionedModelId id, std::string address) : model_(id), address_(address) {}


int ModelContainer::get_queue_size() const { return request_queue_.size(); }

ModelContainer &assign_container(
    const PredictTask &task, std::vector<ModelContainer> &containers) const {
  assert(containers.size() >= 1);
  if (containers.size() > 1) {
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<> dist(0, containers.size());
    int first_choice = dist(generator);
    int second_choice = first_choice;
    while (second_choice == first_choice) {
      second_choice = dist(generator);
    }
    if (containers[first_choice].get_queue_size() >
        containers[second_choice].get_queue_size()) {
      return containers[second_choice];
    } else {
      return containers[first_choice];
    }
  } else {
    return containers[0];
  }
}

}  // namespace clipper
