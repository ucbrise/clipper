#include <algorithm>

#include <gtest/gtest.h>

#include <container/container_parsing.hpp>
#include <container/container_rpc.hpp>
#include <clipper/datatypes.hpp>

using namespace clipper;

namespace {

template <typename T>
std::vector<std::vector<T>> create_primitive_parser_vecs() {
  std::vector<T> first_input_vec {static_cast<T>(1), static_cast<T>(2), static_cast<T>(3)};
  std::vector<T> second_input_vec {static_cast<T>(4), static_cast<T>(5)};
  std::vector<T> third_input_vec {static_cast<T>(6)};

  std::vector<std::vector<T>> vecs;
  vecs.push_back(first_input_vec);
  vecs.push_back(second_input_vec);
  vecs.push_back(third_input_vec);
  return vecs;
}

template <typename D, class I>
std::vector<std::shared_ptr<I>> get_inputs_from_prediction_request(
    rpc::PredictionRequest& prediction_request, container::InputParser<D,I>& input_parser) {
  std::vector<ByteBuffer> serialized_request = prediction_request.serialize();

  auto raw_input_header = serialized_request[2];
  int* input_header = reinterpret_cast<int*>(raw_input_header.data());
  std::vector<int> header_vec(input_header, input_header + raw_input_header.size() / sizeof(int));

  auto raw_content = serialized_request[4];
  D* content = reinterpret_cast<D*>(raw_content.data());
  std::vector<D> content_vec(content, content + raw_content.size() / sizeof(D));

  std::vector<D>& parser_buffer = input_parser.get_data_buffer(raw_content.size());
  std::copy(content_vec.begin(), content_vec.end(), parser_buffer.begin());

  return input_parser.get_inputs(header_vec, raw_content.size());
};

TEST(ContainerTests, IntVectorParserCreatesInputsFromRawContentCorrectly) {
  std::vector<std::vector<int>> input_vecs = create_primitive_parser_vecs<int>();

  std::vector<std::shared_ptr<Input>> inputs;
  for(auto const& vec : input_vecs) {
    inputs.push_back(std::make_shared<IntVector>(vec));
  }

  ASSERT_EQ(input_vecs.size(), inputs.size());

  rpc::PredictionRequest prediction_request(inputs, InputType::Ints);
  container::IntVectorParser parser;

  std::vector<std::shared_ptr<IntVector>> parsed_inputs =
      get_inputs_from_prediction_request(prediction_request, parser);
  ASSERT_EQ(parsed_inputs.size(), input_vecs.size());
  for(int i = 0; i < static_cast<int>(parsed_inputs.size()); i++) {
    ASSERT_EQ(parsed_inputs[i]->get_data(), input_vecs[i]);
  }
}

TEST(ContainerTests, FloatVectorParserCreatesInputsFromRawContentCorrectly) {
  std::vector<std::vector<float>> input_vecs = create_primitive_parser_vecs<float>();

  std::vector<std::shared_ptr<Input>> inputs;
  for(auto const& vec : input_vecs) {
    inputs.push_back(std::make_shared<FloatVector>(vec));
  }

  ASSERT_EQ(input_vecs.size(), inputs.size());

  rpc::PredictionRequest prediction_request(inputs, InputType::Floats);
  container::FloatVectorParser parser;

  std::vector<std::shared_ptr<FloatVector>> parsed_inputs =
      get_inputs_from_prediction_request(prediction_request, parser);
  ASSERT_EQ(parsed_inputs.size(), input_vecs.size());
  for(int i = 0; i < static_cast<int>(parsed_inputs.size()); i++) {
    ASSERT_EQ(parsed_inputs[i]->get_data(), input_vecs[i]);
  }
}

TEST(ContainerTests, DoubleVectorParserCreatesInputsFromRawContentCorrectly) {
  std::vector<std::vector<double>> input_vecs = create_primitive_parser_vecs<double>();

  std::vector<std::shared_ptr<Input>> inputs;
  for(auto const& vec : input_vecs) {
    inputs.push_back(std::make_shared<DoubleVector>(vec));
  }

  ASSERT_EQ(input_vecs.size(), inputs.size());

  rpc::PredictionRequest prediction_request(inputs, InputType::Doubles);
  container::DoubleVectorParser parser;

  std::vector<std::shared_ptr<DoubleVector>> parsed_inputs =
      get_inputs_from_prediction_request(prediction_request, parser);

  ASSERT_EQ(parsed_inputs.size(), input_vecs.size());
  for(int i = 0; i < static_cast<int>(parsed_inputs.size()); i++) {
    ASSERT_EQ(parsed_inputs[i]->get_data(), input_vecs[i]);
  }
}




}