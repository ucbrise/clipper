#ifndef CLIPPER_EXCEPTIONS_HPP
#define CLIPPER_EXCEPTIONS_HPP

#include <stdexcept>

namespace clipper {

class PredictError : public std::runtime_error {
 public:
  PredictError(const std::string msg);

  const char *what() const noexcept;

 private:
  const std::string msg_;
};

class ManagementOperationError : public std::runtime_error {
 public:
  ManagementOperationError(const std::string msg);

  const char *what() const noexcept;

 private:
  const std::string msg_;
};

class ThreadPoolError : public std::runtime_error {
 public:
  ThreadPoolError(const std::string msg);

  const char *what() const noexcept;

 private:
  const std::string msg_;
};

}  // namespace clipper

#endif  // CLIPPER_EXCEPTIONS_HPP
