#include <sstream>
#include <string>

#include <clipper/exceptions.hpp>

namespace clipper {

PredictError::PredictError(const std::string msg)
    : std::runtime_error(msg), msg_(msg) {}

const char *PredictError::what() const noexcept { return msg_.c_str(); }
}
