#include <stdexcept>

#include <boost/thread.hpp>
#define BOOST_SPIRIT_THREADSAFE
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <rapidjson/document.h>

#include <clipper/datatypes.hpp>


using namespace boost::property_tree;
using clipper::Input;
using clipper::InputType;
using clipper::Output;
using clipper::OutputType;
using clipper::VersionedModelId;

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

/*
template <typename T>
std::vector<T> as_vector(ptree const& pt, ptree::key_type const& key) {
  std::vector<T> r;
  for (auto& item : pt.get_child(key)) {
    r.push_back(item.second.get_value<T>());
  }
  return r;
}

std::shared_ptr<Input> decode_input(InputType input_type, ptree& parsed_json) {
  switch (input_type) {
    case InputType::Doubles: {
      std::vector<double> inputs = as_vector<double>(parsed_json, "input");
      return std::make_shared<clipper::DoubleVector>(inputs);
    }
    case InputType::Floats: {
      std::vector<float> inputs = as_vector<float>(parsed_json, "input");
      return std::make_shared<clipper::FloatVector>(inputs);
    }
    case InputType::Ints: {
      std::vector<int> inputs = as_vector<int>(parsed_json, "input");
      return std::make_shared<clipper::IntVector>(inputs);
    }
    case InputType::Strings: {
      std::string input_string =
          parsed_json.get_child("input").get_value<std::string>();
      return std::make_shared<clipper::SerializableString>(input_string);
    }
    case InputType::Bytes: {
      throw std::invalid_argument("Base64 encoded bytes are not supported yet");
    }
    default:
      throw std::invalid_argument("input_type is not a valid type");
  }
}

Output decode_output(OutputType output_type, ptree parsed_json) {
  std::string model_name = parsed_json.get<std::string>("model_name");
  int model_version = parsed_json.get<int>("model_version");
  VersionedModelId versioned_model = std::make_pair(model_name, model_version);
  switch (output_type) {
    case OutputType::Double: {
      double y_hat = parsed_json.get<double>("label");
      return Output(y_hat, versioned_model);
    }
    case OutputType::Int: {
      double y_hat = parsed_json.get<int>("label");
      return Output(y_hat, versioned_model);
    }
    default:
      throw std::invalid_argument("output_type is not a valid type");
  }
} */

std::shared_ptr<Input> decode_input(InputType input_type,
                                    rapidjson::Document& d) {
  switch (input_type) {
    case InputType::Doubles: {
      std::vector<double> inputs;
      inputs.reserve(d["input"].Capacity());
      for (rapidjson::Value& elem : d["input"].GetArray()) {
        inputs.push_back(elem.GetDouble());
      }
      return std::make_shared<clipper::DoubleVector>(inputs);
    }
    case InputType::Floats: {
      std::vector<float> inputs;
      inputs.reserve(d["input"].Capacity());
      for (rapidjson::Value& elem : d["input"].GetArray()) {
        inputs.push_back(elem.GetFloat());
      }
      return std::make_shared<clipper::FloatVector>(inputs);
    }
    case InputType::Ints: {
      std::vector<int> inputs;
      inputs.reserve(d["input"].Capacity());
      for (rapidjson::Value& elem : d["input"].GetArray()) {
        inputs.push_back(elem.GetInt());
      }
      return std::make_shared<clipper::IntVector>(inputs);
    }
    case InputType::Strings: {
      std::string input_string = d["input"].GetString();
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
  std::string model_name = std::string(parsed_json["model_name"].GetString());
  int model_version = parsed_json["model_version"].GetInt();
  VersionedModelId versioned_model = std::make_pair(model_name, model_version);
  switch (output_type) {
    case OutputType::Double: {
      double y_hat = parsed_json["label"].GetDouble();
      return Output(y_hat, versioned_model);
    }
    case OutputType::Int: {
      double y_hat = parsed_json["label"].GetInt();
      return Output(y_hat, versioned_model);
    }
    default:
      throw std::invalid_argument("output_type is not a valid type");
  }
}

} // namespace clipper_json
