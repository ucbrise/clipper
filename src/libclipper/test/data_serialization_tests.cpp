#include <stdexcept>
#include <type_traits>

#include <gtest/gtest.h>
#include <clipper/datatypes.hpp>
#include <clipper/memory.hpp>

using namespace clipper;
using std::vector;

namespace {

const unsigned long SERIALIZED_REQUEST_HEADERS_SIZE = 3;
const int NUM_PRIMITIVE_INPUTS = 500;
const int NUM_STRING_INPUTS = 1;
const uint8_t PRIMITIVE_INPUT_SIZE_ELEMS = 200;

/**
 * @return A pair consisting of a shared pointer to the primitive
 * data and the number of data elements
 */
template <typename T>
std::vector<std::pair<SharedPoolPtr<T>, size_t>> get_primitive_data_items() {
  std::vector<std::pair<SharedPoolPtr<T>, size_t>> data_items;
  for (int i = 0; i < NUM_PRIMITIVE_INPUTS; i++) {
    SharedPoolPtr<T> data_item =
        memory::allocate_shared<T>(PRIMITIVE_INPUT_SIZE_ELEMS);
    T* data_item_raw = data_item.get();
    for (uint8_t j = 0; j < PRIMITIVE_INPUT_SIZE_ELEMS; j++) {
      // Differentiate vectors by populating them with the index j under
      // differing moduli
      int k = static_cast<int>(j) % (i + 1);
      data_item_raw[j] = k;
    }
    data_items.push_back(
        std::make_pair(std::move(data_item), PRIMITIVE_INPUT_SIZE_ELEMS));
  }
  return data_items;
}

std::vector<std::string> get_string_data() {
  std::vector<std::string> string_data;
  for (int i = 0; i < NUM_STRING_INPUTS; i++) {
    std::string str;
    switch (i % 3) {
      case 0: str = std::string("CAT"); break;
      case 1: str = std::string("DOG"); break;
      case 2: str = std::string("COW"); break;
      default: str = std::string("INVALID"); break;
    }
    string_data.push_back(std::move(str));
  }
  return string_data;
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
    std::shared_ptr<ByteVector> byte_vec = std::make_shared<ByteVector>(
        data_items[i].first, 0, data_items[i].second);
    request.add_input(byte_vec);
  }
  std::vector<clipper::ByteBuffer> serialized_request = request.serialize();
  ASSERT_EQ(serialized_request.size(),
            SERIALIZED_REQUEST_HEADERS_SIZE + data_items.size());

  clipper::ByteBuffer& request_header_buf = serialized_request[0];
  clipper::ByteBuffer& input_header_size_buf = serialized_request[1];
  clipper::ByteBuffer& input_header_buf = serialized_request[2];

  uint32_t* request_header_data =
      static_cast<uint32_t*>(std::get<0>(request_header_buf).get());
  size_t request_header_start = std::get<1>(request_header_buf);

  uint64_t* input_header_size_data =
      static_cast<uint64_t*>(std::get<0>(input_header_size_buf).get());
  size_t input_header_size_start = std::get<1>(input_header_size_buf);

  uint64_t* input_header_data =
      static_cast<uint64_t*>(std::get<0>(input_header_buf).get());
  size_t input_header_start = std::get<1>(input_header_buf);
  size_t input_header_size = std::get<2>(input_header_buf);

  uint64_t parsed_input_header_size =
      input_header_size_data[input_header_size_start / sizeof(uint64_t)];
  ASSERT_EQ(parsed_input_header_size, input_header_size);

  uint32_t raw_request_type =
      request_header_data[request_header_start / sizeof(uint32_t)];
  ASSERT_EQ(raw_request_type,
            static_cast<uint32_t>(clipper::RequestType::PredictRequest));

  input_header_data += input_header_start / sizeof(uint64_t);
  uint64_t raw_input_type = input_header_data[0];
  ASSERT_EQ(raw_input_type, static_cast<uint64_t>(clipper::DataType::Bytes));
  uint64_t num_inputs = input_header_data[1];
  ASSERT_EQ(num_inputs, data_items.size());
  ASSERT_EQ(num_inputs, (input_header_size / sizeof(uint64_t)) - 2);

  for (uint64_t i = 0; i < num_inputs; ++i) {
    clipper::ByteBuffer& serialized_input = serialized_request[i + 3];
    size_t serialized_input_start_byte = std::get<1>(serialized_input);
    size_t serialized_input_size_bytes = std::get<2>(serialized_input);
    uint8_t* input_data = data_items[i].first.get();
    size_t input_size_bytes = data_items[i].second * sizeof(uint8_t);
    size_t parsed_input_size_bytes = input_header_data[i + 2];
    ASSERT_EQ(serialized_input_size_bytes, input_size_bytes);
    ASSERT_EQ(parsed_input_size_bytes, input_size_bytes);
    uint8_t* serialized_input_data =
        static_cast<uint8_t*>(std::get<0>(serialized_input).get());
    serialized_input_data += (serialized_input_start_byte / sizeof(uint8_t));
    for (size_t j = 0; j < input_size_bytes / sizeof(uint8_t); ++j) {
      ASSERT_EQ(serialized_input_data[j], input_data[j]);
    }
  }
}

TEST(InputSerializationTests, IntSerialization) {
  clipper::rpc::PredictionRequest request(InputType::Ints);
  auto data_items = get_primitive_data_items<int>();
  for (size_t i = 0; i < data_items.size(); i++) {
    std::shared_ptr<IntVector> int_vec = std::make_shared<IntVector>(
        data_items[i].first, 0, data_items[i].second);
    request.add_input(int_vec);
  }
  std::vector<clipper::ByteBuffer> serialized_request = request.serialize();
  ASSERT_EQ(serialized_request.size(),
            SERIALIZED_REQUEST_HEADERS_SIZE + data_items.size());

  clipper::ByteBuffer& request_header_buf = serialized_request[0];
  clipper::ByteBuffer& input_header_size_buf = serialized_request[1];
  clipper::ByteBuffer& input_header_buf = serialized_request[2];

  uint32_t* request_header_data =
      static_cast<uint32_t*>(std::get<0>(request_header_buf).get());
  size_t request_header_start = std::get<1>(request_header_buf);

  uint64_t* input_header_size_data =
      static_cast<uint64_t*>(std::get<0>(input_header_size_buf).get());
  size_t input_header_size_start = std::get<1>(input_header_size_buf);

  uint64_t* input_header_data =
      static_cast<uint64_t*>(std::get<0>(input_header_buf).get());
  size_t input_header_start = std::get<1>(input_header_buf);
  size_t input_header_size = std::get<2>(input_header_buf);

  uint64_t parsed_input_header_size =
      input_header_size_data[input_header_size_start / sizeof(uint64_t)];
  ASSERT_EQ(parsed_input_header_size, input_header_size);

  uint32_t raw_request_type =
      request_header_data[request_header_start / sizeof(uint32_t)];
  ASSERT_EQ(raw_request_type,
            static_cast<uint32_t>(clipper::RequestType::PredictRequest));

  input_header_data += input_header_start / sizeof(uint64_t);
  uint64_t raw_input_type = input_header_data[0];
  ASSERT_EQ(raw_input_type, static_cast<uint64_t>(clipper::DataType::Ints));
  uint64_t num_inputs = input_header_data[1];
  ASSERT_EQ(num_inputs, data_items.size());
  ASSERT_EQ(num_inputs, (input_header_size / sizeof(uint64_t)) - 2);

  for (uint64_t i = 0; i < num_inputs; ++i) {
    clipper::ByteBuffer& serialized_input = serialized_request[i + 3];
    size_t serialized_input_start_byte = std::get<1>(serialized_input);
    size_t serialized_input_size_bytes = std::get<2>(serialized_input);
    int* input_data = data_items[i].first.get();
    size_t input_size_bytes = data_items[i].second * sizeof(int);
    size_t parsed_input_size_bytes = input_header_data[i + 2];
    ASSERT_EQ(serialized_input_size_bytes, input_size_bytes);
    ASSERT_EQ(parsed_input_size_bytes, input_size_bytes);
    int* serialized_input_data =
        static_cast<int*>(std::get<0>(serialized_input).get());
    serialized_input_data += (serialized_input_start_byte / sizeof(int));
    for (size_t j = 0; j < input_size_bytes / sizeof(int); ++j) {
      ASSERT_EQ(serialized_input_data[j], input_data[j]);
    }
  }
}

TEST(InputSerializationTests, FloatSerialization) {
  clipper::rpc::PredictionRequest request(InputType::Floats);
  auto data_items = get_primitive_data_items<float>();
  for (size_t i = 0; i < data_items.size(); i++) {
    std::shared_ptr<FloatVector> float_vec = std::make_shared<FloatVector>(
        data_items[i].first, 0, data_items[i].second);
    request.add_input(float_vec);
  }
  std::vector<clipper::ByteBuffer> serialized_request = request.serialize();
  ASSERT_EQ(serialized_request.size(),
            SERIALIZED_REQUEST_HEADERS_SIZE + data_items.size());

  clipper::ByteBuffer& request_header_buf = serialized_request[0];
  clipper::ByteBuffer& input_header_size_buf = serialized_request[1];
  clipper::ByteBuffer& input_header_buf = serialized_request[2];

  uint32_t* request_header_data =
      static_cast<uint32_t*>(std::get<0>(request_header_buf).get());
  size_t request_header_start = std::get<1>(request_header_buf);

  uint64_t* input_header_size_data =
      static_cast<uint64_t*>(std::get<0>(input_header_size_buf).get());
  size_t input_header_size_start = std::get<1>(input_header_size_buf);

  uint64_t* input_header_data =
      static_cast<uint64_t*>(std::get<0>(input_header_buf).get());
  size_t input_header_start = std::get<1>(input_header_buf);
  size_t input_header_size = std::get<2>(input_header_buf);

  uint64_t parsed_input_header_size =
      input_header_size_data[input_header_size_start / sizeof(uint64_t)];
  ASSERT_EQ(parsed_input_header_size, input_header_size);

  uint32_t raw_request_type =
      request_header_data[request_header_start / sizeof(uint32_t)];
  ASSERT_EQ(raw_request_type,
            static_cast<uint32_t>(clipper::RequestType::PredictRequest));

  input_header_data += input_header_start / sizeof(uint64_t);
  uint64_t raw_input_type = input_header_data[0];
  ASSERT_EQ(raw_input_type, static_cast<uint64_t>(clipper::DataType::Floats));
  uint64_t num_inputs = input_header_data[1];
  ASSERT_EQ(num_inputs, data_items.size());
  ASSERT_EQ(num_inputs, (input_header_size / sizeof(uint64_t)) - 2);

  for (uint64_t i = 0; i < num_inputs; ++i) {
    clipper::ByteBuffer& serialized_input = serialized_request[i + 3];
    size_t serialized_input_start_byte = std::get<1>(serialized_input);
    size_t serialized_input_size_bytes = std::get<2>(serialized_input);
    float* input_data = data_items[i].first.get();
    size_t input_size_bytes = data_items[i].second * sizeof(float);
    size_t parsed_input_size_bytes = input_header_data[i + 2];
    ASSERT_EQ(serialized_input_size_bytes, input_size_bytes);
    ASSERT_EQ(parsed_input_size_bytes, input_size_bytes);
    float* serialized_input_data =
        static_cast<float*>(std::get<0>(serialized_input).get());
    serialized_input_data += (serialized_input_start_byte / sizeof(float));
    for (size_t j = 0; j < input_size_bytes / sizeof(float); ++j) {
      ASSERT_EQ(serialized_input_data[j], input_data[j]);
    }
  }
}

TEST(InputSerializationTests, DoubleSerialization) {
  clipper::rpc::PredictionRequest request(InputType::Doubles);
  auto data_items = get_primitive_data_items<double>();
  for (size_t i = 0; i < data_items.size(); i++) {
    std::shared_ptr<DoubleVector> double_vec = std::make_shared<DoubleVector>(
        data_items[i].first, 0, data_items[i].second);
    request.add_input(double_vec);
  }
  std::vector<clipper::ByteBuffer> serialized_request = request.serialize();
  ASSERT_EQ(serialized_request.size(),
            SERIALIZED_REQUEST_HEADERS_SIZE + data_items.size());

  clipper::ByteBuffer& request_header_buf = serialized_request[0];
  clipper::ByteBuffer& input_header_size_buf = serialized_request[1];
  clipper::ByteBuffer& input_header_buf = serialized_request[2];

  uint32_t* request_header_data =
      static_cast<uint32_t*>(std::get<0>(request_header_buf).get());
  size_t request_header_start = std::get<1>(request_header_buf);

  uint64_t* input_header_size_data =
      static_cast<uint64_t*>(std::get<0>(input_header_size_buf).get());
  size_t input_header_size_start = std::get<1>(input_header_size_buf);

  uint64_t* input_header_data =
      static_cast<uint64_t*>(std::get<0>(input_header_buf).get());
  size_t input_header_start = std::get<1>(input_header_buf);
  size_t input_header_size = std::get<2>(input_header_buf);

  uint64_t parsed_input_header_size =
      input_header_size_data[input_header_size_start / sizeof(uint64_t)];
  ASSERT_EQ(parsed_input_header_size, input_header_size);

  uint32_t raw_request_type =
      request_header_data[request_header_start / sizeof(uint32_t)];
  ASSERT_EQ(raw_request_type,
            static_cast<uint32_t>(clipper::RequestType::PredictRequest));

  input_header_data += input_header_start / sizeof(uint64_t);
  uint64_t raw_input_type = input_header_data[0];
  ASSERT_EQ(raw_input_type, static_cast<uint64_t>(clipper::DataType::Doubles));
  uint64_t num_inputs = input_header_data[1];
  ASSERT_EQ(num_inputs, data_items.size());
  ASSERT_EQ(num_inputs, (input_header_size / sizeof(uint64_t)) - 2);

  for (uint64_t i = 0; i < num_inputs; ++i) {
    clipper::ByteBuffer& serialized_input = serialized_request[i + 3];
    size_t serialized_input_start_byte = std::get<1>(serialized_input);
    size_t serialized_input_size_bytes = std::get<2>(serialized_input);
    double* input_data = data_items[i].first.get();
    size_t input_size_bytes = data_items[i].second * sizeof(double);
    size_t parsed_input_size_bytes = input_header_data[i + 2];
    ASSERT_EQ(serialized_input_size_bytes, input_size_bytes);
    ASSERT_EQ(parsed_input_size_bytes, input_size_bytes);
    double* serialized_input_data =
        static_cast<double*>(std::get<0>(serialized_input).get());
    serialized_input_data += (serialized_input_start_byte / sizeof(double));
    for (size_t j = 0; j < input_size_bytes / sizeof(double); ++j) {
      ASSERT_EQ(serialized_input_data[j], input_data[j]);
    }
  }
}

TEST(InputSerializationTests, StringSerialization) {
  clipper::rpc::PredictionRequest request(InputType::Strings);
  std::vector<std::string> data_items = get_string_data();
  for (size_t i = 0; i < data_items.size(); ++i) {
    std::unique_ptr<PredictionData> serialized_item =
        to_serializable_string(data_items[i]);
    request.add_input(std::move(serialized_item));
  }

  std::vector<clipper::ByteBuffer> serialized_request = request.serialize();
  ASSERT_EQ(serialized_request.size(),
            SERIALIZED_REQUEST_HEADERS_SIZE + data_items.size());

  clipper::ByteBuffer& request_header_buf = serialized_request[0];
  clipper::ByteBuffer& input_header_size_buf = serialized_request[1];
  clipper::ByteBuffer& input_header_buf = serialized_request[2];

  uint32_t* request_header_data =
      static_cast<uint32_t*>(std::get<0>(request_header_buf).get());
  size_t request_header_start = std::get<1>(request_header_buf);

  uint64_t* input_header_size_data =
      static_cast<uint64_t*>(std::get<0>(input_header_size_buf).get());
  size_t input_header_size_start = std::get<1>(input_header_size_buf);

  uint64_t* input_header_data =
      static_cast<uint64_t*>(std::get<0>(input_header_buf).get());
  size_t input_header_start = std::get<1>(input_header_buf);
  size_t input_header_size = std::get<2>(input_header_buf);

  uint64_t parsed_input_header_size =
      input_header_size_data[input_header_size_start / sizeof(uint64_t)];
  ASSERT_EQ(parsed_input_header_size, input_header_size);

  uint32_t raw_request_type =
      request_header_data[request_header_start / sizeof(uint32_t)];
  ASSERT_EQ(raw_request_type,
            static_cast<uint32_t>(clipper::RequestType::PredictRequest));

  input_header_data += input_header_start / sizeof(uint64_t);
  uint64_t raw_input_type = input_header_data[0];
  ASSERT_EQ(raw_input_type, static_cast<uint64_t>(clipper::DataType::Strings));
  uint64_t num_inputs = input_header_data[1];
  ASSERT_EQ(num_inputs, data_items.size());
  ASSERT_EQ(num_inputs, (input_header_size / sizeof(uint64_t)) - 2);

  for (uint64_t i = 0; i < num_inputs; ++i) {
    clipper::ByteBuffer& serialized_input = serialized_request[i + 3];
    size_t serialized_input_start_byte = std::get<1>(serialized_input);
    size_t serialized_input_size_bytes = std::get<2>(serialized_input);
    std::string input_data = data_items[i];
    size_t input_size_bytes = data_items[i].size();
    size_t parsed_input_size_bytes = input_header_data[i + 2];
    ASSERT_EQ(serialized_input_size_bytes, input_size_bytes);
    ASSERT_EQ(parsed_input_size_bytes, input_size_bytes);
    char* serialized_input_data =
        static_cast<char*>(std::get<0>(serialized_input).get());
    serialized_input_data += (serialized_input_start_byte / sizeof(char));

    for (size_t j = 0; j < input_size_bytes / sizeof(char); ++j) {
      ASSERT_EQ(serialized_input_data[j], input_data[j]);
    }
  }
}

TEST(InputSerializationTests, RpcPredictionRequestsOnlyAcceptValidInputs) {
  size_t data_size = 5;
  SharedPoolPtr<int> input_data = memory::allocate_shared<int>(5);
  for (int i = 0; i < static_cast<int>(data_size); i++) {
    input_data.get()[i] = i;
  }
  std::shared_ptr<PredictionData> int_vec =
      std::make_shared<IntVector>(input_data, 0, data_size);
  std::vector<std::shared_ptr<PredictionData>> inputs;
  inputs.push_back(int_vec);

  // Without error, we should be able to directly construct an integer-typed
  // PredictionRequest by supplying a vector of IntVector inputs
  ASSERT_NO_THROW(rpc::PredictionRequest(inputs, InputType::Ints));

  // Without error, we should be able to add an IntVector input to
  // an integer-typed PredictionRequest
  rpc::PredictionRequest ints_prediction_request(InputType::Ints);
  ASSERT_NO_THROW(ints_prediction_request.add_input(int_vec));

  // We expect an invalid argument exception when we attempt to construct a
  // double-typed PredictionRequest by supplying a vector of IntVector inputs
  ASSERT_THROW(rpc::PredictionRequest(inputs, InputType::Doubles),
               std::invalid_argument);

  // We expect an invalid argument exception when we attempt to add
  // an IntVector input to a double-typed PredictionRequest
  rpc::PredictionRequest doubles_prediction_request(InputType::Doubles);
  ASSERT_THROW(doubles_prediction_request.add_input(int_vec),
               std::invalid_argument);
}

template <typename T>
std::vector<std::pair<SharedPoolPtr<T>, size_t>> get_primitive_hash_items() {
  size_t hash_item_size = 100;
  std::vector<std::pair<SharedPoolPtr<T>, size_t>> hash_items;
  SharedPoolPtr<T> hash_item_1 = memory::allocate_shared<T>(hash_item_size);
  SharedPoolPtr<T> hash_item_3 = memory::allocate_shared<T>(hash_item_size);
  for (uint8_t i = 0; i < static_cast<uint8_t>(hash_item_size); i++) {
    T elem = static_cast<T>(i);
    hash_item_1.get()[static_cast<size_t>(i)] = elem;
    hash_item_3.get()[static_cast<size_t>(i)] = hash_item_size - elem - 1;
  }
  SharedPoolPtr<T> hash_item_2 = memory::allocate_shared<T>(hash_item_size);
  memcpy(hash_item_2.get(), hash_item_1.get(), hash_item_size * sizeof(T));
  hash_items.push_back(std::make_pair(std::move(hash_item_1), hash_item_size));
  hash_items.push_back(std::make_pair(std::move(hash_item_2), hash_item_size));
  hash_items.push_back(std::make_pair(std::move(hash_item_3), hash_item_size));
  return hash_items;
}

TEST(InputHashTests, IntVectorsHashCorrectly) {
  // Obtains 3 data items containing integer interpretations of unsigned bytes
  // 0-99.
  // The first two are identical, and the third is reversed
  std::vector<std::pair<SharedPoolPtr<int>, size_t>> int_hash_items =
      get_primitive_hash_items<int>();
  SharedPoolPtr<int> first_item_data;
  SharedPoolPtr<int> second_item_data;
  SharedPoolPtr<int> third_item_data;
  size_t first_item_size;
  size_t second_item_size;
  size_t third_item_size;
  std::tie(first_item_data, first_item_size) = int_hash_items[0];
  std::tie(second_item_data, second_item_size) = int_hash_items[1];
  std::tie(third_item_data, third_item_size) = int_hash_items[2];

  ASSERT_EQ(IntVector(first_item_data, 0, first_item_size).hash(),
            IntVector(second_item_data, 0, second_item_size).hash());
  ASSERT_NE(IntVector(first_item_data, 0, first_item_size).hash(),
            IntVector(third_item_data, 0, third_item_size).hash());
  // Disregarding the last element of the second item renders the first two
  // items
  // distinct, so they should have different hashes
  ASSERT_NE(IntVector(first_item_data, 0, first_item_size - 1).hash(),
            IntVector(second_item_data, 0, second_item_size).hash());
  size_t new_item_size = second_item_size + 1;
  SharedPoolPtr<int> new_item_data =
      memory::allocate_shared<int>(new_item_size);
  memcpy(new_item_data.get(), second_item_data.get(), second_item_size);
  new_item_data.get()[new_item_size - 1] = 500;
  // Adding the element 500, which is not present in the first item, to the
  // second item
  // leaves the first two items distinct, so they should have different hashes
  ASSERT_NE(IntVector(first_item_data, 0, first_item_size).hash(),
            IntVector(new_item_data, 0, new_item_size).hash());
  // Reversing the third vector, which was initially the reverse of the first
  // vector,
  // renders the first and third vectors identical, so they should have the same
  // hash
  ASSERT_EQ(third_item_size % 2, 0UL);
  for (size_t i = 0; i < (third_item_size / 2); i++) {
    int tmp = third_item_data.get()[i];
    third_item_data.get()[i] = third_item_data.get()[third_item_size - i - 1];
    third_item_data.get()[third_item_size - i - 1] = tmp;
  }
  ASSERT_EQ(IntVector(first_item_data, 0, first_item_size).hash(),
            IntVector(third_item_data, 0, third_item_size).hash());
}

TEST(InputHashTests, FloatVectorsHashCorrectly) {
  // Obtains 3 data items containing float interpretations of unsigned bytes
  // 0-99.
  // The first two are identical, and the third is reversed
  std::vector<std::pair<SharedPoolPtr<float>, size_t>> float_hash_items =
      get_primitive_hash_items<float>();
  SharedPoolPtr<float> first_item_data;
  SharedPoolPtr<float> second_item_data;
  SharedPoolPtr<float> third_item_data;
  size_t first_item_size;
  size_t second_item_size;
  size_t third_item_size;
  std::tie(first_item_data, first_item_size) = float_hash_items[0];
  std::tie(second_item_data, second_item_size) = float_hash_items[1];
  std::tie(third_item_data, third_item_size) = float_hash_items[2];

  ASSERT_EQ(FloatVector(first_item_data, 0, first_item_size).hash(),
            FloatVector(second_item_data, 0, second_item_size).hash());
  ASSERT_NE(FloatVector(first_item_data, 0, first_item_size).hash(),
            FloatVector(third_item_data, 0, third_item_size).hash());
  // Disregarding the last element of the second item renders the first two
  // items
  // distinct, so they should have different hashes
  ASSERT_NE(FloatVector(first_item_data, 0, first_item_size - 1).hash(),
            FloatVector(second_item_data, 0, second_item_size).hash());
  size_t new_item_size = second_item_size + 1;
  SharedPoolPtr<float> new_item_data =
      memory::allocate_shared<float>(new_item_size);
  memcpy(new_item_data.get(), second_item_data.get(), second_item_size);
  new_item_data.get()[new_item_size - 1] = 500;
  // Adding the element 500, which is not present in the first item, to the
  // second item
  // leaves the first two items distinct, so they should have different hashes
  ASSERT_NE(FloatVector(first_item_data, 0, first_item_size).hash(),
            FloatVector(new_item_data, 0, new_item_size).hash());
  // Reversing the third vector, which was initially the reverse of the first
  // vector,
  // renders the first and third vectors identical, so they should have the same
  // hash
  ASSERT_EQ(third_item_size % 2, 0UL);
  for (size_t i = 0; i < (third_item_size / 2); i++) {
    float tmp = third_item_data.get()[i];
    third_item_data.get()[i] = third_item_data.get()[third_item_size - i - 1];
    third_item_data.get()[third_item_size - i - 1] = tmp;
  }
  ASSERT_EQ(FloatVector(first_item_data, 0, first_item_size).hash(),
            FloatVector(third_item_data, 0, third_item_size).hash());
}

TEST(InputHashTests, DoubleVectorsHashCorrectly) {
  // Obtains 3 data items containing double interpretations of unsigned bytes
  // 0-99.
  // The first two are identical, and the third is reversed
  std::vector<std::pair<SharedPoolPtr<double>, size_t>> double_hash_items =
      get_primitive_hash_items<double>();
  SharedPoolPtr<double> first_item_data;
  SharedPoolPtr<double> second_item_data;
  SharedPoolPtr<double> third_item_data;
  size_t first_item_size;
  size_t second_item_size;
  size_t third_item_size;
  std::tie(first_item_data, first_item_size) = double_hash_items[0];
  std::tie(second_item_data, second_item_size) = double_hash_items[1];
  std::tie(third_item_data, third_item_size) = double_hash_items[2];

  ASSERT_EQ(DoubleVector(first_item_data, 0, first_item_size).hash(),
            DoubleVector(second_item_data, 0, second_item_size).hash());
  ASSERT_NE(DoubleVector(first_item_data, 0, first_item_size).hash(),
            DoubleVector(third_item_data, 0, third_item_size).hash());
  // Disregarding the last element of the second item renders the first two
  // items
  // distinct, so they should have different hashes
  ASSERT_NE(DoubleVector(first_item_data, 0, first_item_size - 1).hash(),
            DoubleVector(second_item_data, 0, second_item_size).hash());
  size_t new_item_size = second_item_size + 1;
  SharedPoolPtr<double> new_item_data =
      memory::allocate_shared<double>(new_item_size);
  memcpy(new_item_data.get(), second_item_data.get(), second_item_size);
  new_item_data.get()[new_item_size - 1] = 500;
  // Adding the element 500, which is not present in the first item, to the
  // second item
  // leaves the first two items distinct, so they should have different hashes
  ASSERT_NE(DoubleVector(first_item_data, 0, first_item_size).hash(),
            DoubleVector(new_item_data, 0, new_item_size).hash());
  // Reversing the third vector, which was initially the reverse of the first
  // vector,
  // renders the first and third vectors identical, so they should have the same
  // hash
  ASSERT_EQ(third_item_size % 2, 0UL);
  for (size_t i = 0; i < (third_item_size / 2); i++) {
    double tmp = third_item_data.get()[i];
    third_item_data.get()[i] = third_item_data.get()[third_item_size - i - 1];
    third_item_data.get()[third_item_size - i - 1] = tmp;
  }
  ASSERT_EQ(DoubleVector(first_item_data, 0, first_item_size).hash(),
            DoubleVector(third_item_data, 0, third_item_size).hash());
}

TEST(InputHashTests, ByteVectorsHashCorrectly) {
  // Obtains 3 data items containing unsigned bytes 0-99.
  // The first two are identical, and the third is reversed
  std::vector<std::pair<SharedPoolPtr<uint8_t>, size_t>> byte_hash_items =
      get_primitive_hash_items<uint8_t>();
  SharedPoolPtr<uint8_t> first_item_data;
  SharedPoolPtr<uint8_t> second_item_data;
  SharedPoolPtr<uint8_t> third_item_data;
  size_t first_item_size;
  size_t second_item_size;
  size_t third_item_size;
  std::tie(first_item_data, first_item_size) = byte_hash_items[0];
  std::tie(second_item_data, second_item_size) = byte_hash_items[1];
  std::tie(third_item_data, third_item_size) = byte_hash_items[2];

  ASSERT_EQ(ByteVector(first_item_data, 0, first_item_size).hash(),
            ByteVector(second_item_data, 0, second_item_size).hash());
  ASSERT_NE(ByteVector(first_item_data, 0, first_item_size).hash(),
            ByteVector(third_item_data, 0, third_item_size).hash());
  // Disregarding the last element of the second item renders the first two
  // items
  // distinct, so they should have different hashes
  ASSERT_NE(ByteVector(first_item_data, 0, first_item_size - 1).hash(),
            ByteVector(second_item_data, 0, second_item_size).hash());
  size_t new_item_size = second_item_size + 1;
  SharedPoolPtr<uint8_t> new_item_data =
      memory::allocate_shared<uint8_t>(new_item_size);
  memcpy(new_item_data.get(), second_item_data.get(), second_item_size);
  new_item_data.get()[new_item_size - 1] = 200;
  // Adding the element 200, which is not present in the first item, to the
  // second item
  // leaves the first two items distinct, so they should have different hashes
  ASSERT_NE(ByteVector(first_item_data, 0, first_item_size).hash(),
            ByteVector(new_item_data, 0, new_item_size).hash());
  // Reversing the third vector, which was initially the reverse of the first
  // vector,
  // renders the first and third vectors identical, so they should have the same
  // hash
  ASSERT_EQ(third_item_size % 2, 0UL);
  for (size_t i = 0; i < (third_item_size / 2); i++) {
    uint8_t tmp = third_item_data.get()[i];
    third_item_data.get()[i] = third_item_data.get()[third_item_size - i - 1];
    third_item_data.get()[third_item_size - i - 1] = tmp;
  }
  ASSERT_EQ(ByteVector(first_item_data, 0, first_item_size).hash(),
            ByteVector(third_item_data, 0, third_item_size).hash());
}

TEST(InputHashTests, SerializableStringsHashCorrectly) {
  std::string cat_string = "CAT";
  std::string cat_string_copy = cat_string;
  std::string tac_string = "TAC";

  ASSERT_EQ(to_serializable_string(cat_string)->hash(),
            to_serializable_string(cat_string_copy)->hash());
  ASSERT_NE(to_serializable_string(cat_string)->hash(),
            to_serializable_string(tac_string)->hash());
  // The strings "CATS" and "CAT" are not equal, so they should have different
  // hashes
  ASSERT_NE(to_serializable_string(cat_string + "S")->hash(),
            to_serializable_string(cat_string)->hash());
  std::reverse(tac_string.rbegin(), tac_string.rend());
  // The reverse of the string "TAC" is "CAT", so cat_string and the reverse of
  // tac_string
  // should have identical hashes
  ASSERT_EQ(to_serializable_string(cat_string)->hash(),
            to_serializable_string(tac_string)->hash());
}

TEST(OutputDeserializationTests, PredictionResponseDeserialization) {
  std::string output("OUTPUT");
  size_t output_size_bytes = output.size() * sizeof(char);
  SharedPoolPtr<void> output_data(malloc(output_size_bytes), free);
  memcpy(output_data.get(), output.data(), output_size_bytes);
  std::vector<ByteBuffer> output_bufs;
  output_bufs.push_back(std::make_tuple(output_data, 0, output_size_bytes));

  rpc::PredictionResponse response =
      rpc::PredictionResponse::deserialize_prediction_response(output_bufs);

  auto& response_outputs = response.outputs_;
  ASSERT_EQ(response_outputs.size(), 1UL);
  auto& response_output = response_outputs[0];
  ASSERT_EQ(response_output->type(), DataType::Strings);
  ASSERT_EQ(response_output->size(), output.size());
  SharedPoolPtr<char> response_output_data = get_data<char>(response_output);
  std::string parsed_response_output(
      response_output_data.get() + response_output->start(),
      response_output_data.get() + response_output->start() +
          response_output->size());
  ASSERT_EQ(output, parsed_response_output);
}

}  // namespace
