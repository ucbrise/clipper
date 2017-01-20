#include <stdexcept>
#include <rapidjson/document.h>
#include <clipper/datatypes.hpp>

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
