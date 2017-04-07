#ifndef CLIPPER_LIB_JSON_UTIL_H
#define CLIPPER_LIB_JSON_UTIL_H

#include <stdexcept>

#include <boost/algorithm/string.hpp>

#include <unordered_map>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <clipper/datatypes.hpp>

using clipper::Input;
using clipper::InputType;
using clipper::Output;
using clipper::VersionedModelId;
using rapidjson::Type;

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
static std::vector<std::string> kTypeNames = {
    "Null", "False", "True", "Object", "Array", "String", "Number"};

namespace clipper {
namespace json {

class json_parse_error : public std::runtime_error {
 public:
  json_parse_error(const std::string& what) : std::runtime_error(what) {}
  ~json_parse_error() throw(){};
};

class json_semantic_error : public std::runtime_error {
 public:
  json_semantic_error(const std::string& what) : std::runtime_error(what) {}
  ~json_semantic_error() throw(){};
};

void check_document_is_object_and_key_exists(rapidjson::Value& d,
                                             const char* key_name) {
  if (!d.IsObject()) {
    throw json_semantic_error("Can only get key-value pair from an object");
  } else if (!d.HasMember(key_name)) {
    throw json_semantic_error("JSON object does not have required key: " +
                              std::string(key_name));
  }
}

/* Check for matching types else throw exception */
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

/* Check for matching types else throw exception */
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

/* Getters with error handling for double, float, long, int, string */
double get_double(rapidjson::Value& d, const char* key_name) {
  rapidjson::Value& v =
      check_kv_type_and_return(d, key_name, rapidjson::kNumberType);
  if (!v.IsDouble()) {
    throw json_semantic_error("Input of type " + kTypeNames[v.GetType()] +
                              " is not of type double");
  }
  return v.GetDouble();
}

float get_float(rapidjson::Value& d, const char* key_name) {
  rapidjson::Value& v =
      check_kv_type_and_return(d, key_name, rapidjson::kNumberType);
  if (!v.IsFloat()) {
    throw json_semantic_error("Input of type " + kTypeNames[v.GetType()] +
                              " is not of type float");
  }
  return v.GetFloat();
}

long get_long(rapidjson::Value& d, const char* key_name) {
  rapidjson::Value& v =
      check_kv_type_and_return(d, key_name, rapidjson::kNumberType);
  if (!v.IsInt64()) {
    throw json_semantic_error("Input of type " + kTypeNames[v.GetType()] +
                              " is not of type long");
  }
  return static_cast<long>(v.GetInt64());
}

int get_int(rapidjson::Value& d, const char* key_name) {
  rapidjson::Value& v =
      check_kv_type_and_return(d, key_name, rapidjson::kNumberType);
  if (!v.IsInt()) {
    throw json_semantic_error("Input of type " + kTypeNames[v.GetType()] +
                              " is not of type int");
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

bool get_bool(rapidjson::Value& d, const char* key_name) {
  rapidjson::Value& v = check_kv_type_is_bool_and_return(d, key_name);
  if (!v.IsBool()) {
    throw json_semantic_error("Input of type " + kTypeNames[v.GetType()] +
                              " is not of type bool");
  }
  return v.GetBool();
}

/* Getters with error handling for arrays of double, float, int, string */
std::vector<double> get_double_array(rapidjson::Value& d,
                                     const char* key_name) {
  rapidjson::Value& v =
      check_kv_type_and_return(d, key_name, rapidjson::kArrayType);
  std::vector<double> vals;
  vals.reserve(v.Capacity());
  for (rapidjson::Value& elem : v.GetArray()) {
    if (!elem.IsDouble()) {
      throw json_semantic_error("Array input of type " +
                                kTypeNames[elem.GetType()] +
                                " is not of type double");
    }
    vals.push_back(elem.GetDouble());
  }
  return vals;
}

std::vector<float> get_float_array(rapidjson::Value& d, const char* key_name) {
  rapidjson::Value& v =
      check_kv_type_and_return(d, key_name, rapidjson::kArrayType);
  std::vector<float> vals;
  vals.reserve(v.Capacity());
  for (rapidjson::Value& elem : v.GetArray()) {
    if (!elem.IsFloat()) {
      throw json_semantic_error("Array input of type " +
                                kTypeNames[elem.GetType()] +
                                " is not of type float");
    }
    vals.push_back(elem.GetFloat());
  }
  return vals;
}

std::vector<int> get_int_array(rapidjson::Value& d, const char* key_name) {
  rapidjson::Value& v =
      check_kv_type_and_return(d, key_name, rapidjson::kArrayType);
  std::vector<int> vals;
  vals.reserve(v.Capacity());
  for (rapidjson::Value& elem : v.GetArray()) {
    if (!elem.IsInt()) {
      throw json_semantic_error("Array input of type " +
                                kTypeNames[elem.GetType()] +
                                " is not of type int");
    }
    vals.push_back(elem.GetInt());
  }
  return vals;
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
    int model_version = get_int(elem, "model_version");
    candidate_models.push_back(std::make_pair(model_name, model_version));
  }
  return candidate_models;
}

rapidjson::Value& get_object(rapidjson::Value& d, const char* key_name) {
  rapidjson::Value& object =
      check_kv_type_and_return(d, key_name, rapidjson::kObjectType);
  return object;
}

rapidjson::Value& get_array(rapidjson::Value& d, const char* key_name) {
  rapidjson::Value& array =
      check_kv_type_and_return(d, key_name, rapidjson::kArrayType);
  return array;
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

std::shared_ptr<Input> parse_input(InputType input_type, rapidjson::Value& d) {
  switch (input_type) {
    case InputType::Doubles: {
      std::vector<double> inputs = get_double_array(d, "input");
      return std::make_shared<clipper::DoubleVector>(inputs);
    }
    case InputType::Floats: {
      std::vector<float> inputs = get_float_array(d, "input");
      return std::make_shared<clipper::FloatVector>(inputs);
    }
    case InputType::Ints: {
      std::vector<int> inputs = get_int_array(d, "input");
      return std::make_shared<clipper::IntVector>(inputs);
    }
    case InputType::Strings: {
      std::string input_string = get_string(d, "input");
      return std::make_shared<clipper::SerializableString>(input_string);
    }
    case InputType::Bytes: {
      throw std::invalid_argument("Base64 encoded bytes are not supported yet");
    }
    default: throw std::invalid_argument("input_type is not a valid type");
  }
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
  rapidjson::Value val_to_add(val.c_str(), d.GetAllocator());
  add_kv_pair(d, key_name, val_to_add);
}

void add_object(rapidjson::Document& d, const char* key_name,
                rapidjson::Document& to_add) {
  add_kv_pair(d, key_name, to_add);
}

/* Sets `d` to an array with the values in `string_vec` */
void set_string_array_doc(std::vector<std::string>& string_vec,
                          rapidjson::Document& d) {
  d.SetArray();

  size_t num_elements = string_vec.size();
  for (size_t i = 0; i < num_elements; i++) {
    rapidjson::Value v(rapidjson::StringRef(string_vec.at(i).c_str(),
                                            string_vec.at(i).length()));
    d.PushBack(v, d.GetAllocator());
  }
}

/* Sets `d` to an array containing info from `candidate_models_redis_format`*/
void set_candidate_models_doc(std::string& candidate_models_redis_format,
                              rapidjson::Document& d) {
  d.SetArray();

  std::vector<std::string> candidate_model_strings;
  boost::split(candidate_model_strings, candidate_models_redis_format,
               boost::is_any_of(","));

  std::vector<std::string> candidate_model_components;
  for (auto candidate_model_str : candidate_model_strings) {
    boost::split(candidate_model_components, candidate_model_str,
                 boost::is_any_of(":"));
    std::string model_name = candidate_model_components[0];
    int model_version = atoi(candidate_model_components[1].c_str());

    rapidjson::Document candidate_model_doc(&d.GetAllocator());
    candidate_model_doc.SetObject();
    clipper::json::add_string(candidate_model_doc, "model_name", model_name);
    clipper::json::add_int(candidate_model_doc, "model_version", model_version);
    d.PushBack(candidate_model_doc, d.GetAllocator());
  }
}

/* Sets `d` to an object containing reformatted info from `app_info`*/
void set_app_info_doc(std::unordered_map<std::string, std::string>& app_info,
                      rapidjson::Document& d) {
  d.SetObject();

  for (auto item : app_info) {
    std::string key = item.first;
    std::string value = item.second;
    if (key == "name" || key == "input_type" || key == "policy") {
      if (key == "policy") {
        // Converts the Redis storage key to the publicly facing label
        key = "selection_policy";
      }
      clipper::json::add_string(d, key.c_str(), value);
    } else if (key == "latency_slo_micros") {
      clipper::json::add_int(d, key.c_str(), atoi(value.c_str()));
    } else {
      // `item` corresponds to app's candidate_models. Need to convert the
      // Redis candidate_models storage format to the publicly facing format
      rapidjson::Document candidate_models_doc(&d.GetAllocator());
      set_candidate_models_doc(value, candidate_models_doc);
      clipper::json::add_object(d, "candidate_models", candidate_models_doc);
    }
  }
}

/* Sets `arr_doc` to an array of objects with info from`app_details` */
void set_app_info_array_doc(
    std::vector<std::unordered_map<std::string, std::string>>& app_details,
    rapidjson::Document& arr_doc) {
  arr_doc.SetArray();

  for (auto app_info : app_details) {
    rapidjson::Document d(&arr_doc.GetAllocator());
    set_app_info_doc(app_info, d);
    arr_doc.PushBack(d, arr_doc.GetAllocator());
  }
}

std::string to_json_string(rapidjson::Document& d) {
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  d.Accept(writer);
  return buffer.GetString();
}

}  // namespace json
}  // namespace clipper
#endif  // CLIPPER_LIB_JSON_UTIL_H
