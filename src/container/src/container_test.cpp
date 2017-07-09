#include <container/container_rpc.hpp>
#include <container/container_parsing.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/logging.hpp>

using namespace clipper::container;

class TestModel : public Model<clipper::DoubleVector> {
  public:
    std::vector<std::string> predict(const std::vector<std::shared_ptr<clipper::DoubleVector>> inputs) const {
      clipper::log_info_formatted(LOGGING_TAG_CONTAINER, "RECEIVED {} INPUTS", inputs.size());
      auto first_input = inputs[0];
      for(auto const& elem : first_input->get_data()) {
        clipper::log_info_formatted(LOGGING_TAG_CONTAINER, "{}", elem);
      }
      std::vector<std::string> outputs;
      for(auto const& elem : inputs) {
        outputs.push_back("GAH");
      }
      return outputs;
    }

    InputType get_input_type() const {
      return InputType::Doubles;
    }
};

int main(int argc, char* argv[]) {
  RPC container_rpc;
  //RPCTestModel test_model(container_rpc);
  TestModel test_model;
  std::string model_name = "cpp_test";
  int model_version = 1;
  DoubleVectorParser parser;


  std::string clipper_ip = "localhost";
  int clipper_port = 7000;

  container_rpc.start(test_model, model_name, model_version, parser, clipper_ip, clipper_port);
  while(true);
}