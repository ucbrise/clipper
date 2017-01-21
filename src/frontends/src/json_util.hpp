#include <stdexcept>
#include <rapidjson/document.h>
#include <clipper/datatypes.hpp>

using clipper::Input;
using clipper::InputType;
using clipper::Output;
using clipper::OutputType;
using clipper::VersionedModelId;
using rapidjson::Type;

static std::vector<std::string> kTypeNames =
  {"Null", "False", "True", "Object", "Array", "String", "Number"};

// RapidJSON value types
/*
enum Type {
  kNullType = 0,      // null
  kFalseType = 1,     // false
  kTrueType = 2,      // true
  kObjectType = 3,    // object
  kArrayType = 4,     // array
  kStringType = 5,    // string
  kNumberType = 6     // number
}; */

namespace clipper_json {

class json_parse_error : public std::runtime_error {
 public:
  json_parse_error(const std::string &what) : std::runtime_error(what) {}
  ~json_parse_error() throw() {};
};

class json_semantic_error : public std::runtime_error {
 public:
  json_semantic_error(const std::string &what) : std::runtime_error(what) {}
  ~json_semantic_error() throw() {};
};

/* Check for matching types else throw exception */
void check_type(
    rapidjson::Document& d, const char* key_name, Type expected_type) {
  if (!d.HasMember(key_name)) {
    throw json_semantic_error(
      "JSON object does not have required key: " + std::string(key_name));
  }
  rapidjson::Value& val = d[key_name];
  if (val.GetType() != expected_type) {
    throw json_semantic_error(
      "Type mismatch! JSON key " + std::string(key_name) + " expected type " +
      kTypeNames[expected_type] + "but found type " + kTypeNames[val.GetType()]);
  }
}

/* Getters with error handling for double, float, long, int, string */
double getDouble(rapidjson::Document& d, const char* key_name) {
  check_type(d, key_name, rapidjson::kNumberType);
  rapidjson::Value& v = d[key_name];
  if (!v.IsDouble()) {
    throw json_semantic_error(
      "Input of type " + kTypeNames[v.GetType()] + "is not of type double");
  }
  return v.GetDouble();
}

float getFloat(rapidjson::Document& d, const char* key_name) {
  check_type(d, key_name, rapidjson::kNumberType);
  rapidjson::Value& v = d[key_name];
  if (!v.IsFloat()) {
    throw json_semantic_error(
      "Input of type " + kTypeNames[v.GetType()] + "is not of type float");
  }
  return v.GetFloat();
}

long getLong(rapidjson::Document& d, const char* key_name) {
  check_type(d, key_name, rapidjson::kNumberType);
  rapidjson::Value& v = d[key_name];
  if (!v.IsInt64()) {
    throw json_semantic_error(
      "Input of type " + kTypeNames[v.GetType()] + "is not of type long");
  }
  return (long) v.GetInt64();
}

int getInt(rapidjson::Document& d, const char* key_name) {
  check_type(d, key_name, rapidjson::kNumberType);
  rapidjson::Value& v = d[key_name];
  if (!v.IsInt()) {
    throw json_semantic_error(
      "Input of type " + kTypeNames[v.GetType()] + "is not of type int");
  }
  return v.GetInt();
}

std::string getString(rapidjson::Document& d, const char* key_name) {
  check_type(d, key_name, rapidjson::kStringType);
  rapidjson::Value& v = d[key_name];
  if (!v.IsString()) {
    throw json_semantic_error(
      "Input of type " + kTypeNames[v.GetType()] + "is not of type string");
  }
  return std::string(v.GetString());
}

/* Getters with error handling for arrays of double, float, int */
std::vector<double> getDoubleArray(rapidjson::Document& d,
                                   const char* key_name) {
  check_type(d, key_name, rapidjson::kArrayType);
  rapidjson::Value& v = d[key_name];
  std::vector<double> vals;
  vals.reserve(v.Capacity());
  for (rapidjson::Value& elem : v.GetArray()) {
    if (!elem.IsDouble()) {
      throw json_semantic_error(
        "Array input of type " + kTypeNames[elem.GetType()] + "is not of type double");
    }
    vals.push_back(elem.GetDouble());
  }
  return vals;
}

std::vector<float> getFloatArray(rapidjson::Document& d, const char* key_name) {
  check_type(d, key_name, rapidjson::kArrayType);
  rapidjson::Value& v = d[key_name];
  std::vector<float> vals;
  vals.reserve(v.Capacity());
  for (rapidjson::Value& elem : v.GetArray()) {
    if (!elem.IsFloat()) {
      throw json_semantic_error(
        "Array input of type " + kTypeNames[elem.GetType()] + "is not of type float");
    }
    vals.push_back(elem.GetFloat());
  }
  return vals;
}

std::vector<int> getIntArray(rapidjson::Document& d, const char* key_name) {
  check_type(d, key_name, rapidjson::kArrayType);
  rapidjson::Value& v = d[key_name];
  std::vector<int> vals;
  vals.reserve(v.Capacity());
  for (rapidjson::Value& elem : v.GetArray()) {
    if (!elem.IsInt()) {
      throw json_semantic_error(
        "Array input of type " + kTypeNames[elem.GetType()] + "is not of type int");
    }
    vals.push_back(elem.GetInt());
  }
  return vals;
}

std::shared_ptr<Input> decode_input(InputType input_type,
                                    rapidjson::Document& d) {
  switch (input_type) {
    case InputType::Doubles: {
      std::vector<double> inputs = getDoubleArray(d, "input");
      return std::make_shared<clipper::DoubleVector>(inputs);
    }
    case InputType::Floats: {
      std::vector<float> inputs = getFloatArray(d, "input");
      return std::make_shared<clipper::FloatVector>(inputs);
    }
    case InputType::Ints: {
      std::vector<int> inputs = getIntArray(d, "input");
      return std::make_shared<clipper::IntVector>(inputs);
    }
    case InputType::Strings: {
      std::string input_string = getString(d, "input");
      return std::make_shared<clipper::SerializableString>(input_string);
    }
    case InputType::Bytes: {
      throw std::invalid_argument("Base64 encoded bytes are not supported yet");
    }
    default:
      throw std::invalid_argument("input_type is not a valid type");
  }
}

Output decode_output(OutputType output_type, rapidjson::Document& parsed_json) {
  std::string model_name = getString(parsed_json, "model_name");
  int model_version = getInt(parsed_json, "model_version");
  VersionedModelId versioned_model = std::make_pair(model_name, model_version);
  switch (output_type) {
    case OutputType::Double: {
      double y_hat = getDouble(parsed_json, "label");
      return Output(y_hat, versioned_model);
    }
    case OutputType::Int: {
      double y_hat = getInt(parsed_json, "label");
      return Output(y_hat, versioned_model);
    }
    default:
      throw std::invalid_argument("output_type is not a valid type");
  }
}

} // namespace clipper_json
