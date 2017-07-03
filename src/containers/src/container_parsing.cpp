#include "container_parsing.hpp"

#include <clipper/datatypes.hpp>

namespace clipper {

namespace container {

template <typename D, class I>
std::vector<std::shared_ptr<I>> get_parsed_inputs(std::vector<D> &data_buffer,
                              const std::vector<long>& input_splits,
                              long num_splits,
                              long input_content_size,
                              std::function<std::shared_ptr<I>(std::vector<D>&, long, long)> construct_input) {
  std::vector <std::shared_ptr<I>> inputs(num_splits);
  long prev_split = 0;
  for (int i = 0; i < num_splits; i++) {
    long curr_split = input_splits[i];
    std::shared_ptr<I> input = construct_input(data_buffer, prev_split, curr_split);
    inputs.push_back(input);
    prev_split = curr_split;
  }
  std::shared_ptr<I> tail_input = construct_input(data_buffer, prev_split, input_content_size);
  inputs.push_back(tail_input);
  return inputs;
};

template <typename D>
void resize_if_necessary(std::vector<D> buffer, long required_buffer_size) {
  if(static_cast<long>((buffer.size() / sizeof(D))) < required_buffer_size) {
    buffer.resize((2 * required_buffer_size) / sizeof(D));
  }
}

const std::vector<uint8_t> &ByteVectorParser::get_data_buffer(long min_size_bytes) {
  resize_if_necessary(buffer_, min_size_bytes);
  return buffer_;
}

const std::vector <std::shared_ptr<ByteVector>> ByteVectorParser::get_inputs(
    const std::vector<long>& input_splits, long num_splits, long input_content_size) {
  return get_parsed_inputs(
      buffer_,
      input_splits,
      num_splits,
      input_content_size,
      std::function<std::shared_ptr<ByteVector>(std::vector<uint8_t>&, long, long)>(construct_input));
}

std::shared_ptr<ByteVector> ByteVectorParser::construct_input(std::vector<uint8_t>& data_buffer,
                                                              long data_start,
                                                              long data_end) {
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
    const std::vector<long>& input_splits, long num_splits, long input_content_size) {
  return get_parsed_inputs(
      buffer_,
      input_splits,
      num_splits,
      input_content_size,
      std::function<std::shared_ptr<IntVector>(std::vector<int>&, long, long)>(construct_input));
}

std::shared_ptr<IntVector> IntVectorParser::construct_input(std::vector<int> &data_buffer,
                                                            long data_start,
                                                            long data_end) {
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
    const std::vector<long>& input_splits, long num_splits, long input_content_size) {
  return get_parsed_inputs(
      buffer_,
      input_splits,
      num_splits,
      input_content_size,
      std::function<std::shared_ptr<FloatVector>(std::vector<float>&, long, long)>(construct_input));
}

std::shared_ptr<FloatVector> FloatVectorParser::construct_input(std::vector<float> &data_buffer,
                                                                long data_start,
                                                                long data_end) {
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
    const std::vector<long>& input_splits, long num_splits, long input_content_size) {
  return get_parsed_inputs(
      buffer_,
      input_splits,
      num_splits,
      input_content_size,
      std::function<std::shared_ptr<DoubleVector>(std::vector<double>&, long, long)>(construct_input));
}

std::shared_ptr<DoubleVector> DoubleVectorParser::construct_input(std::vector<double> &data_buffer,
                                                                  long data_start,
                                                                  long data_end) {
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