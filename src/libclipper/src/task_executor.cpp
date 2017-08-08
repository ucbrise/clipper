#include <memory>
#include <random>
// uncomment to disable assert()
// #define NDEBUG
#include <cassert>

#include <clipper/metrics.hpp>
#include <clipper/task_executor.hpp>
#include <clipper/util.hpp>

#include <boost/thread.hpp>

namespace clipper {

CacheEntry::CacheEntry() {}

PredictionCache::PredictionCache() {
  lookups_counter_ = metrics::MetricsRegistry::get_metrics().create_counter(
      "internal:prediction_cache_lookups");
  hit_ratio_ = metrics::MetricsRegistry::get_metrics().create_ratio_counter(
      "internal:prediction_cache_hit_ratio");
}

folly::Future<Output> PredictionCache::fetch(
    const VersionedModelId &model, const std::shared_ptr<Input> &input) {
  std::unique_lock<std::mutex> l(m_);
  auto key = hash(model, input->hash());
  auto search = cache_.find(key);
  lookups_counter_->increment(1);
  if (search != cache_.end()) {
    // cache entry exists
    if (search->second.completed_) {
      // value already in cache
      hit_ratio_->increment(1, 1);
      return folly::makeFuture<Output>(std::move(search->second.value_));
    } else {
      // value not in cache yet
      folly::Promise<Output> new_promise;
      folly::Future<Output> new_future = new_promise.getFuture();
      search->second.value_promises_.push_back(std::move(new_promise));
      hit_ratio_->increment(0, 1);
      return new_future;
    }
  } else {
    // cache entry doesn't exist yet, so create entry
    CacheEntry new_entry;
    // create promise/future pair for this request
    folly::Promise<Output> new_promise;
    folly::Future<Output> new_future = new_promise.getFuture();
    new_entry.value_promises_.push_back(std::move(new_promise));
    cache_.insert(std::make_pair(key, std::move(new_entry)));
    hit_ratio_->increment(0, 1);
    return new_future;
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
      // Complete the outstanding promises
      for (auto &p : search->second.value_promises_) {
        p.setValue(std::move(output));
      }
      search->second.completed_ = true;
      search->second.value_ = output;
    }
  } else {
    CacheEntry new_entry;
    new_entry.value_ = output;
    new_entry.completed_ = true;
    cache_.insert(std::make_pair(key, std::move(new_entry)));
  }
}

size_t PredictionCache::hash(const VersionedModelId &model,
                             size_t input_hash) const {
  std::size_t seed = 0;
  size_t model_hash = std::hash<clipper::VersionedModelId>()(model);
  boost::hash_combine(seed, model_hash);
  boost::hash_combine(seed, input_hash);
  return seed;
}

}  // namespace clipper
