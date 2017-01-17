#include <memory>
#include <random>
// uncomment to disable assert()
// #define NDEBUG
#include <cassert>

#include <clipper/task_executor.hpp>
#include <clipper/metrics.hpp>
#include <clipper/util.hpp>

#include <boost/thread.hpp>

namespace clipper {

CacheEntry::CacheEntry() { value_ = value_promise_.get_future(); }

PredictionCache::PredictionCache() {
  lookups_counter_ = metrics::MetricsRegistry::get_metrics().create_counter("prediction_cache_lookups");
  hit_ratio_ = metrics::MetricsRegistry::get_metrics().create_ratio_counter("prediction_cache_hit_ratio");
}

boost::shared_future<Output> PredictionCache::fetch(
    const VersionedModelId &model, const std::shared_ptr<Input> &input) {
  std::unique_lock<std::mutex> l(m_);
  auto key = hash(model, input->hash());
  auto search = cache_.find(key);
  lookups_counter_->increment(1);
  if (search != cache_.end()) {
    hit_ratio_->increment(1,1);
    return search->second.value_;
  } else {
    CacheEntry new_entry;
    auto f = new_entry.value_;
    cache_.insert(std::make_pair(key, std::move(new_entry)));
    hit_ratio_->increment(0,1);
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
  std::cout << "HASH: " << input_hash << std::endl;
  return versioned_model_hash(model) ^ input_hash;
}

std::shared_ptr<ModelContainer> PowerTwoChoicesScheduler::assign_container(
    const PredictTask &task,
    std::vector<std::shared_ptr<ModelContainer>> &containers) const {
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

std::vector<float> deserialize_outputs(std::vector<uint8_t> bytes) {
  assert(bytes.size() % sizeof(float) == 0);
//  uint8_t *bytes_ptr = bytes.data();  // point to beginning of memory
  float *float_array = reinterpret_cast<float *>(bytes.data());
  std::vector<float> outputs(float_array,
                             float_array + bytes.size() / sizeof(float));
  return outputs;
}

}  // namespace clipper
