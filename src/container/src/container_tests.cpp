#include <algorithm>

#include <gtest/gtest.h>

#include <clipper/datatypes.hpp>
#include <container/container_parsing.hpp>
#include <container/container_rpc.hpp>
<<<<<<< fe5ba92482c3b48f13993d05b4e634b00cb04b2a
#include <container/datatypes.hpp>
=======
>>>>>>> Format code

using namespace clipper::container;

namespace {

using ByteVectorParser = InputParser<uint8_t>;
using IntVectorParser = InputParser<int>;
using FloatVectorParser = InputParser<float>;
using DoubleVectorParser = InputParser<double>;
using SerializableStringParser = InputParser<char>;

template <typename T>
std::vector<std::vector<T>> create_primitive_parser_vecs() {
  std::vector<T> first_input_vec{static_cast<T>(1), static_cast<T>(2),
                                 static_cast<T>(3)};
  std::vector<T> second_input_vec{static_cast<T>(4), static_cast<T>(5)};
  std::vector<T> third_input_vec{static_cast<T>(6)};

  std::vector<std::vector<T>> vecs;
  vecs.push_back(first_input_vec);
  vecs.push_back(second_input_vec);
  vecs.push_back(third_input_vec);
  return vecs;
}

template <typename D>
std::vector<Input<D>> get_inputs_from_prediction_request(
    clipper::rpc::PredictionRequest& prediction_request,
    InputParser<D>& input_parser) {
  std::vector<clipper::ByteBuffer> serialized_request =
      prediction_request.serialize();

  auto raw_input_header = serialized_request[2];
  int* input_header = reinterpret_cast<int*>(raw_input_header.data());
  std::vector<int> header_vec(
      input_header, input_header + raw_input_header.size() / sizeof(int));

  auto raw_content = serialized_request[4];
  D* content = reinterpret_cast<D*>(raw_content.data());
  std::vector<D> content_vec(content, content + raw_content.size() / sizeof(D));

  std::vector<D>& parser_buffer =
      input_parser.get_data_buffer(raw_content.size());
  std::copy(content_vec.begin(), content_vec.end(), parser_buffer.begin());

  return input_parser.get_inputs(header_vec, raw_content.size());
};

TEST(ContainerTests, ByteVectorParserCreatesInputsFromRawContentCorrectly) {
  std::vector<std::vector<uint8_t>> input_vecs;
  for (int i = 0; i < 3; i++) {
    std::vector<uint8_t> input_vec;
    uint8_t* vec_data = reinterpret_cast<uint8_t*>(&i);
    for (size_t j = 0; j < sizeof(int); j++) {
      input_vec.push_back(vec_data[j]);
    }
    input_vecs.push_back(input_vec);
  }

  std::vector<std::shared_ptr<clipper::Input>> inputs;
  for (auto const& vec : input_vecs) {
    inputs.push_back(std::make_shared<clipper::ByteVector>(vec));
  }

  ASSERT_EQ(input_vecs.size(), inputs.size());

  clipper::rpc::PredictionRequest prediction_request(inputs,
                                                     clipper::InputType::Bytes);

  std::vector<uint8_t> input_buffer;
  ByteVectorParser parser(input_buffer);

  std::vector<ByteVector> parsed_inputs =
      get_inputs_from_prediction_request<uint8_t>(prediction_request, parser);

  ASSERT_EQ(parsed_inputs.size(), input_vecs.size());
  for (size_t i = 0; i < parsed_inputs.size(); i++) {
    const uint8_t* input_content = parsed_inputs[i].get_data();
    size_t input_length = parsed_inputs[i].get_length();
    ASSERT_EQ(input_length, input_vecs[i].size());
    for (size_t j = 0; j < input_length; j++) {
      ASSERT_EQ(input_content[j], input_vecs[i][j]);
    }
  }
}

TEST(ContainerTests, IntVectorParserCreatesInputsFromRawContentCorrectly) {
  std::vector<std::vector<int>> input_vecs =
      create_primitive_parser_vecs<int>();

  std::vector<std::shared_ptr<clipper::Input>> inputs;
  for (auto const& vec : input_vecs) {
    inputs.push_back(std::make_shared<clipper::IntVector>(vec));
  }

  ASSERT_EQ(input_vecs.size(), inputs.size());

  clipper::rpc::PredictionRequest prediction_request(inputs,
                                                     clipper::InputType::Ints);

  std::vector<int> input_buffer;
  IntVectorParser parser(input_buffer);

  std::vector<IntVector> parsed_inputs =
      get_inputs_from_prediction_request<int>(prediction_request, parser);

  ASSERT_EQ(parsed_inputs.size(), input_vecs.size());
  for (size_t i = 0; i < parsed_inputs.size(); i++) {
    const int* input_content = parsed_inputs[i].get_data();
    size_t input_length = parsed_inputs[i].get_length();
    ASSERT_EQ(input_length, input_vecs[i].size());
    for (size_t j = 0; j < input_length; j++) {
      ASSERT_EQ(input_content[j], input_vecs[i][j]);
    }
  }
}

TEST(ContainerTests, FloatVectorParserCreatesInputsFromRawContentCorrectly) {
  std::vector<std::vector<float>> input_vecs =
      create_primitive_parser_vecs<float>();

  std::vector<std::shared_ptr<clipper::Input>> inputs;
  for (auto const& vec : input_vecs) {
    inputs.push_back(std::make_shared<clipper::FloatVector>(vec));
  }

  ASSERT_EQ(input_vecs.size(), inputs.size());

  clipper::rpc::PredictionRequest prediction_request(
      inputs, clipper::InputType::Floats);

  std::vector<float> input_buffer;
  FloatVectorParser parser(input_buffer);

  std::vector<FloatVector> parsed_inputs =
      get_inputs_from_prediction_request<float>(prediction_request, parser);

  ASSERT_EQ(parsed_inputs.size(), input_vecs.size());
  for (size_t i = 0; i < parsed_inputs.size(); i++) {
    const float* input_content = parsed_inputs[i].get_data();
    size_t input_length = parsed_inputs[i].get_length();
    ASSERT_EQ(input_length, input_vecs[i].size());
    for (size_t j = 0; j < input_length; j++) {
      ASSERT_EQ(input_content[j], input_vecs[i][j]);
    }
  }
}

TEST(ContainerTests, DoubleVectorParserCreatesInputsFromRawContentCorrectly) {
  std::vector<std::vector<double>> input_vecs =
      create_primitive_parser_vecs<double>();

  std::vector<std::shared_ptr<clipper::Input>> inputs;
  for (auto const& vec : input_vecs) {
    inputs.push_back(std::make_shared<clipper::DoubleVector>(vec));
  std::vector<std::vector<double>> input_vecs = create_primitive_parser_vecs<double>();

  std::vector<std::shared_ptr<Input>> inputs;
  for (auto const& vec : input_vecs) {
    inputs.push_back(std::make_shared<DoubleVector>(vec));
  clipper::rpc::PredictionRequest prediction_request(
      inputs, clipper::InputType::Doubles);

  std::vector<double> input_buffer;
  DoubleVectorParser parser(input_buffer);

  std::vector<DoubleVector> parsed_inputs =
      get_inputs_from_prediction_request<double>(prediction_request, parser);

  ASSERT_EQ(parsed_inputs.size(), input_vecs.size());
  for (size_t i = 0; i < parsed_inputs.size(); i++) {
    const double* input_content = parsed_inputs[i].get_data();
    size_t input_length = parsed_inputs[i].get_length();
    ASSERT_EQ(input_length, input_vecs[i].size());
    for (size_t j = 0; j < input_length; j++) {
      ASSERT_EQ(input_content[j], input_vecs[i][j]);
    }
  }
}

TEST(ContainerTests,
     SerializableStringParserCreatesInputsFromRawContentCorrectly) {
  std::vector<std::string> input_strs;
  input_strs.push_back("first_test_string");
  input_strs.push_back("%*7&3333$$$$");
  char16_t unicode_char = u'\u00F6';
  input_strs.push_back(std::to_string(unicode_char));

  std::vector<std::shared_ptr<clipper::Input>> inputs;
  for (auto const& vec : input_strs) {
    inputs.push_back(std::make_shared<clipper::SerializableString>(vec));
  }

  ASSERT_EQ(input_strs.size(), inputs.size());

  clipper::rpc::PredictionRequest prediction_request(
      inputs, clipper::InputType::Strings);

  std::vector<char> input_buffer;
  SerializableStringParser parser(input_buffer);

  std::vector<SerializableString> parsed_inputs =
      get_inputs_from_prediction_request<char>(prediction_request, parser);

  ASSERT_EQ(parsed_inputs.size(), input_strs.size());
  for (size_t i = 0; i < parsed_inputs.size(); i++) {
    const char* input_content = parsed_inputs[i].get_data();
    std::string input_str(input_content);
    ASSERT_EQ(input_str, input_strs[i]);
  }
}

}  // namespace
