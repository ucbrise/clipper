#ifndef CLIPPER_UTIL_HPP
#define CLIPPER_UTIL_HPP

namespace container {

template <class T>
class CircularBuffer {
 public:
  explicit CircularBuffer(const size_t capacity) : capacity_(capacity) {
    if (capacity <= 0) {
      throw std::runtime_error(
          "Circular buffer instance must have a positive capacity!");
    }
    items_.reserve(capacity_);
  }

  void insert(T item) {
    if (items_.size() < capacity_) {
      items_.push_back(item);
    } else {
      items_[curr_index_] = item;
    }
    curr_index_ = (curr_index_ + 1) % capacity_;
  }

  std::vector<T> get_items() const {
    std::vector<T> items_to_return;
    items_to_return.reserve(items_.size());
    size_t output_index = curr_index_ % items_.size();
    for (size_t i = 0; i < items_.size(); i++) {
      items_to_return.push_back(items_[output_index]);
      output_index = (output_index + 1) % items_.size();
    }
    return items_to_return;
  }

 private:
  size_t curr_index_ = 0;
  std::vector<T> items_;
  const size_t capacity_;
};
}  // namespace container

#endif  // CLIPPER_UTIL_HPP
