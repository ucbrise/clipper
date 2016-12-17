#include <memory>
#include <random>
// uncomment to disable assert()
// #define NDEBUG
#include <cassert>

#include <boost/thread.hpp>

#include <clipper/concurrency.hpp>
#include <clipper/task_executor.hpp>

namespace clipper {

CacheEntry::CacheEntry() {}

boost::future<Output> PredictionCache::fetch(
    const VersionedModelId& model, const std::shared_ptr<Input>& input) {
  std::unique_lock<std::mutex> l(m_);
  auto key = hash(model, input->hash());
  auto search = cache_.find(key);
  if (search != cache_.end()) {
    // If there is already a value in the cache, we can
    // complete the future immediately
    if (search->second.completed_) {
      return boost::make_ready_future<Output>(search->second.value_);
    } else {
      // If no value yet, create a promise and store it
      // in the cache entry, then return the associated future.
      search->second.value_promises_.emplace_back();
      return search->second.value_promises_.back().get_future();
    }
  } else {
    CacheEntry new_entry;
    new_entry.value_promises_.emplace_back();
    boost::future<Output> f = new_entry.value_promises_.back().get_future();
    cache_.insert(std::make_pair(key, std::move(new_entry)));
    return f;
  }
}

void PredictionCache::put(const VersionedModelId& model,
                          const std::shared_ptr<Input>& input,
                          const Output& output) {
  std::unique_lock<std::mutex> l(m_);
  auto key = hash(model, input->hash());
  auto search = cache_.find(key);
  if (search != cache_.end()) {
    if (!search->second.completed_) {
      // complete all the stored promises
      for (auto p = search->second.value_promises_.begin();
           p != search->second.value_promises_.end(); ++p) {
        p->set_value(output);
      }
      // search->second.value_promise_.set_value(output);
      search->second.value_ = output;
      search->second.completed_ = true;
    }
  } else {
    CacheEntry new_entry;
    new_entry.value_ = output;
    new_entry.completed_ = true;
    cache_.insert(std::make_pair(key, std::move(new_entry)));
  }
}

size_t PredictionCache::hash(const VersionedModelId& model,
                             size_t input_hash) const {
  return versioned_model_hash(model) ^ input_hash;
}

// std::shared_ptr<ModelContainer> PowerTwoChoicesScheduler::assign_container(
//     const PredictTask& task,
//     std::vector<std::shared_ptr<ModelContainer>>& containers) const {
//   UNUSED(task);
//   assert(containers.size() >= 1);
//   if (containers.size() > 1) {
//     std::random_device rd;
//     std::mt19937 generator(rd());
//     std::uniform_int_distribution<> dist(0, containers.size());
//     int first_choice = dist(generator);
//     int second_choice = first_choice;
//     while (second_choice == first_choice) {
//       second_choice = dist(generator);
//     }
//     if (containers[first_choice]->get_queue_size() >
//         containers[second_choice]->get_queue_size()) {
//       return containers[second_choice];
//     } else {
//       return containers[first_choice];
//     }
//   } else {
//     return containers[0];
//   }
// }

std::vector<float> deserialize_outputs(std::vector<uint8_t> bytes) {
  assert(bytes.size() % sizeof(float) == 0);
  //  uint8_t *bytes_ptr = bytes.data();  // point to beginning of memory
  float* float_array = reinterpret_cast<float*>(bytes.data());
  std::vector<float> outputs(float_array,
                             float_array + bytes.size() / sizeof(float));
  return outputs;
}

}  // namespace clipper
