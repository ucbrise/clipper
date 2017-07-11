#ifndef CLIPPER_CONTAINER_PARSING_HPP
#define CLIPPER_CONTAINER_PARSING_HPP

#include <container/datatypes.hpp>
#include <vector>

namespace clipper {

namespace container {

template <typename D>
class InputParser {
 public:
  InputParser(std::vector<D>& buffer) : buffer_(buffer) {

  }
  /**
   * Exposes a buffer into which request data should be
   * read directly from a socket. The buffer contents
   * can then be parsed via `get_inputs`
   */
  std::vector<D>& get_data_buffer(long min_size_bytes) {
    resize_if_necessary(buffer_, min_size_bytes);
    return buffer_;
  }

  const std::vector<std::shared_ptr<Input<D>>> get_inputs(
      const std::vector<int>& input_header, long input_content_size) {
    // For a prediction request containing `n` inputs, there are `n - 1` split
    // indices
    int num_splits = input_header[1] - 1;
    std::vector<std::shared_ptr<Input<D>>> inputs;
    int prev_split = 0;
    // Iterate from the beginning of the input data content
    // (the first two elements of the header are metadata)
    for (int i = 2; i < num_splits + 2; i++) {
      int curr_split = input_header[i];
      std::shared_ptr<Input<D>> input =
          construct_input(buffer_, prev_split, curr_split);
      inputs.push_back(input);
      prev_split = curr_split;
    }
    int typed_input_content_size =
        static_cast<int>(input_content_size / static_cast<long>(sizeof(D)));
    std::shared_ptr<Input<D>> tail_input =
        construct_input(buffer_, prev_split, typed_input_content_size);
    inputs.push_back(tail_input);
    return inputs;
  }

 private:
  std::vector<D>& buffer_;

  void resize_if_necessary(std::vector<D>& buffer, long required_buffer_size) {
    if (static_cast<long>((buffer.size() * sizeof(D))) < required_buffer_size) {
      buffer.reserve((2 * required_buffer_size) / sizeof(D));
    }
  }

  std::shared_ptr<Input<D>> construct_input(
      std::vector<D>& data_buffer, int data_start, int data_end) {
    D* input_data = data_buffer.data() + data_start;
    size_t length = static_cast<size_t>(data_end - data_start);
    std::shared_ptr<Input<D>> input = std::make_shared<Input<D>>(input_data, length);
    return input;
  }
};

}  // namespace container

}  // namespace clipper

#endif  // CLIPPER_CONTAINER_PARSING_HPP
