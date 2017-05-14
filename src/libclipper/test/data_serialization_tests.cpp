#include <stdexcept>
#include <type_traits>

#include <gtest/gtest.h>
#include <clipper/datatypes.hpp>

using namespace clipper;
using std::vector;

namespace {

const unsigned long SERIALIZED_REQUEST_SIZE = 5;
const int NUM_PRIMITIVE_INPUTS = 500;
const int NUM_STRING_INPUTS = 300;
const uint8_t PRIMITIVE_INPUT_SIZE_ELEMS = 200;

template <typename T>
std::vector<std::vector<T>> get_primitive_data_vectors() {
  std::vector<std::vector<T>> data_vectors;
  for (int i = 0; i < NUM_PRIMITIVE_INPUTS; i++) {
    std::vector<T> data_vector;
    for (uint8_t j = 0; j < PRIMITIVE_INPUT_SIZE_ELEMS; j++) {
      // Differentiate vectors by populating them with the index j under
      // differing moduli
      int k = static_cast<int>(j) % (i + 1);
      data_vector.push_back(static_cast<T>(k));
    }
    data_vectors.push_back(data_vector);
  }
  return data_vectors;
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
  std::vector<std::vector<uint8_t>> data_vectors =
      get_primitive_data_vectors<uint8_t>();
  for (int i = 0; i < (int)data_vectors.size(); i++) {
    std::shared_ptr<ByteVector> byte_vec =
        std::make_shared<ByteVector>(data_vectors[i]);
    request.add_input(byte_vec);
  }
  std::vector<clipper::ByteBuffer> serialized_request = request.serialize();
  ASSERT_EQ(serialized_request.size(), SERIALIZED_REQUEST_SIZE);
  clipper::ByteBuffer request_header = serialized_request[0];
  clipper::ByteBuffer input_header_size = serialized_request[1];
  clipper::ByteBuffer input_header = serialized_request[2];
  clipper::ByteBuffer input_content_size = serialized_request[3];
  clipper::ByteBuffer input_content = serialized_request[4];

  long* raw_input_header_size =
      reinterpret_cast<long*>(input_header_size.data());
  ASSERT_EQ((size_t)raw_input_header_size[0], input_header.size());
  long* raw_input_content_size =
      reinterpret_cast<long*>(input_content_size.data());
  ASSERT_EQ((size_t)raw_input_content_size[0], input_content.size());

  uint32_t* raw_request_type =
      reinterpret_cast<uint32_t*>(request_header.data());
  ASSERT_EQ(*raw_request_type,
            static_cast<uint32_t>(clipper::RequestType::PredictRequest));
  uint32_t* typed_input_header =
      reinterpret_cast<uint32_t*>(input_header.data());
  ASSERT_EQ(*typed_input_header,
            static_cast<uint32_t>(clipper::InputType::Bytes));
  typed_input_header++;
  uint32_t num_inputs = *typed_input_header;
  ASSERT_EQ(num_inputs, static_cast<uint32_t>(data_vectors.size()));
  typed_input_header++;

  uint8_t* content_ptr = input_content.data();
  uint32_t prev_index = 0;
  for (int i = 0; i < (int)num_inputs - 1; i++) {
    uint32_t split_index = typed_input_header[i];
    std::vector<uint8_t> vec(content_ptr + prev_index,
                             content_ptr + split_index);
    ASSERT_EQ(vec, data_vectors[i]);
    prev_index = split_index;
  }
  // Our splits define internal indices at which to delimit input vectors, so
  // they exclude 0
  // and the content length. We therefore have to check the consistency of the
  // final vector.
  // This vector is composed of the elements from the last split index through
  // the total content length.
  std::vector<uint8_t> tail_vec(
      content_ptr + prev_index,
      content_ptr + (input_content.size() / sizeof(uint8_t)));
  ASSERT_EQ(tail_vec, data_vectors[data_vectors.size() - 1]);
}

TEST(InputSerializationTests, IntSerialization) {
  clipper::rpc::PredictionRequest request(InputType::Ints);
  std::vector<std::vector<int>> data_vectors =
      get_primitive_data_vectors<int>();
  for (int i = 0; i < (int)data_vectors.size(); i++) {
    std::shared_ptr<IntVector> int_vec =
        std::make_shared<IntVector>(data_vectors[i]);
    request.add_input(int_vec);
  }
  std::vector<clipper::ByteBuffer> serialized_request = request.serialize();
  ASSERT_EQ(serialized_request.size(), SERIALIZED_REQUEST_SIZE);

  clipper::ByteBuffer request_header = serialized_request[0];
  clipper::ByteBuffer input_header_size = serialized_request[1];
  clipper::ByteBuffer input_header = serialized_request[2];
  clipper::ByteBuffer input_content_size = serialized_request[3];
  clipper::ByteBuffer input_content = serialized_request[4];

  long* raw_input_header_size =
      reinterpret_cast<long*>(input_header_size.data());
  ASSERT_EQ(static_cast<size_t>(raw_input_header_size[0]), input_header.size());
  long* raw_input_content_size =
      reinterpret_cast<long*>(input_content_size.data());
  ASSERT_EQ(static_cast<size_t>(raw_input_content_size[0]),
            input_content.size());

  uint32_t* raw_request_type =
      reinterpret_cast<uint32_t*>(request_header.data());
  ASSERT_EQ(*raw_request_type,
            static_cast<uint32_t>(clipper::RequestType::PredictRequest));
  uint32_t* typed_input_header =
      reinterpret_cast<uint32_t*>(input_header.data());
  ASSERT_EQ(*typed_input_header,
            static_cast<uint32_t>(clipper::InputType::Ints));
  typed_input_header++;
  uint32_t num_inputs = *typed_input_header;
  ASSERT_EQ(num_inputs, static_cast<uint32_t>(data_vectors.size()));
  typed_input_header++;

  int* content_ptr = reinterpret_cast<int*>(input_content.data());
  uint32_t prev_index = 0;
  for (int i = 0; i < (int)num_inputs - 1; i++) {
    uint32_t split_index = typed_input_header[i];
    std::vector<int> vec(content_ptr + prev_index, content_ptr + split_index);
    ASSERT_EQ(vec, data_vectors[i]);
    prev_index = split_index;
  }
  // Our splits define internal indices at which to delimit input vectors, so
  // they exclude 0
  // and the content length. We therefore have to check the consistency of the
  // final vector.
  // This vector is composed of the elements from the last split index through
  // the total content length.
  std::vector<int> tail_vec(content_ptr + prev_index,
                            content_ptr + (input_content.size() / sizeof(int)));
  ASSERT_EQ(tail_vec, data_vectors[data_vectors.size() - 1]);
}

TEST(InputSerializationTests, FloatSerialization) {
  clipper::rpc::PredictionRequest request(InputType::Floats);
  std::vector<std::vector<float>> data_vectors =
      get_primitive_data_vectors<float>();
  for (int i = 0; i < (int)data_vectors.size(); i++) {
    std::shared_ptr<FloatVector> float_vec =
        std::make_shared<FloatVector>(data_vectors[i]);
    request.add_input(float_vec);
  }
  std::vector<clipper::ByteBuffer> serialized_request = request.serialize();
  ASSERT_EQ(serialized_request.size(), SERIALIZED_REQUEST_SIZE);

  clipper::ByteBuffer request_header = serialized_request[0];
  clipper::ByteBuffer input_header_size = serialized_request[1];
  clipper::ByteBuffer input_header = serialized_request[2];
  clipper::ByteBuffer input_content_size = serialized_request[3];
  clipper::ByteBuffer input_content = serialized_request[4];

  long* raw_input_header_size =
      reinterpret_cast<long*>(input_header_size.data());
  ASSERT_EQ(static_cast<size_t>(raw_input_header_size[0]), input_header.size());
  long* raw_input_content_size =
      reinterpret_cast<long*>(input_content_size.data());
  ASSERT_EQ(static_cast<size_t>(raw_input_content_size[0]),
            input_content.size());

  uint32_t* raw_request_type =
      reinterpret_cast<uint32_t*>(request_header.data());
  ASSERT_EQ(*raw_request_type,
            static_cast<uint32_t>(clipper::RequestType::PredictRequest));
  uint32_t* typed_input_header =
      reinterpret_cast<uint32_t*>(input_header.data());
  ASSERT_EQ(*typed_input_header,
            static_cast<uint32_t>(clipper::InputType::Floats));
  typed_input_header++;
  uint32_t num_inputs = *typed_input_header;
  ASSERT_EQ(num_inputs, static_cast<uint32_t>(data_vectors.size()));
  typed_input_header++;

  float* content_ptr = reinterpret_cast<float*>(input_content.data());
  uint32_t prev_index = 0;
  for (int i = 0; i < (int)num_inputs - 1; i++) {
    uint32_t split_index = typed_input_header[i];
    std::vector<float> vec(content_ptr + prev_index, content_ptr + split_index);
    ASSERT_EQ(vec, data_vectors[i]);
    prev_index = split_index;
  }
  // Our splits define internal indices at which to delimit input vectors, so
  // they exclude 0
  // and the content length. We therefore have to check the consistency of the
  // final vector.
  // This vector is composed of the elements from the last split index through
  // the total content length.
  std::vector<float> tail_vec(
      content_ptr + prev_index,
      content_ptr + (input_content.size() / sizeof(float)));
  ASSERT_EQ(tail_vec, data_vectors[data_vectors.size() - 1]);
}

TEST(InputSerializationTests, DoubleSerialization) {
  clipper::rpc::PredictionRequest request(InputType::Doubles);
  std::vector<std::vector<double>> data_vectors =
      get_primitive_data_vectors<double>();
  for (int i = 0; i < (int)data_vectors.size(); i++) {
    std::shared_ptr<DoubleVector> double_vec =
        std::make_shared<DoubleVector>(data_vectors[i]);
    request.add_input(double_vec);
  }
  std::vector<clipper::ByteBuffer> serialized_request = request.serialize();
  ASSERT_EQ(serialized_request.size(), SERIALIZED_REQUEST_SIZE);

  clipper::ByteBuffer request_header = serialized_request[0];
  clipper::ByteBuffer input_header_size = serialized_request[1];
  clipper::ByteBuffer input_header = serialized_request[2];
  clipper::ByteBuffer input_content_size = serialized_request[3];
  clipper::ByteBuffer input_content = serialized_request[4];

  long* raw_input_header_size =
      reinterpret_cast<long*>(input_header_size.data());
  ASSERT_EQ(static_cast<size_t>(raw_input_header_size[0]), input_header.size());
  long* raw_input_content_size =
      reinterpret_cast<long*>(input_content_size.data());
  ASSERT_EQ(static_cast<size_t>(raw_input_content_size[0]),
            input_content.size());

  uint32_t* raw_request_type =
      reinterpret_cast<uint32_t*>(request_header.data());
  ASSERT_EQ(*raw_request_type,
            static_cast<uint32_t>(clipper::RequestType::PredictRequest));
  uint32_t* typed_input_header =
      reinterpret_cast<uint32_t*>(input_header.data());
  ASSERT_EQ(*typed_input_header,
            static_cast<uint32_t>(clipper::InputType::Doubles));
  typed_input_header++;
  uint32_t num_inputs = *typed_input_header;
  ASSERT_EQ(num_inputs, static_cast<uint32_t>(data_vectors.size()));
  typed_input_header++;

  double* content_ptr = reinterpret_cast<double*>(input_content.data());
  uint32_t prev_index = 0;
  for (int i = 0; i < (int)num_inputs - 1; i++) {
    uint32_t split_index = typed_input_header[i];
    std::vector<double> vec(content_ptr + prev_index,
                            content_ptr + split_index);
    ASSERT_EQ(vec, data_vectors[i]);
    prev_index = split_index;
  }
  // Our splits define internal indices at which to delimit input vectors, so
  // they exclude 0
  // and the content length. We therefore have to check the consistency of the
  // final vector.
  // This vector is composed of the elements from the last split index through
  // the total content length.
  std::vector<double> tail_vec(
      content_ptr + prev_index,
      content_ptr + (input_content.size() / sizeof(double)));
  ASSERT_EQ(tail_vec, data_vectors[data_vectors.size() - 1]);
}

TEST(InputSerializationTests, StringSerialization) {
  clipper::rpc::PredictionRequest request(InputType::Strings);
  std::vector<std::string> string_vector;
  get_string_data(string_vector);
  for (int i = 0; i < (int)string_vector.size(); i++) {
    std::shared_ptr<SerializableString> serializable_str =
        std::make_shared<SerializableString>(string_vector[i]);
    request.add_input(serializable_str);
  }
  std::vector<clipper::ByteBuffer> serialized_request = request.serialize();
  ASSERT_EQ(serialized_request.size(), SERIALIZED_REQUEST_SIZE);
  clipper::ByteBuffer request_header = serialized_request[0];
  clipper::ByteBuffer input_header_size = serialized_request[1];
  clipper::ByteBuffer input_header = serialized_request[2];
  clipper::ByteBuffer input_content_size = serialized_request[3];
  clipper::ByteBuffer input_content = serialized_request[4];

  long* raw_input_header_size =
      reinterpret_cast<long*>(input_header_size.data());
  ASSERT_EQ(static_cast<size_t>(raw_input_header_size[0]), input_header.size());
  long* raw_input_content_size =
      reinterpret_cast<long*>(input_content_size.data());
  ASSERT_EQ(static_cast<size_t>(raw_input_content_size[0]),
            input_content.size());

  uint32_t* raw_request_type =
      reinterpret_cast<uint32_t*>(request_header.data());
  ASSERT_EQ(*raw_request_type,
            static_cast<uint32_t>(clipper::RequestType::PredictRequest));
  uint32_t* typed_input_header =
      reinterpret_cast<uint32_t*>(input_header.data());
  ASSERT_EQ(*typed_input_header,
            static_cast<uint32_t>(clipper::InputType::Strings));
  typed_input_header++;
  uint32_t num_inputs = *typed_input_header;
  ASSERT_EQ(num_inputs, static_cast<uint32_t>(string_vector.size()));
  typed_input_header++;

  char* content_ptr = reinterpret_cast<char*>(input_content.data());
  for (int i = 0; i < (int)num_inputs; i++) {
    std::string str(content_ptr);
    ASSERT_EQ(str, string_vector[i]);
    content_ptr += (str.length() + 1);
  }
}

TEST(InputSerializationTests, RpcPredictionRequestsOnlyAcceptValidInputs) {
  int int_data[] = {5, 6, 7, 8, 9};
  std::vector<int> raw_data_vec;
  for (int elem : int_data) {
    raw_data_vec.push_back(elem);
  }
  std::shared_ptr<IntVector> int_vec =
      std::make_shared<IntVector>(raw_data_vec);
  std::vector<std::shared_ptr<Input>> inputs;
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
std::vector<std::vector<T>> get_primitive_hash_vectors() {
  std::vector<std::vector<T>> hash_vectors;
  std::vector<T> hash_vec_1;
  for (uint8_t i = 0; i < 100; i++) {
    T elem = static_cast<T>(i);
    hash_vec_1.push_back(elem);
  }
  std::vector<T> hash_vec_2 = hash_vec_1;
  std::vector<T> hash_vec_3 = hash_vec_1;
  std::reverse(hash_vec_3.begin(), hash_vec_3.end());
  hash_vectors.push_back(hash_vec_1);
  hash_vectors.push_back(hash_vec_2);
  hash_vectors.push_back(hash_vec_3);
  return hash_vectors;
}

TEST(InputHashTests, IntVectorsHashCorrectly) {
  // Obtains 3 vectors containing integer interpretations of unsigned bytes
  // 0-99.
  // The first two are identical, and the third is reversed
  std::vector<std::vector<int>> int_hash_vecs =
      get_primitive_hash_vectors<int>();
  ASSERT_EQ(IntVector(int_hash_vecs[0]).hash(),
            IntVector(int_hash_vecs[1]).hash());
  ASSERT_NE(IntVector(int_hash_vecs[0]).hash(),
            IntVector(int_hash_vecs[2]).hash());
  int_hash_vecs[1].pop_back();
  // Removing the last element of the second vector renders the first two
  // vectors
  // distinct, so they should have different hashes
  ASSERT_NE(IntVector(int_hash_vecs[0]).hash(),
            IntVector(int_hash_vecs[1]).hash());
  int_hash_vecs[1].push_back(500);
  // Adding the element 500, which is not present in the first vector, to the
  // second vector
  // leaves the first two vectors distinct, so they should have different hashes
  ASSERT_NE(IntVector(int_hash_vecs[0]).hash(),
            IntVector(int_hash_vecs[1]).hash());
  std::reverse(int_hash_vecs[2].begin(), int_hash_vecs[2].end());
  // Reversing the third vector, which was initially the reverse of the first
  // vector,
  // renders the first and third vectors identical, so they should have the same
  // hash
  ASSERT_EQ(IntVector(int_hash_vecs[0]).hash(),
            IntVector(int_hash_vecs[2]).hash());
}

TEST(InputHashTests, FloatVectorsHashCorrectly) {
  // Obtains 3 vectors containing float intepretations of unsigned bytes 0-99.
  // The first two are identical, and the third is reversed
  std::vector<std::vector<float>> float_hash_vecs =
      get_primitive_hash_vectors<float>();
  ASSERT_EQ(FloatVector(float_hash_vecs[0]).hash(),
            FloatVector(float_hash_vecs[1]).hash());
  ASSERT_NE(FloatVector(float_hash_vecs[0]).hash(),
            FloatVector(float_hash_vecs[2]).hash());
  float_hash_vecs[1].pop_back();
  // Removing the last element of the second vector renders the first two
  // vectors
  // distinct, so they should have different hashes
  ASSERT_NE(FloatVector(float_hash_vecs[0]).hash(),
            FloatVector(float_hash_vecs[1]).hash());
  float_hash_vecs[1].push_back(500);
  // Adding the element 500.0, which is not present in the first vector, to the
  // second vector
  // leaves the first two vectors distinct, so they should have different hashes
  ASSERT_NE(FloatVector(float_hash_vecs[0]).hash(),
            FloatVector(float_hash_vecs[1]).hash());
  std::reverse(float_hash_vecs[2].begin(), float_hash_vecs[2].end());
  // Reversing the third vector, which was initially the reverse of the first
  // vector,
  // renders the first and third vectors identical, so they should have the same
  // hash
  ASSERT_EQ(FloatVector(float_hash_vecs[0]).hash(),
            FloatVector(float_hash_vecs[2]).hash());
}

TEST(InputHashTests, DoubleVectorsHashCorrectly) {
  // Obtains 3 vectors containing double intepretations of unsigned bytes 0-99.
  // The first two are identical, and the third is reversed
  std::vector<std::vector<double>> double_hash_vecs =
      get_primitive_hash_vectors<double>();
  ASSERT_EQ(DoubleVector(double_hash_vecs[0]).hash(),
            DoubleVector(double_hash_vecs[1]).hash());
  ASSERT_NE(DoubleVector(double_hash_vecs[0]).hash(),
            DoubleVector(double_hash_vecs[2]).hash());
  double_hash_vecs[1].pop_back();
  // Removing the last element of the second vector renders the first two
  // vectors
  // distinct, so they should have different hashes
  ASSERT_NE(DoubleVector(double_hash_vecs[0]).hash(),
            DoubleVector(double_hash_vecs[1]).hash());
  double_hash_vecs[1].push_back(500);
  // Adding the element 500.0, which is not present in the first vector, to the
  // second vector
  // leaves the first two vectors distinct, so they should have different hashes
  ASSERT_NE(DoubleVector(double_hash_vecs[0]).hash(),
            DoubleVector(double_hash_vecs[1]).hash());
  std::reverse(double_hash_vecs[2].begin(), double_hash_vecs[2].end());
  // Reversing the third vector, which was initially the reverse of the first
  // vector,
  // renders the first and third vectors identical, so they should have the same
  // hash
  ASSERT_EQ(DoubleVector(double_hash_vecs[0]).hash(),
            DoubleVector(double_hash_vecs[2]).hash());
}

TEST(InputHashTests, ByteVectorsHashCorrectly) {
  // Obtains 3 vectors containing unsigned bytes 0-99.
  // The first two are identical, and the third is reversed
  std::vector<std::vector<uint8_t>> byte_hash_vecs =
      get_primitive_hash_vectors<uint8_t>();
  ASSERT_EQ(ByteVector(byte_hash_vecs[0]).hash(),
            ByteVector(byte_hash_vecs[1]).hash());
  ASSERT_NE(ByteVector(byte_hash_vecs[0]).hash(),
            ByteVector(byte_hash_vecs[2]).hash());
  byte_hash_vecs[1].pop_back();
  // Removing the last element of the second vector renders the first two
  // vectors
  // distinct, so they should have different hashes
  ASSERT_NE(ByteVector(byte_hash_vecs[0]).hash(),
            ByteVector(byte_hash_vecs[1]).hash());
  byte_hash_vecs[1].push_back(200);
  // Adding an unsigned byte with value 200, which is not present in the first
  // vector,
  // to the second vector leaves the first two vectors distinct, so they should
  // have different hashes
  ASSERT_NE(ByteVector(byte_hash_vecs[0]).hash(),
            ByteVector(byte_hash_vecs[1]).hash());
  std::reverse(byte_hash_vecs[2].begin(), byte_hash_vecs[2].end());
  // Reversing the third vector, which was initially the reverse of the first
  // vector,
  // renders the first and third vectors identical, so they should have the same
  // hash
  ASSERT_EQ(ByteVector(byte_hash_vecs[0]).hash(),
            ByteVector(byte_hash_vecs[2]).hash());
}

TEST(InputHashTests, SerializableStringsHashCorrectly) {
  std::string cat_string = "CAT";
  std::string cat_string_copy = cat_string;
  std::string tac_string = "TAC";

  ASSERT_EQ(SerializableString(cat_string).hash(),
            SerializableString(cat_string_copy).hash());
  ASSERT_NE(SerializableString(cat_string).hash(),
            SerializableString(tac_string).hash());
  // The strings "CATS" and "CAT" are not equal, so they should have different
  // hashes
  ASSERT_NE(SerializableString(cat_string + "S").hash(),
            SerializableString(cat_string).hash());
  std::reverse(tac_string.rbegin(), tac_string.rend());
  // The reverse of the string "TAC" is "CAT", so cat_string and the reverse of
  // tac_string
  // should have identical hashes
  ASSERT_EQ(SerializableString(cat_string).hash(),
            SerializableString(tac_string).hash());
}

TEST(OutputDeserializationTests, PredictionResponseDeserialization) {
  std::string first_string("first_string");
  std::string second_string("second_string");
  uint32_t first_length = static_cast<uint32_t>(first_string.length());
  uint32_t second_length = static_cast<uint32_t>(second_string.length());
  uint32_t num_outputs = 2;
  uint8_t* first_length_bytes = reinterpret_cast<uint8_t*>(&first_length);
  uint8_t* second_length_bytes = reinterpret_cast<uint8_t*>(&second_length);
  uint8_t* num_outputs_bytes = reinterpret_cast<uint8_t*>(&num_outputs);

  // Initialize a buffer to accomodate both strings, in addition to
  // metadata containing the number of outputs and the length of each
  // string as unsigned integers
  int metadata_length = 3 * sizeof(uint32_t);
  std::vector<uint8_t> buf(first_length + second_length + metadata_length);

  memcpy(buf.data(), num_outputs_bytes, sizeof(uint32_t));
  memcpy(buf.data() + sizeof(uint32_t), first_length_bytes, sizeof(uint32_t));
  memcpy(buf.data() + (2 * sizeof(uint32_t)), second_length_bytes,
         sizeof(uint32_t));
  memcpy(buf.data() + metadata_length, first_string.data(),
         first_string.length());
  memcpy(buf.data() + metadata_length + first_string.length(),
         second_string.data(), second_string.length());

  rpc::PredictionResponse response =
      rpc::PredictionResponse::deserialize_prediction_response(buf);
  ASSERT_EQ(response.outputs_.size(), static_cast<size_t>(2));
  ASSERT_EQ(response.outputs_[0], first_string);
  ASSERT_EQ(response.outputs_[1], second_string);
}

}  // namespace
