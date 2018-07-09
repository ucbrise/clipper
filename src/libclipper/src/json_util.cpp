
#include <sstream>
#include <stdexcept>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <boost/algorithm/string.hpp>
#include <unordered_map>

#include <base64.h>

#include <clipper/datatypes.hpp>
#include <clipper/json_util.hpp>
#include <clipper/memory.hpp>
#include <clipper/redis.hpp>

using clipper::DataType;
using clipper::Output;
using clipper::PredictionData;
using clipper::VersionedModelId;
using rapidjson::Type;

namespace clipper {
namespace json {

json_parse_error::json_parse_error(const std::string& what)
    : std::runtime_error(what) {}
json_parse_error::~json_parse_error() throw() {}

json_semantic_error::json_semantic_error(const std::string& what)
    : std::runtime_error(what) {}
json_semantic_error::~json_semantic_error() throw() {}

void check_document_is_object_and_key_exists(rapidjson::Value& d,
                                             const char* key_name) {
  if (!d.IsObject()) {
    throw json_semantic_error("Can only get key-value pair from an object");
  } else if (!d.HasMember(key_name)) {
    throw json_semantic_error("JSON object does not have required key: " +
                              std::string(key_name));
  }
}

rapidjson::Value& check_kv_type_and_return(rapidjson::Value& d,
                                           const char* key_name,
                                           Type expected_type) {
  check_document_is_object_and_key_exists(d, key_name);
  rapidjson::Value& val = d[key_name];
  if (val.GetType() != expected_type) {
    throw json_semantic_error("Type mismatch! JSON key " +
                              std::string(key_name) + " expected type " +
                              kTypeNames[expected_type] + "but found type " +
                              kTypeNames[val.GetType()]);
  }
  return val;
}

rapidjson::Value& check_kv_type_is_bool_and_return(rapidjson::Value& d,
                                                   const char* key_name) {
  check_document_is_object_and_key_exists(d, key_name);
  rapidjson::Value& val = d[key_name];
  if (val.GetType() != rapidjson::kFalseType &&
      val.GetType() != rapidjson::kTrueType) {
    throw json_semantic_error(
        "Type mismatch! JSON key " + std::string(key_name) +
        " expected type bool but found type " + kTypeNames[val.GetType()]);
  }
  return val;
}

/* Getters with error handling for bool, double, float, long, int, string */

bool get_bool(rapidjson::Value& d, const char* key_name) {
  rapidjson::Value& v = check_kv_type_is_bool_and_return(d, key_name);
  if (!v.IsBool()) {
    throw json_semantic_error("PredictionData of type " +
                              kTypeNames[v.GetType()] + " is not of type bool");
  }
  return v.GetBool();
}

double get_double(rapidjson::Value& d, const char* key_name) {
  rapidjson::Value& v =
      check_kv_type_and_return(d, key_name, rapidjson::kNumberType);
  if (!v.IsDouble()) {
    throw json_semantic_error("PredictionData of type " +
                              kTypeNames[v.GetType()] +
                              " is not of type double");
  }
  return v.GetDouble();
}

float get_float(rapidjson::Value& d, const char* key_name) {
  rapidjson::Value& v =
      check_kv_type_and_return(d, key_name, rapidjson::kNumberType);
  if (!v.IsFloat()) {
    throw json_semantic_error("PredictionData of type " +
                              kTypeNames[v.GetType()] +
                              " is not of type float");
  }
  return v.GetFloat();
}

long get_long(rapidjson::Value& d, const char* key_name) {
  rapidjson::Value& v =
      check_kv_type_and_return(d, key_name, rapidjson::kNumberType);
  if (!v.IsInt64()) {
    throw json_semantic_error("PredictionData of type " +
                              kTypeNames[v.GetType()] + " is not of type long");
  }
  return static_cast<long>(v.GetInt64());
}

int get_int(rapidjson::Value& d, const char* key_name) {
  rapidjson::Value& v =
      check_kv_type_and_return(d, key_name, rapidjson::kNumberType);
  if (!v.IsInt()) {
    throw json_semantic_error("PredictionData of type " +
                              kTypeNames[v.GetType()] + " is not of type int");
  }
  return v.GetInt();
}

std::string get_string(rapidjson::Value& d, const char* key_name) {
  rapidjson::Value& v =
      check_kv_type_and_return(d, key_name, rapidjson::kStringType);
  if (!v.IsString()) {
    throw json_semantic_error("Input of type " + kTypeNames[v.GetType()] +
                              " is not of type string");
  }
  return std::string(v.GetString());
}

std::vector<std::string> get_string_array(rapidjson::Value& d,
                                          const char* key_name) {
  rapidjson::Value& v =
      check_kv_type_and_return(d, key_name, rapidjson::kArrayType);
  std::vector<std::string> vals;
  vals.reserve(v.Capacity());
  for (rapidjson::Value& elem : v.GetArray()) {
    if (!elem.IsString()) {
      throw json_semantic_error("Array input of type " +
                                kTypeNames[elem.GetType()] +
                                " is not of type string");
    }
    vals.push_back(elem.GetString());
  }
  return vals;
}

InputParseResult<uint8_t> get_base64_encoded_byte_array(rapidjson::Value& d,
                                                        const char* key_name) {
  rapidjson::Value& v =
      check_kv_type_and_return(d, key_name, rapidjson::kStringType);
  if (!v.IsString()) {
    throw json_semantic_error("PredictionData of type " +
                              kTypeNames[v.GetType()] +
                              " is not of type base64-encoded string");
  }
  Base64 decoder;
  const char* encoded_string = v.GetString();
  size_t encoded_length = v.GetStringLength();
  size_t decoded_length = static_cast<size_t>(
      decoder.DecodedLength(encoded_string, encoded_length));
  UniquePoolPtr<uint8_t> decoded_bytes =
      memory::allocate_unique<uint8_t>(decoded_length);
  decoder.Decode(encoded_string, encoded_length,
                 reinterpret_cast<char*>(decoded_bytes.get()), decoded_length);

  return std::make_pair(std::move(decoded_bytes), decoded_length);
}

/* Getters with error handling for arrays of byte, double, float, int */
InputParseResult<double> get_double_array(rapidjson::Value& d,
                                          const char* key_name) {
  rapidjson::Value& v =
      check_kv_type_and_return(d, key_name, rapidjson::kArrayType);

  size_t arr_size = v.GetArray().Size();
  UniquePoolPtr<double> arr = memory::allocate_unique<double>(arr_size);
  double* arr_data = arr.get();

  size_t arr_idx = 0;
  for (rapidjson::Value& elem : v.GetArray()) {
    if (!elem.IsDouble()) {
      throw json_semantic_error("Array input of type " +
                                kTypeNames[elem.GetType()] +
                                " is not of type double");
    }
    arr_data[arr_idx] = elem.GetDouble();
    arr_idx++;
  }

  return std::make_pair(std::move(arr), arr_size);
}

InputParseResult<float> get_float_array(rapidjson::Value& d,
                                        const char* key_name) {
  rapidjson::Value& v =
      check_kv_type_and_return(d, key_name, rapidjson::kArrayType);

  size_t arr_size = v.GetArray().Size();
  UniquePoolPtr<float> arr = memory::allocate_unique<float>(arr_size);
  float* arr_data = arr.get();

  size_t arr_idx = 0;
  for (rapidjson::Value& elem : v.GetArray()) {
    if (!elem.IsFloat()) {
      throw json_semantic_error("Array input of type " +
                                kTypeNames[elem.GetType()] +
                                " is not of type float");
    }
    arr_data[arr_idx] = elem.GetFloat();
    arr_idx++;
  }

  return std::make_pair(std::move(arr), arr_size);
}

InputParseResult<int> get_int_array(rapidjson::Value& d, const char* key_name) {
  rapidjson::Value& v =
      check_kv_type_and_return(d, key_name, rapidjson::kArrayType);

  size_t arr_size = v.GetArray().Size();
  UniquePoolPtr<int> arr = memory::allocate_unique<int>(arr_size);
  int* arr_data = arr.get();

  size_t arr_idx = 0;
  for (rapidjson::Value& elem : v.GetArray()) {
    if (!elem.IsInt()) {
      throw json_semantic_error("Array input of type " +
                                kTypeNames[elem.GetType()] +
                                " is not of type int");
    }
    arr_data[arr_idx] = elem.GetInt();
    arr_idx++;
  }

  return std::make_pair(std::move(arr), arr_size);
}

InputParseResult<char> get_char_array(rapidjson::Value& d,
                                      const char* key_name) {
  rapidjson::Value& v =
      check_kv_type_and_return(d, key_name, rapidjson::kStringType);
  if (!v.IsString()) {
    throw json_semantic_error("PredictionData of type " +
                              kTypeNames[v.GetType()] +
                              " is not of type string");
  }
  size_t arr_size = v.GetStringLength();
  UniquePoolPtr<char> arr = memory::allocate_unique<char>(arr_size);
  memcpy(arr.get(), v.GetString(), arr_size * sizeof(char));
  return std::make_pair(std::move(arr), arr_size);
}

/*
 * Getters with error handling for nested arrays of byte, double, float, int,
 * char
 */
std::vector<InputParseResult<uint8_t>> get_base64_encoded_byte_arrays(
    rapidjson::Value& d, const char* key_name) {
  rapidjson::Value& v =
      check_kv_type_and_return(d, key_name, rapidjson::kArrayType);
  std::vector<InputParseResult<uint8_t>> byte_arrays;
  byte_arrays.reserve(v.Capacity());
  Base64 decoder;

  for (rapidjson::Value& elem : v.GetArray()) {
    if (!elem.IsString()) {
      throw json_semantic_error("PredictionData of type " +
                                kTypeNames[elem.GetType()] +
                                " is not of type base64-encoded string");
    }
    const char* encoded_string = elem.GetString();
    size_t encoded_length = elem.GetStringLength();
    size_t decoded_length = static_cast<size_t>(
        decoder.DecodedLength(encoded_string, encoded_length));
    UniquePoolPtr<uint8_t> decoded_bytes =
        memory::allocate_unique<uint8_t>(decoded_length);
    decoder.Decode(encoded_string, encoded_length,
                   reinterpret_cast<char*>(decoded_bytes.get()),
                   decoded_length);
    byte_arrays.push_back(
        std::make_pair(std::move(decoded_bytes), decoded_length));
  }
  return byte_arrays;
}

std::vector<InputParseResult<double>> get_double_arrays(rapidjson::Value& d,
                                                        const char* key_name) {
  rapidjson::Value& v =
      check_kv_type_and_return(d, key_name, rapidjson::kArrayType);
  std::vector<InputParseResult<double>> double_arrays;

  double_arrays.reserve(v.Capacity());
  for (rapidjson::Value& elem_array : v.GetArray()) {
    if (!elem_array.IsArray()) {
      throw json_semantic_error("Array input of type " +
                                kTypeNames[elem_array.GetType()] +
                                " is not of type array");
    }
    size_t arr_size = elem_array.Size();
    UniquePoolPtr<double> arr = memory::allocate_unique<double>(arr_size);
    double* arr_data = arr.get();

    size_t arr_idx = 0;
    for (rapidjson::Value& elem : elem_array.GetArray()) {
      if (!elem.IsDouble()) {
        throw json_semantic_error("Array input of type " +
                                  kTypeNames[elem.GetType()] +
                                  " is not of type double");
      }
      arr_data[arr_idx] = elem.GetDouble();
      arr_idx++;
    }
    double_arrays.push_back(std::make_pair(std::move(arr), arr_size));
  }
  return double_arrays;
}

std::vector<InputParseResult<float>> get_float_arrays(rapidjson::Value& d,
                                                      const char* key_name) {
  rapidjson::Value& v =
      check_kv_type_and_return(d, key_name, rapidjson::kArrayType);
  std::vector<InputParseResult<float>> float_arrays;

  float_arrays.reserve(v.Capacity());
  for (rapidjson::Value& elem_array : v.GetArray()) {
    if (!elem_array.IsArray()) {
      throw json_semantic_error("Array input of type " +
                                kTypeNames[elem_array.GetType()] +
                                " is not of type array");
    }
    size_t arr_size = elem_array.Size();
    UniquePoolPtr<float> arr = memory::allocate_unique<float>(arr_size);
    float* arr_data = arr.get();

    size_t arr_idx = 0;
    for (rapidjson::Value& elem : elem_array.GetArray()) {
      if (!elem.IsFloat()) {
        throw json_semantic_error("Array input of type " +
                                  kTypeNames[elem.GetType()] +
                                  " is not of type float");
      }
      arr_data[arr_idx] = elem.GetFloat();
      arr_idx++;
    }
    float_arrays.push_back(std::make_pair(std::move(arr), arr_size));
  }
  return float_arrays;
}

std::vector<InputParseResult<int>> get_int_arrays(rapidjson::Value& d,
                                                  const char* key_name) {
  rapidjson::Value& v =
      check_kv_type_and_return(d, key_name, rapidjson::kArrayType);
  std::vector<InputParseResult<int>> int_arrays;

  int_arrays.reserve(v.Capacity());
  for (rapidjson::Value& elem_array : v.GetArray()) {
    if (!elem_array.IsArray()) {
      throw json_semantic_error("Array input of type " +
                                kTypeNames[elem_array.GetType()] +
                                " is not of type array");
    }
    size_t arr_size = elem_array.Size();
    UniquePoolPtr<int> arr = memory::allocate_unique<int>(arr_size);
    int* arr_data = arr.get();

    size_t arr_idx = 0;
    for (rapidjson::Value& elem : elem_array.GetArray()) {
      if (!elem.IsInt()) {
        throw json_semantic_error("Array input of type " +
                                  kTypeNames[elem.GetType()] +
                                  " is not of type int");
      }
      arr_data[arr_idx] = elem.GetInt();
      arr_idx++;
    }
    int_arrays.push_back(std::make_pair(std::move(arr), arr_size));
  }
  return int_arrays;
}

std::vector<InputParseResult<char>> get_char_arrays(rapidjson::Value& d,
                                                    const char* key_name) {
  rapidjson::Value& v =
      check_kv_type_and_return(d, key_name, rapidjson::kArrayType);
  std::vector<InputParseResult<char>> vals;
  vals.reserve(v.Capacity());
  for (rapidjson::Value& elem : v.GetArray()) {
    if (!elem.IsString()) {
      throw json_semantic_error("Array input of type " +
                                kTypeNames[elem.GetType()] +
                                " is not of type string");
    }
    size_t arr_size = elem.GetStringLength();
    UniquePoolPtr<char> arr = memory::allocate_unique<char>(arr_size);
    memcpy(arr.get(), elem.GetString(), arr_size * sizeof(char));
    vals.push_back(std::make_pair(std::move(arr), arr_size));
  }
  return vals;
}

std::vector<VersionedModelId> get_candidate_models(rapidjson::Value& d,
                                                   const char* key_name) {
  rapidjson::Value& v =
      check_kv_type_and_return(d, key_name, rapidjson::kArrayType);
  std::vector<VersionedModelId> candidate_models;
  candidate_models.reserve(v.Capacity());
  for (rapidjson::Value& elem : v.GetArray()) {
    if (!elem.IsObject()) {
      throw json_semantic_error("Array input of type " +
                                kTypeNames[elem.GetType()] +
                                " is not of type Object");
    } else if (!elem.HasMember("model_name")) {
      throw json_semantic_error(
          "Candidate model JSON object missing model_name.");
    } else if (!elem.HasMember("model_version")) {
      throw json_semantic_error(
          "Candidate model JSON object missing model_version.");
    }
    std::string model_name = get_string(elem, "model_name");
    std::string model_version = get_string(elem, "model_version");
    candidate_models.push_back(VersionedModelId(model_name, model_version));
  }
  return candidate_models;
}

rapidjson::Value& get_object(rapidjson::Value& d, const char* key_name) {
  rapidjson::Value& object =
      check_kv_type_and_return(d, key_name, rapidjson::kObjectType);
  return object;
}

void parse_json(const std::string& json_content, rapidjson::Document& d) {
  rapidjson::ParseResult ok = d.Parse(json_content.c_str());
  if (!ok) {
    std::stringstream ss;
    ss << "JSON parse error: " << rapidjson::GetParseError_En(ok.Code())
       << " (offset " << ok.Offset() << ")\n";
    throw json_parse_error(ss.str());
  }
}

std::vector<std::shared_ptr<PredictionData>> parse_inputs(DataType input_type,
                                                          rapidjson::Value& d) {
  if (d.HasMember("input")) {
    std::vector<std::shared_ptr<PredictionData>> wrapped_result;
    std::shared_ptr<PredictionData> result = parse_single_input(input_type, d);
    wrapped_result.push_back(std::move(result));
    return wrapped_result;
  } else if (d.HasMember("input_batch")) {
    return parse_input_batch(input_type, d);
  } else {
    throw json_semantic_error(
        "JSON object does not have required keys input or input_batch");
  }
}

std::shared_ptr<PredictionData> parse_single_input(DataType input_type,
                                                   rapidjson::Value& d) {
  switch (input_type) {
    case DataType::Doubles: {
      InputParseResult<double> input = get_double_array(d, "input");
      return std::make_shared<DoubleVector>(std::move(input.first),
                                            input.second);
    }
    case DataType::Floats: {
      InputParseResult<float> input = get_float_array(d, "input");
      return std::make_shared<FloatVector>(std::move(input.first),
                                           input.second);
    }
    case DataType::Ints: {
      InputParseResult<int> input = get_int_array(d, "input");
      return std::make_shared<IntVector>(std::move(input.first), input.second);
    }
    case DataType::Strings: {
      InputParseResult<char> input = get_char_array(d, "input");
      return std::make_shared<SerializableString>(std::move(input.first),
                                                  input.second);
    }
    case DataType::Bytes: {
      InputParseResult<uint8_t> input =
          get_base64_encoded_byte_array(d, "input");
      return std::make_shared<ByteVector>(std::move(input.first), input.second);
    }
    default: throw std::invalid_argument("input_type is not a valid type");
  }
}

std::vector<std::shared_ptr<PredictionData>> parse_input_batch(
    DataType input_type, rapidjson::Value& d) {
  std::vector<std::shared_ptr<PredictionData>> result;
  switch (input_type) {
    case DataType::Doubles: {
      auto input_batch = get_double_arrays(d, "input_batch");
      for (auto& input : input_batch) {
        result.push_back(std::make_shared<DoubleVector>(std::move(input.first),
                                                        input.second));
      }
    } break;
    case DataType::Floats: {
      auto input_batch = get_float_arrays(d, "input_batch");
      for (auto& input : input_batch) {
        result.push_back(std::make_shared<FloatVector>(std::move(input.first),
                                                       input.second));
      }
    } break;
    case DataType::Ints: {
      auto input_batch = get_int_arrays(d, "input_batch");
      for (auto& input : input_batch) {
        result.push_back(
            std::make_shared<IntVector>(std::move(input.first), input.second));
      }
    } break;
    case DataType::Strings: {
      auto input_batch = get_char_arrays(d, "input_batch");
      for (auto& input : input_batch) {
        result.push_back(std::make_shared<SerializableString>(
            std::move(input.first), input.second));
      }
    } break;
    case DataType::Bytes: {
      auto input_batch = get_base64_encoded_byte_arrays(d, "input_batch");
      for (auto& input : input_batch) {
        result.push_back(
            std::make_shared<ByteVector>(std::move(input.first), input.second));
      }
    } break;
    default: throw std::invalid_argument("input_type is not a valid type");
  }
  return result;
}

/* Utilities for serialization into JSON */
void add_kv_pair(rapidjson::Document& d, const char* key_name,
                 rapidjson::Value& value_to_add) {
  if (!d.IsObject()) {
    throw json_semantic_error("Can only add a key-value pair to an object");
  } else if (d.HasMember(key_name)) {
    // Remove old value associated with key_name
    d.RemoveMember(key_name);
  }
  rapidjson::Document::AllocatorType& allocator = d.GetAllocator();
  rapidjson::Value key(key_name, allocator);
  d.AddMember(key, value_to_add, allocator);
}

void add_bool(rapidjson::Document& d, const char* key_name, bool value_to_add) {
  rapidjson::Document boolean_doc;
  boolean_doc.SetBool(value_to_add);
  add_kv_pair(d, key_name, boolean_doc);
}

void add_double_array(rapidjson::Document& d, const char* key_name,
                      std::vector<double>& values_to_add) {
  rapidjson::Value double_array(rapidjson::kArrayType);
  rapidjson::Document::AllocatorType& allocator = d.GetAllocator();
  for (std::size_t i = 0; i < values_to_add.size(); i++) {
    double_array.PushBack(values_to_add[i], allocator);
  }
  add_kv_pair(d, key_name, double_array);
}

void add_float_array(rapidjson::Document& d, const char* key_name,
                     std::vector<float>& values_to_add) {
  rapidjson::Value float_array(rapidjson::kArrayType);
  rapidjson::Document::AllocatorType& allocator = d.GetAllocator();
  for (std::size_t i = 0; i < values_to_add.size(); i++) {
    float_array.PushBack(values_to_add[i], allocator);
  }
  add_kv_pair(d, key_name, float_array);
}

void add_int_array(rapidjson::Document& d, const char* key_name,
                   std::vector<int>& values_to_add) {
  rapidjson::Value int_array(rapidjson::kArrayType);
  rapidjson::Document::AllocatorType& allocator = d.GetAllocator();
  for (std::size_t i = 0; i < values_to_add.size(); i++) {
    int_array.PushBack(values_to_add[i], allocator);
  }
  add_kv_pair(d, key_name, int_array);
}

void add_string_array(rapidjson::Document& d, const char* key_name,
                      std::vector<std::string>& values_to_add) {
  rapidjson::Value string_array(rapidjson::kArrayType);
  rapidjson::Document::AllocatorType& allocator = d.GetAllocator();
  for (std::size_t i = 0; i < values_to_add.size(); i++) {
    rapidjson::Value string_val(values_to_add[i].c_str(), allocator);
    string_array.PushBack(string_val, allocator);
  }
  add_kv_pair(d, key_name, string_array);
}

void add_json_array(rapidjson::Document& d, const char* key_name,
                    std::vector<std::string>& values_to_add) {
  rapidjson::Value json_array(rapidjson::kArrayType);
  rapidjson::Document::AllocatorType& allocator = d.GetAllocator();
  for (std::string json_string : values_to_add) {
    rapidjson::Document json_obj(&allocator);
    parse_json(json_string, json_obj);
    json_array.PushBack(json_obj, allocator);
  }

  add_kv_pair(d, key_name, json_array);
}

void add_double(rapidjson::Document& d, const char* key_name, double val) {
  rapidjson::Value val_to_add(val);
  add_kv_pair(d, key_name, val_to_add);
}

void add_float(rapidjson::Document& d, const char* key_name, float val) {
  rapidjson::Value val_to_add(val);
  add_kv_pair(d, key_name, val_to_add);
}

void add_int(rapidjson::Document& d, const char* key_name, int val) {
  rapidjson::Value val_to_add(val);
  add_kv_pair(d, key_name, val_to_add);
}

void add_long(rapidjson::Document& d, const char* key_name, long val) {
  rapidjson::Value val_to_add((int64_t)val);
  add_kv_pair(d, key_name, val_to_add);
}

void add_string(rapidjson::Document& d, const char* key_name,
                const std::string& val) {
  // We specify the string length in the second parameter to prevent
  // strings containing null terminators from being prematurely truncated
  rapidjson::Value val_to_add(val.c_str(), static_cast<int>(val.length()),
                              d.GetAllocator());
  add_kv_pair(d, key_name, val_to_add);
}

void add_object(rapidjson::Document& d, const char* key_name,
                rapidjson::Document& to_add) {
  add_kv_pair(d, key_name, to_add);
}

std::string to_json_string(rapidjson::Document& d) {
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  d.Accept(writer);
  return buffer.GetString();
}

void set_string_array(rapidjson::Document& d,
                      const std::vector<std::string>& str_array) {
  d.SetArray();
  for (auto const& str : str_array) {
    rapidjson::Value string_val(rapidjson::StringRef(str.c_str(), str.length()),
                                d.GetAllocator());
    d.PushBack(string_val, d.GetAllocator());
  }
}

std::vector<std::string> to_string_array(rapidjson::Document& d) {
  if (!d.IsArray()) {
    throw json_semantic_error("Document must be of array type");
  }
  std::vector<std::string> converted_array;

  for (rapidjson::Value& elem : d.GetArray()) {
    if (!elem.IsString()) {
      throw json_semantic_error("Array element of type " +
                                kTypeNames[elem.GetType()] +
                                " is not of type string");
    }
    converted_array.push_back(elem.GetString());
  }
  return converted_array;
}

void check_key_exists_in_map(
    std::string& key, const std::unordered_map<std::string, std::string>& map) {
  if (map.find(key) == map.end()) {
    throw std::invalid_argument("key `" + key + "` does not exist in map");
  }
}

void add_input_type_from_redis(
    rapidjson::Document& d,
    const std::unordered_map<std::string, std::string>& app_metadata) {
  std::string key = "input_type";
  check_key_exists_in_map(key, app_metadata);
  add_string(d, key.c_str(), app_metadata.at(key));
}

void add_app_default_output_from_redis(
    rapidjson::Document& d,
    const std::unordered_map<std::string, std::string>& app_metadata) {
  std::string key = "default_output";
  check_key_exists_in_map(key, app_metadata);
  add_string(d, key.c_str(), app_metadata.at(key));
}

void add_app_latency_slo_micros_from_redis(
    rapidjson::Document& d,
    const std::unordered_map<std::string, std::string>& app_metadata) {
  // latency_slo_micros is stored as a string in redis
  std::string key = "latency_slo_micros";
  check_key_exists_in_map(key, app_metadata);
  add_int(d, key.c_str(), atoi(app_metadata.at(key).c_str()));
}

void redis_app_metadata_to_json(
    rapidjson::Document& d,
    const std::unordered_map<std::string, std::string>& app_metadata) {
  d.SetObject();
  add_input_type_from_redis(d, app_metadata);
  add_app_default_output_from_redis(d, app_metadata);
  add_app_latency_slo_micros_from_redis(d, app_metadata);
}

void add_model_name_from_redis(
    rapidjson::Document& d,
    const std::unordered_map<std::string, std::string>& model_metadata) {
  std::string key = "model_name";
  check_key_exists_in_map(key, model_metadata);
  add_string(d, key.c_str(), model_metadata.at(key));
}

void add_model_version_from_redis(
    rapidjson::Document& d,
    const std::unordered_map<std::string, std::string>& model_metadata) {
  std::string key = "model_version";
  check_key_exists_in_map(key, model_metadata);
  add_string(d, key.c_str(), model_metadata.at(key));
}

void add_model_labels_from_redis(
    rapidjson::Document& d,
    const std::unordered_map<std::string, std::string>& model_metadata) {
  std::string key = "labels";
  check_key_exists_in_map(key, model_metadata);
  std::vector<std::string> labels_vec =
      clipper::redis::str_to_labels(model_metadata.at(key));
  add_string_array(d, key.c_str(), labels_vec);
}

void add_container_name_from_redis(
    rapidjson::Document& d,
    const std::unordered_map<std::string, std::string>& model_metadata) {
  std::string key = "container_name";
  check_key_exists_in_map(key, model_metadata);
  add_string(d, key.c_str(), model_metadata.at(key));
}

void add_model_data_path_from_redis(
    rapidjson::Document& d,
    const std::unordered_map<std::string, std::string>& model_metadata) {
  std::string key = "model_data_path";
  check_key_exists_in_map(key, model_metadata);
  add_string(d, key.c_str(), model_metadata.at(key));
}

void redis_model_metadata_to_json(
    rapidjson::Document& d,
    const std::unordered_map<std::string, std::string>& model_metadata) {
  d.SetObject();
  add_model_name_from_redis(d, model_metadata);
  add_model_version_from_redis(d, model_metadata);

  // TODO: include load in returned value when we start tracking
  // model load. Currently load is not tracked so returning it
  // to the user will just confuse them (it's always 0).
  // add_model_load_from_redis(d, model_data);

  add_input_type_from_redis(d, model_metadata);
  add_model_labels_from_redis(d, model_metadata);
  add_container_name_from_redis(d, model_metadata);
  add_model_data_path_from_redis(d, model_metadata);
}

void add_model_id_key_from_redis(
    rapidjson::Document& d,
    const std::unordered_map<std::string, std::string>& container_metadata) {
  std::string key = "model_id";
  check_key_exists_in_map(key, container_metadata);
  add_string(d, key.c_str(), container_metadata.at(key));
}

void add_model_replica_id_from_redis(
    rapidjson::Document& d,
    const std::unordered_map<std::string, std::string>& container_metadata) {
  std::string key = "model_replica_id";
  check_key_exists_in_map(key, container_metadata);
  add_int(d, key.c_str(), std::stoi(container_metadata.at(key)));
}

void redis_container_metadata_to_json(
    rapidjson::Document& d,
    const std::unordered_map<std::string, std::string>& container_metadata) {
  d.SetObject();
  add_model_id_key_from_redis(d, container_metadata);
  add_model_name_from_redis(d, container_metadata);
  add_model_version_from_redis(d, container_metadata);
  add_model_replica_id_from_redis(d, container_metadata);
  // TODO: uncomment this when we start tracking batch size in Redis.
  // add_container_batch_size_from_redis(d, container_metadata);
  add_input_type_from_redis(d, container_metadata);
}

}  // namespace json
}  // namespace clipper
