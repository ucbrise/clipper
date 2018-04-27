#include <sstream>
#include <string>

#include <clipper/exceptions.hpp>

namespace clipper {

PredictError::PredictError(const std::string msg)
    : std::runtime_error(msg), msg_(msg) {}

const char* PredictError::what() const noexcept { return msg_.c_str(); }

ManagementOperationError::ManagementOperationError(const std::string msg)
    : std::runtime_error(msg), msg_(msg) {}

const char* ManagementOperationError::what() const noexcept {
  return msg_.c_str();
}

ThreadPoolError::ThreadPoolError(const std::string msg)
    : std::runtime_error(msg), msg_(msg) {}

const char* ThreadPoolError::what() const noexcept { return msg_.c_str(); }

}  // namespace clipper
