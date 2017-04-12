#ifndef CLIPPER_EXCEPTIONS_HPP
#define CLIPPER_EXCEPTIONS_HPP

#include <stdexcept>

namespace clipper {

class PredictError : public std::runtime_error {
 public:
  PredictError(const long query_id, const std::string msg);

  const char *what() const noexcept;

 private:
  const long query_id_;
  const std::string msg_;
};

} // namespace clipper

#endif //CLIPPER_EXCEPTIONS_HPP
