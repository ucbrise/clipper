#ifndef CLIPPER_CONTAINER_PARSING_HPP
#define CLIPPER_CONTAINER_PARSING_HPP

#include <clipper/datatypes.hpp>
#include <vector>

namespace clipper {

namespace container {

template <typename D, class I>
class InputParser {
 public:
  virtual std::vector<D>& get_data_buffer(long min_size_bytes) = 0;
  virtual const std::vector<std::shared_ptr<I>> get_inputs(
      const std::vector<int>& input_header, long input_content_size) = 0;
};

class ByteVectorParser : public InputParser<uint8_t, ByteVector> {
 public:
  std::vector<uint8_t>& get_data_buffer(long min_size_bytes) override;
  const std::vector<std::shared_ptr<ByteVector>> get_inputs(
      const std::vector<int>& input_header, long input_content_size) override;

 private:
  static std::shared_ptr<ByteVector> construct_input(
      std::vector<uint8_t>& data_buffer, int data_start, int data_end);

  std::vector<uint8_t> buffer_;
};

class IntVectorParser : public InputParser<int, IntVector> {
 public:
  std::vector<int>& get_data_buffer(long min_size_bytes) override;
  const std::vector<std::shared_ptr<IntVector>> get_inputs(
      const std::vector<int>& input_header, long input_content_size) override;

 private:
  static std::shared_ptr<IntVector> construct_input(
      std::vector<int>& data_buffer, int data_start, int data_end);

  std::vector<int> buffer_;
};

class FloatVectorParser : public InputParser<float, FloatVector> {
 public:
  std::vector<float>& get_data_buffer(long min_size_bytes) override;
  const std::vector<std::shared_ptr<FloatVector>> get_inputs(
      const std::vector<int>& input_header, long input_content_size) override;

 private:
  static std::shared_ptr<FloatVector> construct_input(
      std::vector<float>& data_buffer, int data_start, int data_end);

  std::vector<float> buffer_;
};

class DoubleVectorParser : public InputParser<double, DoubleVector> {
 public:
  std::vector<double>& get_data_buffer(long min_size_bytes) override;
  const std::vector<std::shared_ptr<DoubleVector>> get_inputs(
      const std::vector<int>& input_header, long input_content_size) override;

 private:
  static std::shared_ptr<DoubleVector> construct_input(
      std::vector<double>& data_buffer, int data_start, int data_end);

  std::vector<double> buffer_;
};

class SerializableStringParser : public InputParser<char, SerializableString> {
 public:
  std::vector<char>& get_data_buffer(long min_size_bytes) override;
  const std::vector<std::shared_ptr<SerializableString>> get_inputs(
      const std::vector<int>& input_header, long input_content_size) override;

 private:
  std::vector<char> buffer_;
};

}  // namespace container

}  // namespace clipper

#endif  // CLIPPER_CONTAINER_PARSING_HPP
