#include <sstream>
#include <string>

#include <clipper/exceptions.hpp>

namespace clipper {

PredictError::PredictError(const std::string msg)
    : std::runtime_error(msg), msg_(msg) {}

const char* PredictError::what() const noexcept { return msg_.c_str(); }

NoModelsFoundError::NoModelsFoundError()
    : std::runtime_error("No models found for query") {}

const char* NoModelsFoundError::what() const noexcept {
  return "No models found for query";
}
}
