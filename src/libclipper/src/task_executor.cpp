#include <memory>
#include <random>
// uncomment to disable assert()
// #define NDEBUG
#include <cassert>

#include <clipper/metrics.hpp>
#include <clipper/task_executor.hpp>
#include <clipper/util.hpp>

namespace clipper {

CacheEntry::CacheEntry() {}

PredictionCache::PredictionCache(size_t size) : max_size_(size) {
  lookups_counter_ = metrics::MetricsRegistry::get_metrics().create_counter(
      "internal:prediction_cache_lookups");
  hit_ratio_ = metrics::MetricsRegistry::get_metrics().create_ratio_counter(
      "internal:prediction_cache_hit_ratio");
}

folly::Future<Output> PredictionCache::fetch(
    const VersionedModelId &model, const std::shared_ptr<Input> &input) {
  std::unique_lock<std::mutex> l(m_);
  auto key = hash(model, input->hash());
  auto search = entries_.find(key);
  lookups_counter_->increment(1);
  if (search != entries_.end()) {
    // cache entry exists
    if (search->second.completed_) {
      // value already in cache
      hit_ratio_->increment(1, 1);
      search->second.used_ = true;
      // `makeFuture` takes an rvalue reference, so moving/forwarding
      // the cache value directly would destroy it. Therefore, we use
      // copy assignment to `value` and move the copied object instead
      Output value = search->second.value_;
      return folly::makeFuture<Output>(std::move(value));
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
    insert_entry(key, new_entry);
    hit_ratio_->increment(0, 1);
    return new_future;
  }
}

void PredictionCache::put(const VersionedModelId &model,
                          const std::shared_ptr<Input> &input,
                          const Output &output) {
  std::unique_lock<std::mutex> l(m_);
  auto key = hash(model, input->hash());
  auto search = entries_.find(key);
  if (search != entries_.end()) {
    CacheEntry& entry = search->second;
    if (!entry.completed_) {
      // Complete the outstanding promises
      auto& promises = entry.value_promises_;
      while(promises.size() > 0) {
        promises.back().setValue(std::move(output));
        promises.pop_back();
      }
      if(entry.evicted_) {
        // If the page corresponding to this entry was previously evicted,
        // remove the entry from the map
        entries_.erase(search);
      } else {
        entry.completed_ = true;
        entry.value_ = output;
      }
    }
  } else {
    CacheEntry new_entry;
    new_entry.value_ = output;
    new_entry.completed_ = true;
    insert_entry(key, new_entry);
  }
}

void PredictionCache::insert_entry(const long key, CacheEntry &value) {
  if(page_buffer_.size() < max_size_) {
    page_buffer_.push_back(key);
    page_buffer_index_ = (page_buffer_index_ + 1) % max_size_;
  } else {
    bool inserted = false;
    while(!inserted) {
      long page_key = page_buffer_[page_buffer_index_];
      auto page_entry_search = entries_.find(page_key);
      if(page_entry_search == entries_.end()) {
        throw std::runtime_error("Failed to find corresponding cache entry for a buffer page!");
      }
      CacheEntry& page_entry = page_entry_search->second;
      if(page_entry_search->second.used_) {
        page_entry.used_ = false;
      } else {
        page_buffer_[page_buffer_index_] = key;
        inserted = true;
        page_entry.evicted_ = true;
        if(page_entry.completed_) {
          // If there are no outstanding futures, remove
          // the page's corresponding entry from the entries map
          entries_.erase(page_entry_search);
        }
      }
      page_buffer_index_ = (page_buffer_index_ + 1) % max_size_;
    }
  }
  entries_.insert(std::make_pair(key, std::move(value)));
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
