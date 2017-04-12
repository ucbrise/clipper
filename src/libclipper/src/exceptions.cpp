#include <sstream>
#include <string>

#include <clipper/exceptions.hpp>

namespace clipper {

PredictError::PredictError(const long query_id, const std::string msg)
    : std::runtime_error(msg), query_id_(query_id), msg_(msg){};

const char *PredictError::what() const noexcept {
  std::stringstream ss;
  ss << "Failed to render a prediction for query with id " << query_id_
     << std::endl;
  ss << "Explanation: " << msg_ << std::endl;
  return ss.str().data();
}
}