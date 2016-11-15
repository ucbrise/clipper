#include <memory>
#include <random>
// uncomment to disable assert()
// #define NDEBUG
#include <cassert>

#include <clipper/task_executor.hpp>
#include <clipper/util.hpp>

#define BOOST_THREAD_VERSION 4
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
    const VersionedModelId &model, const std::shared_ptr<Input> &input) {
  std::unique_lock<std::mutex> l(m_);
  auto key = hash(model, input->hash());
  auto search = cache_.find(key);
  if (search != cache_.end()) {
    return search->second.value_;
  } else {
    CacheEntry new_entry;
    auto f = new_entry.value_;
    cache_.insert(std::make_pair(key, std::move(new_entry)));
    return f;
  }
}

void PredictionCache::put(const VersionedModelId &model,
                          const std::shared_ptr<Input> &input,
                          const Output &output) {
  std::unique_lock<std::mutex> l(m_);
  auto key = hash(model, input->hash());
  auto search = cache_.find(key);
  if (search != cache_.end()) {
    if (!search->second.completed_) {
      search->second.value_promise_.set_value(output);
      search->second.completed_ = true;
    }
  } else {
    CacheEntry new_entry;
    new_entry.value_promise_.set_value(output);
    new_entry.completed_ = true;
    cache_.insert(std::make_pair(key, std::move(new_entry)));
  }
}

size_t PredictionCache::hash(const VersionedModelId &model,
                             size_t input_hash) const {
  return versioned_model_hash(model) ^ input_hash;
}

// std::vector<boost::future<Output>>
// BatchingTaskExecutor::schedule_predictions(
//     const std::vector<PredictTask>& tasks) {}
//
// std::vector<boost::future<FeedbackAck>>
// BatchingTaskExecutor::schedule_feedback(
//     const std::vector<FeedbackTask> tasks) {}

ModelContainer::ModelContainer(VersionedModelId id, std::string address)
    : model_(id), address_(address) {}

int ModelContainer::get_queue_size() { return request_queue_.size(); }

void ModelContainer::send_prediction(PredictTask task) {
  request_queue_.push(task);
}

std::shared_ptr<ModelContainer> PowerTwoChoicesScheduler::assign_container(
    const PredictTask &task, std::vector<std::shared_ptr<ModelContainer>> &containers) const {
  UNUSED(task);
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
    if (containers[first_choice]->get_queue_size() >
        containers[second_choice]->get_queue_size()) {
      return containers[second_choice];
    } else {
      return containers[first_choice];
    }
  } else {
    return containers[0];
  }
}

}  // namespace clipper
