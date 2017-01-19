#include <stdexcept>

#include <gtest/gtest.h>
#include <clipper/datatypes.hpp>

using namespace clipper;
using std::vector;

namespace {

  const unsigned long SERIALIZED_REQUEST_SIZE = 3;
  const int NUM_PRIMITIVE_INPUTS = 500;
  const int PRIMITIVE_INPUT_SIZE_ELEMS = 784;
  const int NUM_STRING_INPUTS = 300;

  template<typename T>
  void get_primitive_data_vectors(clipper::InputType type, std::vector<std::vector<T>> &data_vectors) {
    for (int i = 0; i < NUM_PRIMITIVE_INPUTS; i++) {
      std::vector<T> data_vector;
      for (int j = 0; j < PRIMITIVE_INPUT_SIZE_ELEMS; j++) {
        // Differentiate vectors by populating them with the index j under differing moduli
        int k = j % (i + 1);
        if (type == clipper::InputType::Bytes) {
          uint8_t *bytes = reinterpret_cast<uint8_t *>(&k);
          for (int i = 0; i < (int) (sizeof(int) / sizeof(uint8_t)); i++) {
            data_vector.push_back(*(bytes + i));
          }
        } else {
          data_vector.push_back(static_cast<T>(k));
        }
      }
      data_vectors.push_back(data_vector);
    }
  }

  void get_string_data(std::vector<std::string> &string_vector) {
    for(int i = 0; i < NUM_STRING_INPUTS; i++) {
      std::string str;
      switch(i % 3) {
        case 0:
          str = std::string("CAT");
          break;
        case 1:
          str = std::string("DOG");
          break;
        case 2:
          str = std::string("COW");
          break;
        default:
          str = std::string("INVALID");
          break;
      }
      string_vector.push_back(str);
    }
  }

  TEST(SerializationTests, EmptySerialization) {
    clipper::rpc::PredictionRequest request(InputType::Bytes);
    try {
      request.serialize();
    } catch (std::length_error) {
      SUCCEED();
      return;
    }
    FAIL();
  }

  TEST(SerializationTests, ByteSerialization) {
    clipper::rpc::PredictionRequest request(InputType::Bytes);
    std::vector<std::vector<uint8_t>> data_vectors;
    get_primitive_data_vectors(InputType::Bytes, data_vectors);
    for(int i = 0; i < (int) data_vectors.size(); i++) {
      std::shared_ptr<ByteVector> byte_vec = std::make_shared<ByteVector>(data_vectors[i]);
      request.add_input(byte_vec);
    }
    std::vector<clipper::ByteBuffer> serialized_request = request.serialize();
    ASSERT_EQ(serialized_request.size(), SERIALIZED_REQUEST_SIZE);
    clipper::ByteBuffer request_header = serialized_request[0];
    clipper::ByteBuffer input_header = serialized_request[1];
    clipper::ByteBuffer raw_content = serialized_request[2];
    uint32_t* raw_request_type = reinterpret_cast<uint32_t*>(request_header.data());
    ASSERT_EQ(*raw_request_type, static_cast<uint32_t>(clipper::RequestType::PredictRequest));
    uint32_t* typed_input_header = reinterpret_cast<uint32_t*>(input_header.data());
    ASSERT_EQ(*typed_input_header, static_cast<uint32_t>(clipper::InputType::Bytes));
    typed_input_header++;
    uint32_t num_inputs = *typed_input_header;
    ASSERT_EQ(num_inputs, static_cast<uint32_t>(data_vectors.size()));
    typed_input_header++;

    uint8_t* content_ptr = raw_content.data();
    uint32_t prev_index = 0;
    for(int i = 0; i < (int) num_inputs - 1; i++) {
      uint32_t split_index = typed_input_header[i];
      std::vector<uint8_t> vec(content_ptr + prev_index, content_ptr + split_index);
      ASSERT_EQ(vec, data_vectors[i]);
      prev_index = split_index;
    }
    // Our splits define internal indices at which to delimit input vectors, so they exclude 0
    // and the content length. We therefore have to check the consistency of the final vector.
    // This vector is composed of the elements from the last split index through the total content length.
    std::vector<uint8_t> tail_vec(content_ptr + prev_index, content_ptr + (raw_content.size() / sizeof(uint8_t)));
    ASSERT_EQ(tail_vec, data_vectors[data_vectors.size() - 1]);
  }

  TEST(SerializationTests, IntSerialization) {
  clipper::rpc::PredictionRequest request(InputType::Ints);
    std::vector<std::vector<int>> data_vectors;
    get_primitive_data_vectors(InputType::Ints, data_vectors);
    for(int i = 0; i < (int) data_vectors.size(); i++) {
      std::shared_ptr<IntVector> int_vec = std::make_shared<IntVector>(data_vectors[i]);
      request.add_input(int_vec);
    }
    std::vector<clipper::ByteBuffer> serialized_request = request.serialize();
    ASSERT_EQ(serialized_request.size(), SERIALIZED_REQUEST_SIZE);
    clipper::ByteBuffer request_header = serialized_request[0];
    clipper::ByteBuffer input_header = serialized_request[1];
    clipper::ByteBuffer raw_content = serialized_request[2];
    uint32_t* raw_request_type = reinterpret_cast<uint32_t*>(request_header.data());
    ASSERT_EQ(*raw_request_type, static_cast<uint32_t>(clipper::RequestType::PredictRequest));
    uint32_t* typed_input_header = reinterpret_cast<uint32_t*>(input_header.data());
    ASSERT_EQ(*typed_input_header, static_cast<uint32_t>(clipper::InputType::Ints));
    typed_input_header++;
    uint32_t num_inputs = *typed_input_header;
    ASSERT_EQ(num_inputs, static_cast<uint32_t>(data_vectors.size()));
    typed_input_header++;

    int* content_ptr = reinterpret_cast<int*>(raw_content.data());
    uint32_t prev_index = 0;
    for(int i = 0; i < (int) num_inputs - 1; i++) {
      uint32_t split_index = typed_input_header[i];
      std::vector<int> vec(content_ptr + prev_index, content_ptr + split_index);
      ASSERT_EQ(vec, data_vectors[i]);
      prev_index = split_index;
    }
    // Our splits define internal indices at which to delimit input vectors, so they exclude 0
    // and the content length. We therefore have to check the consistency of the final vector.
    // This vector is composed of the elements from the last split index through the total content length.
    std::vector<int> tail_vec(content_ptr + prev_index, content_ptr + (raw_content.size() / sizeof(int)));
    ASSERT_EQ(tail_vec, data_vectors[data_vectors.size() - 1]);
  }

  TEST(SerializationTests, FloatSerialization) {
    clipper::rpc::PredictionRequest request(InputType::Floats);
    std::vector<std::vector<float>> data_vectors;
    get_primitive_data_vectors(InputType::Floats, data_vectors);
    for(int i = 0; i < (int) data_vectors.size(); i++) {
      std::shared_ptr<FloatVector> float_vec = std::make_shared<FloatVector>(data_vectors[i]);
      request.add_input(float_vec);
    }
    std::vector<clipper::ByteBuffer> serialized_request = request.serialize();
    ASSERT_EQ(serialized_request.size(), SERIALIZED_REQUEST_SIZE);
    clipper::ByteBuffer request_header = serialized_request[0];
    clipper::ByteBuffer input_header = serialized_request[1];
    clipper::ByteBuffer raw_content = serialized_request[2];
    uint32_t* raw_request_type = reinterpret_cast<uint32_t*>(request_header.data());
    ASSERT_EQ(*raw_request_type, static_cast<uint32_t>(clipper::RequestType::PredictRequest));
    uint32_t* typed_input_header = reinterpret_cast<uint32_t*>(input_header.data());
    ASSERT_EQ(*typed_input_header, static_cast<uint32_t>(clipper::InputType::Floats));
    typed_input_header++;
    uint32_t num_inputs = *typed_input_header;
    ASSERT_EQ(num_inputs, static_cast<uint32_t>(data_vectors.size()));
    typed_input_header++;

    float* content_ptr = reinterpret_cast<float*>(raw_content.data());
    uint32_t prev_index = 0;
    for(int i = 0; i < (int) num_inputs - 1; i++) {
      uint32_t split_index = typed_input_header[i];
      std::vector<float> vec(content_ptr + prev_index, content_ptr + split_index);
      ASSERT_EQ(vec, data_vectors[i]);
      prev_index = split_index;
    }
    // Our splits define internal indices at which to delimit input vectors, so they exclude 0
    // and the content length. We therefore have to check the consistency of the final vector.
    // This vector is composed of the elements from the last split index through the total content length.
    std::vector<float> tail_vec(content_ptr + prev_index, content_ptr + (raw_content.size() / sizeof(float)));
    ASSERT_EQ(tail_vec, data_vectors[data_vectors.size() - 1]);
  }

  TEST(SerializationTests, DoubleSerialization) {
    clipper::rpc::PredictionRequest request(InputType::Doubles);
    std::vector<std::vector<double>> data_vectors;
    get_primitive_data_vectors(InputType::Doubles, data_vectors);
    for(int i = 0; i < (int) data_vectors.size(); i++) {
      std::shared_ptr<DoubleVector> double_vec = std::make_shared<DoubleVector>(data_vectors[i]);
      request.add_input(double_vec);
    }
    std::vector<clipper::ByteBuffer> serialized_request = request.serialize();
    ASSERT_EQ(serialized_request.size(), SERIALIZED_REQUEST_SIZE);
    clipper::ByteBuffer request_header = serialized_request[0];
    clipper::ByteBuffer input_header = serialized_request[1];
    clipper::ByteBuffer raw_content = serialized_request[2];
    uint32_t* raw_request_type = reinterpret_cast<uint32_t*>(request_header.data());
    ASSERT_EQ(*raw_request_type, static_cast<uint32_t>(clipper::RequestType::PredictRequest));
    uint32_t* typed_input_header = reinterpret_cast<uint32_t*>(input_header.data());
    ASSERT_EQ(*typed_input_header, static_cast<uint32_t>(clipper::InputType::Doubles));
    typed_input_header++;
    uint32_t num_inputs = *typed_input_header;
    ASSERT_EQ(num_inputs, static_cast<uint32_t>(data_vectors.size()));
    typed_input_header++;

    double* content_ptr = reinterpret_cast<double*>(raw_content.data());
    uint32_t prev_index = 0;
    for(int i = 0; i < (int) num_inputs - 1; i++) {
      uint32_t split_index = typed_input_header[i];
      std::vector<double> vec(content_ptr + prev_index, content_ptr + split_index);
      ASSERT_EQ(vec, data_vectors[i]);
      prev_index = split_index;
    }
    // Our splits define internal indices at which to delimit input vectors, so they exclude 0
    // and the content length. We therefore have to check the consistency of the final vector.
    // This vector is composed of the elements from the last split index through the total content length.
    std::vector<double> tail_vec(content_ptr + prev_index, content_ptr + (raw_content.size() / sizeof(double)));
    ASSERT_EQ(tail_vec, data_vectors[data_vectors.size() - 1]);
  }

  TEST(SerializationTests, StringSerialization) {
    clipper::rpc::PredictionRequest request(InputType::Strings);
    std::vector<std::string> string_vector;
    get_string_data(string_vector);
    for(int i = 0; i < (int) string_vector.size(); i++) {
      std::shared_ptr<SerializableString> serializable_str = std::make_shared<SerializableString>(string_vector[i]);
      request.add_input(serializable_str);
    }
    std::vector<clipper::ByteBuffer> serialized_request = request.serialize();
    ASSERT_EQ(serialized_request.size(), SERIALIZED_REQUEST_SIZE);
    clipper::ByteBuffer request_header = serialized_request[0];
    clipper::ByteBuffer input_header = serialized_request[1];
    clipper::ByteBuffer raw_content = serialized_request[2];
    uint32_t* raw_request_type = reinterpret_cast<uint32_t*>(request_header.data());
    ASSERT_EQ(*raw_request_type, static_cast<uint32_t>(clipper::RequestType::PredictRequest));
    uint32_t* typed_input_header = reinterpret_cast<uint32_t*>(input_header.data());
    ASSERT_EQ(*typed_input_header, static_cast<uint32_t>(clipper::InputType::Strings));
    typed_input_header++;
    uint32_t num_inputs = *typed_input_header;
    ASSERT_EQ(num_inputs, static_cast<uint32_t>(string_vector.size()));
    typed_input_header++;

    char* content_ptr = reinterpret_cast<char*>(raw_content.data());
    for(int i = 0; i < (int) num_inputs; i++) {
      std::string str(content_ptr);
      ASSERT_EQ(str, string_vector[i]);
      content_ptr += (str.length() + 1);
    }
  }

  TEST(SerializationTests, RpcPredictionRequestsOnlyAcceptValidInputs) {
    int int_data[] = {5, 6, 7, 8, 9};
    std::vector<int> raw_data_vec;
    for(int elem : int_data) {
      raw_data_vec.push_back(elem);
    }
    std::shared_ptr<IntVector> int_vec = std::make_shared<IntVector>(raw_data_vec);
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
    ASSERT_THROW(rpc::PredictionRequest(inputs, InputType::Doubles), std::invalid_argument);

    // We expect an invalid argument exception when we attempt to add
    // an IntVector input to a double-typed PredictionRequest
    rpc::PredictionRequest doubles_prediction_request(InputType::Doubles);
    ASSERT_THROW(doubles_prediction_request.add_input(int_vec), std::invalid_argument);
  }

} //namespace