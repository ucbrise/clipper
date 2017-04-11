#ifndef CLIPPER_EXCEPTIONS_HPP
#define CLIPPER_EXCEPTIONS_HPP

#include <stdexcept>
#include <string>
#include <sstream>

namespace clipper {

class PredictError : public std::runtime_error {
 public:
  PredictError(const long query_id, const std::string msg)
      : std::runtime_error(msg), query_id_(query_id), msg_(msg) {};

  const char *what() const noexcept {
    std::stringstream ss;
    ss << "Failed to render a prediction for query with id " << query_id_
       << std::endl;
    ss << "Explanation: " << msg_ << std::endl;
    return ss.str().data();
  }

 private:
  const long query_id_;
  const std::string msg_;
};

} // namespace clipper

#endif //CLIPPER_EXCEPTIONS_HPP
