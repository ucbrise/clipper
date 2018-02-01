#include <stdexcept>
#include <type_traits>

#include <gtest/gtest.h>
#include <clipper/datatypes.hpp>

using namespace clipper;
using std::vector;

namespace {

const unsigned long SERIALIZED_REQUEST_HEADERS_SIZE = 3;
const int NUM_PRIMITIVE_INPUTS = 500;
const int NUM_STRING_INPUTS = 300;
const uint8_t PRIMITIVE_INPUT_SIZE_ELEMS = 200;

/**
 * @return A pair consisting of a shared pointer to the primitive
 * data and the number of data elements
 */
template <typename T>
std::vector<std::pair<SharedPoolPtr<T>, size_t>> get_primitive_data_items() {
  std::vector<std::pair<SharedPoolPtr<T>, size_t>> data_items;
  for (int i = 0; i < NUM_PRIMITIVE_INPUTS; i++) {
    SharedPoolPtr<T> data_item(static_cast<T*>(malloc(PRIMITIVE_INPUT_SIZE_ELEMS * sizeof(T))), free);
    T* data_item_raw = data_item.get();
    for (uint8_t j = 0; j < PRIMITIVE_INPUT_SIZE_ELEMS; j++) {
      // Differentiate vectors by populating them with the index j under
      // differing moduli
      int k = static_cast<int>(j) % (i + 1);
      data_item_raw[j] = k;
    }
    data_items.push_back(std::make_pair(std::move(data_item), PRIMITIVE_INPUT_SIZE_ELEMS));
  }
  return data_items;
}

void get_string_data(std::vector<std::string>& string_vector) {
  for (int i = 0; i < NUM_STRING_INPUTS; i++) {
    std::string str;
    switch (i % 3) {
      case 0: str = std::string("CAT"); break;
      case 1: str = std::string("DOG"); break;
      case 2: str = std::string("COW"); break;
      default: str = std::string("INVALID"); break;
    }
    string_vector.push_back(str);
  }
}

TEST(InputSerializationTests, EmptySerialization) {
  clipper::rpc::PredictionRequest request(InputType::Bytes);
  try {
    request.serialize();
  } catch (std::length_error) {
    SUCCEED();
    return;
  }
  FAIL();
}

TEST(InputSerializationTests, ByteSerialization) {
  clipper::rpc::PredictionRequest request(InputType::Bytes);
  auto data_items = get_primitive_data_items<uint8_t>();
  for (size_t i = 0; i < data_items.size(); i++) {
    std::shared_ptr<ByteVector> byte_vec =
        std::make_shared<ByteVector>(data_items[i].first, 0, data_items[i].second);
    request.add_input(byte_vec);
  }
  std::vector<clipper::ByteBuffer> serialized_request = request.serialize();
  ASSERT_EQ(serialized_request.size(), SERIALIZED_REQUEST_HEADERS_SIZE + data_items.size());

  clipper::ByteBuffer &request_header_buf = serialized_request[0];
  clipper::ByteBuffer &input_header_size_buf = serialized_request[1];
  clipper::ByteBuffer &input_header_buf = serialized_request[2];

  uint32_t* request_header_data = static_cast<uint32_t*>(get_raw(std::move(std::get<0>(request_header_buf))));
  size_t request_header_start = std::get<1>(request_header_buf);

  uint64_t* input_header_size_data = static_cast<uint64_t*>(get_raw(std::move(std::get<0>(input_header_size_buf))));
  size_t input_header_size_start = std::get<1>(input_header_size_buf);

  uint64_t* input_header_data = static_cast<uint64_t*>(get_raw(std::move(std::get<0>(input_header_buf))));
  size_t input_header_start = std::get<1>(input_header_buf);
  size_t input_header_size = std::get<2>(input_header_buf);

  uint64_t parsed_input_header_size = input_header_size_data[input_header_size_start / sizeof(uint64_t)];
  ASSERT_EQ(parsed_input_header_size, input_header_size);

  uint32_t raw_request_type = request_header_data[request_header_start / sizeof(uint32_t)];
  ASSERT_EQ(raw_request_type, static_cast<uint32_t>(clipper::RequestType::PredictRequest));

  input_header_data += input_header_start / sizeof(uint64_t);
  uint64_t raw_input_type = input_header_data[0];
  ASSERT_EQ(raw_input_type, static_cast<uint64_t>(clipper::DataType::Bytes));
  uint64_t num_inputs = input_header_data[1];
  ASSERT_EQ(num_inputs, data_items.size());
  ASSERT_EQ(num_inputs, (input_header_size / sizeof(uint64_t)) - 2);

  for (uint64_t i = 0; i < num_inputs; ++i) {
    clipper::ByteBuffer &serialized_input = serialized_request[i + 3];
    size_t serialized_input_start_byte = std::get<1>(serialized_input);
    size_t serialized_input_size_bytes = std::get<2>(serialized_input);
    uint8_t* input_data = data_items[i].first.get();
    size_t input_size_bytes = data_items[i].second * sizeof(uint8_t);
    size_t parsed_input_size_bytes = input_header_data[i + 2];
    ASSERT_EQ(serialized_input_size_bytes, input_size_bytes);
    ASSERT_EQ(parsed_input_size_bytes, input_size_bytes);
    uint8_t* serialized_input_data = static_cast<uint8_t*>(get_raw(std::move(std::get<0>(serialized_input))));
    serialized_input_data += (serialized_input_start_byte / sizeof(uint8_t));
    for(size_t j = 0; j < input_size_bytes / sizeof(uint8_t); ++j) {
      ASSERT_EQ(serialized_input_data[j], input_data[j]);
    }
  }
}

TEST(InputSerializationTests, IntSerialization) {
  clipper::rpc::PredictionRequest request(InputType::Ints);
  auto data_items = get_primitive_data_items<int>();
  for (size_t i = 0; i < data_items.size(); i++) {
    std::shared_ptr<IntVector> int_vec =
        std::make_shared<IntVector>(data_items[i].first, 0, data_items[i].second);
    request.add_input(int_vec);
  }
  std::vector<clipper::ByteBuffer> serialized_request = request.serialize();
  ASSERT_EQ(serialized_request.size(), SERIALIZED_REQUEST_HEADERS_SIZE + data_items.size());

  clipper::ByteBuffer &request_header_buf = serialized_request[0];
  clipper::ByteBuffer &input_header_size_buf = serialized_request[1];
  clipper::ByteBuffer &input_header_buf = serialized_request[2];

  uint32_t* request_header_data = static_cast<uint32_t*>(get_raw(std::move(std::get<0>(request_header_buf))));
  size_t request_header_start = std::get<1>(request_header_buf);

  uint64_t* input_header_size_data = static_cast<uint64_t*>(get_raw(std::move(std::get<0>(input_header_size_buf))));
  size_t input_header_size_start = std::get<1>(input_header_size_buf);

  uint64_t* input_header_data = static_cast<uint64_t*>(get_raw(std::move(std::get<0>(input_header_buf))));
  size_t input_header_start = std::get<1>(input_header_buf);
  size_t input_header_size = std::get<2>(input_header_buf);

  uint64_t parsed_input_header_size = input_header_size_data[input_header_size_start / sizeof(uint64_t)];
  ASSERT_EQ(parsed_input_header_size, input_header_size);

  uint32_t raw_request_type = request_header_data[request_header_start / sizeof(uint32_t)];
  ASSERT_EQ(raw_request_type, static_cast<uint32_t>(clipper::RequestType::PredictRequest));

  input_header_data += input_header_start / sizeof(uint64_t);
  uint64_t raw_input_type = input_header_data[0];
  ASSERT_EQ(raw_input_type, static_cast<uint64_t>(clipper::DataType::Ints));
  uint64_t num_inputs = input_header_data[1];
  ASSERT_EQ(num_inputs, data_items.size());
  ASSERT_EQ(num_inputs, (input_header_size / sizeof(uint64_t)) - 2);

  for (uint64_t i = 0; i < num_inputs; ++i) {
    clipper::ByteBuffer &serialized_input = serialized_request[i + 3];
    size_t serialized_input_start_byte = std::get<1>(serialized_input);
    size_t serialized_input_size_bytes = std::get<2>(serialized_input);
    int* input_data = data_items[i].first.get();
    size_t input_size_bytes = data_items[i].second * sizeof(int);
    size_t parsed_input_size_bytes = input_header_data[i + 2];
    ASSERT_EQ(serialized_input_size_bytes, input_size_bytes);
    ASSERT_EQ(parsed_input_size_bytes, input_size_bytes);
    int* serialized_input_data = static_cast<int*>(get_raw(std::move(std::get<0>(serialized_input))));
    serialized_input_data += (serialized_input_start_byte / sizeof(int));
    for(size_t j = 0; j < input_size_bytes / sizeof(int); ++j) {
      ASSERT_EQ(serialized_input_data[j], input_data[j]);
    }
  }
}

TEST(InputSerializationTests, FloatSerialization) {
  clipper::rpc::PredictionRequest request(InputType::Floats);
  auto data_items = get_primitive_data_items<float>();
  for (size_t i = 0; i < data_items.size(); i++) {
    std::shared_ptr<FloatVector> float_vec =
        std::make_shared<FloatVector>(data_items[i].first, 0, data_items[i].second);
    request.add_input(float_vec);
  }
  std::vector<clipper::ByteBuffer> serialized_request = request.serialize();
  ASSERT_EQ(serialized_request.size(), SERIALIZED_REQUEST_HEADERS_SIZE + data_items.size());

  clipper::ByteBuffer &request_header_buf = serialized_request[0];
  clipper::ByteBuffer &input_header_size_buf = serialized_request[1];
  clipper::ByteBuffer &input_header_buf = serialized_request[2];

  uint32_t* request_header_data = static_cast<uint32_t*>(get_raw(std::move(std::get<0>(request_header_buf))));
  size_t request_header_start = std::get<1>(request_header_buf);

  uint64_t* input_header_size_data = static_cast<uint64_t*>(get_raw(std::move(std::get<0>(input_header_size_buf))));
  size_t input_header_size_start = std::get<1>(input_header_size_buf);

  uint64_t* input_header_data = static_cast<uint64_t*>(get_raw(std::move(std::get<0>(input_header_buf))));
  size_t input_header_start = std::get<1>(input_header_buf);
  size_t input_header_size = std::get<2>(input_header_buf);

  uint64_t parsed_input_header_size = input_header_size_data[input_header_size_start / sizeof(uint64_t)];
  ASSERT_EQ(parsed_input_header_size, input_header_size);

  uint32_t raw_request_type = request_header_data[request_header_start / sizeof(uint32_t)];
  ASSERT_EQ(raw_request_type, static_cast<uint32_t>(clipper::RequestType::PredictRequest));

  input_header_data += input_header_start / sizeof(uint64_t);
  uint64_t raw_input_type = input_header_data[0];
  ASSERT_EQ(raw_input_type, static_cast<uint64_t>(clipper::DataType::Floats));
  uint64_t num_inputs = input_header_data[1];
  ASSERT_EQ(num_inputs, data_items.size());
  ASSERT_EQ(num_inputs, (input_header_size / sizeof(uint64_t)) - 2);

  for (uint64_t i = 0; i < num_inputs; ++i) {
    clipper::ByteBuffer &serialized_input = serialized_request[i + 3];
    size_t serialized_input_start_byte = std::get<1>(serialized_input);
    size_t serialized_input_size_bytes = std::get<2>(serialized_input);
    float* input_data = data_items[i].first.get();
    size_t input_size_bytes = data_items[i].second * sizeof(float);
    size_t parsed_input_size_bytes = input_header_data[i + 2];
    ASSERT_EQ(serialized_input_size_bytes, input_size_bytes);
    ASSERT_EQ(parsed_input_size_bytes, input_size_bytes);
    float* serialized_input_data = static_cast<float*>(get_raw(std::move(std::get<0>(serialized_input))));
    serialized_input_data += (serialized_input_start_byte / sizeof(float));
    for(size_t j = 0; j < input_size_bytes / sizeof(float); ++j) {
      ASSERT_EQ(serialized_input_data[j], input_data[j]);
    }
  }
}

TEST(InputSerializationTests, DoubleSerialization) {
  clipper::rpc::PredictionRequest request(InputType::Doubles);
  auto data_items = get_primitive_data_items<double>();
  for (size_t i = 0; i < data_items.size(); i++) {
    std::shared_ptr<DoubleVector> double_vec =
        std::make_shared<DoubleVector>(data_items[i].first, 0, data_items[i].second);
    request.add_input(double_vec);
  }
  std::vector<clipper::ByteBuffer> serialized_request = request.serialize();
  ASSERT_EQ(serialized_request.size(), SERIALIZED_REQUEST_HEADERS_SIZE + data_items.size());

  clipper::ByteBuffer &request_header_buf = serialized_request[0];
  clipper::ByteBuffer &input_header_size_buf = serialized_request[1];
  clipper::ByteBuffer &input_header_buf = serialized_request[2];

  uint32_t* request_header_data = static_cast<uint32_t*>(get_raw(std::move(std::get<0>(request_header_buf))));
  size_t request_header_start = std::get<1>(request_header_buf);

  uint64_t* input_header_size_data = static_cast<uint64_t*>(get_raw(std::move(std::get<0>(input_header_size_buf))));
  size_t input_header_size_start = std::get<1>(input_header_size_buf);

  uint64_t* input_header_data = static_cast<uint64_t*>(get_raw(std::move(std::get<0>(input_header_buf))));
  size_t input_header_start = std::get<1>(input_header_buf);
  size_t input_header_size = std::get<2>(input_header_buf);

  uint64_t parsed_input_header_size = input_header_size_data[input_header_size_start / sizeof(uint64_t)];
  ASSERT_EQ(parsed_input_header_size, input_header_size);

  uint32_t raw_request_type = request_header_data[request_header_start / sizeof(uint32_t)];
  ASSERT_EQ(raw_request_type, static_cast<uint32_t>(clipper::RequestType::PredictRequest));

  input_header_data += input_header_start / sizeof(uint64_t);
  uint64_t raw_input_type = input_header_data[0];
  ASSERT_EQ(raw_input_type, static_cast<uint64_t>(clipper::DataType::Doubles));
  uint64_t num_inputs = input_header_data[1];
  ASSERT_EQ(num_inputs, data_items.size());
  ASSERT_EQ(num_inputs, (input_header_size / sizeof(uint64_t)) - 2);

  for (uint64_t i = 0; i < num_inputs; ++i) {
    clipper::ByteBuffer &serialized_input = serialized_request[i + 3];
    size_t serialized_input_start_byte = std::get<1>(serialized_input);
    size_t serialized_input_size_bytes = std::get<2>(serialized_input);
    double* input_data = data_items[i].first.get();
    size_t input_size_bytes = data_items[i].second * sizeof(double);
    size_t parsed_input_size_bytes = input_header_data[i + 2];
    ASSERT_EQ(serialized_input_size_bytes, input_size_bytes);
    ASSERT_EQ(parsed_input_size_bytes, input_size_bytes);
    double* serialized_input_data = static_cast<double*>(get_raw(std::move(std::get<0>(serialized_input))));
    serialized_input_data += (serialized_input_start_byte / sizeof(double));
    for(size_t j = 0; j < input_size_bytes / sizeof(double); ++j) {
      ASSERT_EQ(serialized_input_data[j], input_data[j]);
    }
  }
}
//
//TEST(InputSerializationTests, StringSerialization) {
//  clipper::rpc::PredictionRequest request(InputType::Strings);
//  std::vector<std::string> string_vector;
//  get_string_data(string_vector);
//  for (int i = 0; i < (int)string_vector.size(); i++) {
//    std::shared_ptr<SerializableString> serializable_str =
//        std::make_shared<SerializableString>(string_vector[i]);
//    request.add_input(serializable_str);
//  }
//  std::vector<clipper::ByteBuffer> serialized_request = request.serialize();
//  ASSERT_EQ(serialized_request.size(), SERIALIZED_REQUEST_SIZE);
//  clipper::ByteBuffer request_header = serialized_request[0];
//  clipper::ByteBuffer input_header_size = serialized_request[1];
//  clipper::ByteBuffer input_header = serialized_request[2];
//  clipper::ByteBuffer input_content_size = serialized_request[3];
//  clipper::ByteBuffer input_content = serialized_request[4];
//
//  long* raw_input_header_size =
//      reinterpret_cast<long*>(input_header_size.data());
//  ASSERT_EQ(static_cast<size_t>(raw_input_header_size[0]), input_header.size());
//  long* raw_input_content_size =
//      reinterpret_cast<long*>(input_content_size.data());
//  ASSERT_EQ(static_cast<size_t>(raw_input_content_size[0]),
//            input_content.size());
//
//  uint32_t* raw_request_type =
//      reinterpret_cast<uint32_t*>(request_header.data());
//  ASSERT_EQ(*raw_request_type,
//            static_cast<uint32_t>(clipper::RequestType::PredictRequest));
//  uint32_t* typed_input_header =
//      reinterpret_cast<uint32_t*>(input_header.data());
//  ASSERT_EQ(*typed_input_header,
//            static_cast<uint32_t>(clipper::InputType::Strings));
//  typed_input_header++;
//  uint32_t num_inputs = *typed_input_header;
//  ASSERT_EQ(num_inputs, static_cast<uint32_t>(string_vector.size()));
//  typed_input_header++;
//
//  char* content_ptr = reinterpret_cast<char*>(input_content.data());
//  for (int i = 0; i < (int)num_inputs; i++) {
//    std::string str(content_ptr);
//    ASSERT_EQ(str, string_vector[i]);
//    content_ptr += (str.length() + 1);
//  }
//}
//
//TEST(InputSerializationTests, RpcPredictionRequestsOnlyAcceptValidInputs) {
//  int int_data[] = {5, 6, 7, 8, 9};
//  std::vector<int> raw_data_vec;
//  for (int elem : int_data) {
//    raw_data_vec.push_back(elem);
//  }
//  std::shared_ptr<IntVector> int_vec =
//      std::make_shared<IntVector>(raw_data_vec);
//  std::vector<std::shared_ptr<Input>> inputs;
//  inputs.push_back(int_vec);
//
//  // Without error, we should be able to directly construct an integer-typed
//  // PredictionRequest by supplying a vector of IntVector inputs
//  ASSERT_NO_THROW(rpc::PredictionRequest(inputs, InputType::Ints));
//
//  // Without error, we should be able to add an IntVector input to
//  // an integer-typed PredictionRequest
//  rpc::PredictionRequest ints_prediction_request(InputType::Ints);
//  ASSERT_NO_THROW(ints_prediction_request.add_input(int_vec));
//
//  // We expect an invalid argument exception when we attempt to construct a
//  // double-typed PredictionRequest by supplying a vector of IntVector inputs
//  ASSERT_THROW(rpc::PredictionRequest(inputs, InputType::Doubles),
//               std::invalid_argument);
//
//  // We expect an invalid argument exception when we attempt to add
//  // an IntVector input to a double-typed PredictionRequest
//  rpc::PredictionRequest doubles_prediction_request(InputType::Doubles);
//  ASSERT_THROW(doubles_prediction_request.add_input(int_vec),
//               std::invalid_argument);
//}
//
//template <typename T>
//std::vector<std::vector<T>> get_primitive_hash_vectors() {
//  std::vector<std::vector<T>> hash_vectors;
//  std::vector<T> hash_vec_1;
//  for (uint8_t i = 0; i < 100; i++) {
//    T elem = static_cast<T>(i);
//    hash_vec_1.push_back(elem);
//  }
//  std::vector<T> hash_vec_2 = hash_vec_1;
//  std::vector<T> hash_vec_3 = hash_vec_1;
//  std::reverse(hash_vec_3.begin(), hash_vec_3.end());
//  hash_vectors.push_back(hash_vec_1);
//  hash_vectors.push_back(hash_vec_2);
//  hash_vectors.push_back(hash_vec_3);
//  return hash_vectors;
//}
//
//TEST(InputHashTests, IntVectorsHashCorrectly) {
//  // Obtains 3 vectors containing integer interpretations of unsigned bytes
//  // 0-99.
//  // The first two are identical, and the third is reversed
//  std::vector<std::vector<int>> int_hash_vecs =
//      get_primitive_hash_vectors<int>();
//  ASSERT_EQ(IntVector(int_hash_vecs[0]).hash(),
//            IntVector(int_hash_vecs[1]).hash());
//  ASSERT_NE(IntVector(int_hash_vecs[0]).hash(),
//            IntVector(int_hash_vecs[2]).hash());
//  int_hash_vecs[1].pop_back();
//  // Removing the last element of the second vector renders the first two
//  // vectors
//  // distinct, so they should have different hashes
//  ASSERT_NE(IntVector(int_hash_vecs[0]).hash(),
//            IntVector(int_hash_vecs[1]).hash());
//  int_hash_vecs[1].push_back(500);
//  // Adding the element 500, which is not present in the first vector, to the
//  // second vector
//  // leaves the first two vectors distinct, so they should have different hashes
//  ASSERT_NE(IntVector(int_hash_vecs[0]).hash(),
//            IntVector(int_hash_vecs[1]).hash());
//  std::reverse(int_hash_vecs[2].begin(), int_hash_vecs[2].end());
//  // Reversing the third vector, which was initially the reverse of the first
//  // vector,
//  // renders the first and third vectors identical, so they should have the same
//  // hash
//  ASSERT_EQ(IntVector(int_hash_vecs[0]).hash(),
//            IntVector(int_hash_vecs[2]).hash());
//}
//
//TEST(InputHashTests, FloatVectorsHashCorrectly) {
//  // Obtains 3 vectors containing float intepretations of unsigned bytes 0-99.
//  // The first two are identical, and the third is reversed
//  std::vector<std::vector<float>> float_hash_vecs =
//      get_primitive_hash_vectors<float>();
//  ASSERT_EQ(FloatVector(float_hash_vecs[0]).hash(),
//            FloatVector(float_hash_vecs[1]).hash());
//  ASSERT_NE(FloatVector(float_hash_vecs[0]).hash(),
//            FloatVector(float_hash_vecs[2]).hash());
//  float_hash_vecs[1].pop_back();
//  // Removing the last element of the second vector renders the first two
//  // vectors
//  // distinct, so they should have different hashes
//  ASSERT_NE(FloatVector(float_hash_vecs[0]).hash(),
//            FloatVector(float_hash_vecs[1]).hash());
//  float_hash_vecs[1].push_back(500);
//  // Adding the element 500.0, which is not present in the first vector, to the
//  // second vector
//  // leaves the first two vectors distinct, so they should have different hashes
//  ASSERT_NE(FloatVector(float_hash_vecs[0]).hash(),
//            FloatVector(float_hash_vecs[1]).hash());
//  std::reverse(float_hash_vecs[2].begin(), float_hash_vecs[2].end());
//  // Reversing the third vector, which was initially the reverse of the first
//  // vector,
//  // renders the first and third vectors identical, so they should have the same
//  // hash
//  ASSERT_EQ(FloatVector(float_hash_vecs[0]).hash(),
//            FloatVector(float_hash_vecs[2]).hash());
//}
//
//TEST(InputHashTests, DoubleVectorsHashCorrectly) {
//  // Obtains 3 vectors containing double intepretations of unsigned bytes 0-99.
//  // The first two are identical, and the third is reversed
//  std::vector<std::vector<double>> double_hash_vecs =
//      get_primitive_hash_vectors<double>();
//  ASSERT_EQ(DoubleVector(double_hash_vecs[0]).hash(),
//            DoubleVector(double_hash_vecs[1]).hash());
//  ASSERT_NE(DoubleVector(double_hash_vecs[0]).hash(),
//            DoubleVector(double_hash_vecs[2]).hash());
//  double_hash_vecs[1].pop_back();
//  // Removing the last element of the second vector renders the first two
//  // vectors
//  // distinct, so they should have different hashes
//  ASSERT_NE(DoubleVector(double_hash_vecs[0]).hash(),
//            DoubleVector(double_hash_vecs[1]).hash());
//  double_hash_vecs[1].push_back(500);
//  // Adding the element 500.0, which is not present in the first vector, to the
//  // second vector
//  // leaves the first two vectors distinct, so they should have different hashes
//  ASSERT_NE(DoubleVector(double_hash_vecs[0]).hash(),
//            DoubleVector(double_hash_vecs[1]).hash());
//  std::reverse(double_hash_vecs[2].begin(), double_hash_vecs[2].end());
//  // Reversing the third vector, which was initially the reverse of the first
//  // vector,
//  // renders the first and third vectors identical, so they should have the same
//  // hash
//  ASSERT_EQ(DoubleVector(double_hash_vecs[0]).hash(),
//            DoubleVector(double_hash_vecs[2]).hash());
//}
//
//TEST(InputHashTests, ByteVectorsHashCorrectly) {
//  // Obtains 3 vectors containing unsigned bytes 0-99.
//  // The first two are identical, and the third is reversed
//  std::vector<std::vector<uint8_t>> byte_hash_vecs =
//      get_primitive_hash_vectors<uint8_t>();
//  ASSERT_EQ(ByteVector(byte_hash_vecs[0]).hash(),
//            ByteVector(byte_hash_vecs[1]).hash());
//  ASSERT_NE(ByteVector(byte_hash_vecs[0]).hash(),
//            ByteVector(byte_hash_vecs[2]).hash());
//  byte_hash_vecs[1].pop_back();
//  // Removing the last element of the second vector renders the first two
//  // vectors
//  // distinct, so they should have different hashes
//  ASSERT_NE(ByteVector(byte_hash_vecs[0]).hash(),
//            ByteVector(byte_hash_vecs[1]).hash());
//  byte_hash_vecs[1].push_back(200);
//  // Adding an unsigned byte with value 200, which is not present in the first
//  // vector,
//  // to the second vector leaves the first two vectors distinct, so they should
//  // have different hashes
//  ASSERT_NE(ByteVector(byte_hash_vecs[0]).hash(),
//            ByteVector(byte_hash_vecs[1]).hash());
//  std::reverse(byte_hash_vecs[2].begin(), byte_hash_vecs[2].end());
//  // Reversing the third vector, which was initially the reverse of the first
//  // vector,
//  // renders the first and third vectors identical, so they should have the same
//  // hash
//  ASSERT_EQ(ByteVector(byte_hash_vecs[0]).hash(),
//            ByteVector(byte_hash_vecs[2]).hash());
//}
//
//TEST(InputHashTests, SerializableStringsHashCorrectly) {
//  std::string cat_string = "CAT";
//  std::string cat_string_copy = cat_string;
//  std::string tac_string = "TAC";
//
//  ASSERT_EQ(SerializableString(cat_string).hash(),
//            SerializableString(cat_string_copy).hash());
//  ASSERT_NE(SerializableString(cat_string).hash(),
//            SerializableString(tac_string).hash());
//  // The strings "CATS" and "CAT" are not equal, so they should have different
//  // hashes
//  ASSERT_NE(SerializableString(cat_string + "S").hash(),
//            SerializableString(cat_string).hash());
//  std::reverse(tac_string.rbegin(), tac_string.rend());
//  // The reverse of the string "TAC" is "CAT", so cat_string and the reverse of
//  // tac_string
//  // should have identical hashes
//  ASSERT_EQ(SerializableString(cat_string).hash(),
//            SerializableString(tac_string).hash());
//}
//
//TEST(OutputDeserializationTests, PredictionResponseDeserialization) {
//  std::string first_string("first_string");
//  std::string second_string("second_string");
//  uint32_t first_length = static_cast<uint32_t>(first_string.length());
//  uint32_t second_length = static_cast<uint32_t>(second_string.length());
//  uint32_t num_outputs = 2;
//  uint8_t* first_length_bytes = reinterpret_cast<uint8_t*>(&first_length);
//  uint8_t* second_length_bytes = reinterpret_cast<uint8_t*>(&second_length);
//  uint8_t* num_outputs_bytes = reinterpret_cast<uint8_t*>(&num_outputs);
//
//  // Initialize a buffer to accomodate both strings, in addition to
//  // metadata containing the number of outputs and the length of each
//  // string as unsigned integers
//  int metadata_length = 3 * sizeof(uint32_t);
//  std::vector<uint8_t> buf(first_length + second_length + metadata_length);
//
//  memcpy(buf.data(), num_outputs_bytes, sizeof(uint32_t));
//  memcpy(buf.data() + sizeof(uint32_t), first_length_bytes, sizeof(uint32_t));
//  memcpy(buf.data() + (2 * sizeof(uint32_t)), second_length_bytes,
//         sizeof(uint32_t));
//  memcpy(buf.data() + metadata_length, first_string.data(),
//         first_string.length());
//  memcpy(buf.data() + metadata_length + first_string.length(),
//         second_string.data(), second_string.length());
//
//  rpc::PredictionResponse response =
//      rpc::PredictionResponse::deserialize_prediction_response(buf);
//  ASSERT_EQ(response.outputs_.size(), static_cast<size_t>(2));
//  ASSERT_EQ(response.outputs_[0], first_string);
//  ASSERT_EQ(response.outputs_[1], second_string);
//}

}  // namespace
