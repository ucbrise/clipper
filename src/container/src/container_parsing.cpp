#include <container/container_rpc.hpp>

#include <clipper/datatypes.hpp>
#include <container/container_parsing.hpp>

namespace clipper {

namespace container {

template <typename D, class I>
std::vector<std::shared_ptr<I>> get_parsed_inputs(std::vector<D> &data_buffer,
                              const std::vector<int>& input_header,
                              long input_content_size,
                              std::function<std::shared_ptr<I>(std::vector<D>&, int, int)> construct_input) {
  int num_splits = input_header[1];
  std::vector <std::shared_ptr<I>> inputs(num_splits);
  int prev_split = 0;
  // Iterate from the beginning of the input data content
  // (the first two elements of the header are metadata)
  for (int i = 2; i < num_splits + 2; i++) {
    int curr_split = input_header[i];
    std::shared_ptr<I> input = construct_input(data_buffer, prev_split, curr_split);
    inputs.push_back(input);
    prev_split = curr_split;
  }
  std::shared_ptr<I> tail_input = construct_input(data_buffer, prev_split, input_content_size);
  inputs.push_back(tail_input);
  return inputs;
};

template <typename D>
void resize_if_necessary(std::vector<D> &buffer, long required_buffer_size) {
  if(static_cast<long>((buffer.size() * sizeof(D))) < required_buffer_size) {
    buffer.resize((2 * required_buffer_size) / sizeof(D));
  }
}

const std::vector<uint8_t> &ByteVectorParser::get_data_buffer(long min_size_bytes) {
  resize_if_necessary(buffer_, min_size_bytes);
  return buffer_;
}

const std::vector <std::shared_ptr<ByteVector>> ByteVectorParser::get_inputs(
    const std::vector<int>& input_header, long input_content_size) {
  return get_parsed_inputs(
      buffer_,
      input_header,
      input_content_size,
      std::function<std::shared_ptr<ByteVector>(std::vector<uint8_t>&, int, int)>(construct_input));
}

std::shared_ptr<ByteVector> ByteVectorParser::construct_input(std::vector<uint8_t>& data_buffer,
                                                              int data_start,
                                                              int data_end) {
  std::vector<uint8_t> input_data(data_end - data_start);
  // TODO(czumar): Verify constant complexity of these next operations
  auto begin = std::next(data_buffer.begin(), data_start);
  auto end = std::next(data_buffer.begin(), data_end);
  input_data.insert(input_data.end(), std::make_move_iterator(begin), std::make_move_iterator(end));
  std::shared_ptr<ByteVector> input = std::make_shared<ByteVector>(input_data);
  return input;
}

const std::vector<int>& IntVectorParser::get_data_buffer(long min_size_bytes) {
  resize_if_necessary(buffer_, min_size_bytes);
  return buffer_;
}

const std::vector<std::shared_ptr<IntVector>> IntVectorParser::get_inputs(
    const std::vector<int>& input_header, long input_content_size) {
  return get_parsed_inputs(
      buffer_,
      input_header,
      input_content_size,
      std::function<std::shared_ptr<IntVector>(std::vector<int>&, int, int)>(construct_input));
}

std::shared_ptr<IntVector> IntVectorParser::construct_input(std::vector<int> &data_buffer,
                                                            int data_start,
                                                            int data_end) {
  std::vector<int> input_data(data_end - data_start);
  // TODO(czumar): Verify constant complexity of these next operations
  auto begin = std::next(data_buffer.begin(), data_start);
  auto end = std::next(data_buffer.begin(), data_end);
  input_data.insert(input_data.end(), std::make_move_iterator(begin), std::make_move_iterator(end));
  std::shared_ptr<IntVector> input = std::make_shared<IntVector>(input_data);
  return input;
}

const std::vector<float>& FloatVectorParser::get_data_buffer(long min_size_bytes) {
  resize_if_necessary(buffer_, min_size_bytes);
  return buffer_;
}

const std::vector<std::shared_ptr<FloatVector>> FloatVectorParser::get_inputs(
    const std::vector<int>& input_header, long input_content_size) {
  return get_parsed_inputs(
      buffer_,
      input_header,
      input_content_size,
      std::function<std::shared_ptr<FloatVector>(std::vector<float>&, int, int)>(construct_input));
}

std::shared_ptr<FloatVector> FloatVectorParser::construct_input(std::vector<float> &data_buffer,
                                                                int data_start,
                                                                int data_end) {
  std::vector<float> input_data(data_end - data_start);
  // TODO(czumar): Verify constant complexity of these next operations
  auto begin = std::next(data_buffer.begin(), data_start);
  auto end = std::next(data_buffer.begin(), data_end);
  input_data.insert(input_data.end(), std::make_move_iterator(begin), std::make_move_iterator(end));
  std::shared_ptr<FloatVector> input = std::make_shared<FloatVector>(input_data);
  return input;
}

const std::vector<double>& DoubleVectorParser::get_data_buffer(long min_size_bytes) {
  resize_if_necessary(buffer_, min_size_bytes);
  return buffer_;
}

const std::vector<std::shared_ptr<DoubleVector>> DoubleVectorParser::get_inputs(
    const std::vector<int>& input_header, long input_content_size) {
  return get_parsed_inputs(
      buffer_,
      input_header,
      input_content_size,
      std::function<std::shared_ptr<DoubleVector>(std::vector<double>&, int, int)>(construct_input));
}

std::shared_ptr<DoubleVector> DoubleVectorParser::construct_input(std::vector<double> &data_buffer,
                                                                  int data_start,
                                                                  int data_end) {
  std::vector<double> input_data(data_end - data_start);
  // TODO(czumar): Verify constant complexity of these next operations
  auto begin = std::next(data_buffer.begin(), data_start);
  auto end = std::next(data_buffer.begin(), data_end);
  input_data.insert(input_data.end(), std::make_move_iterator(begin), std::make_move_iterator(end));
  std::shared_ptr<DoubleVector> input = std::make_shared<DoubleVector>(input_data);
  return input;
}

} // namespace container

} // namespace clipper