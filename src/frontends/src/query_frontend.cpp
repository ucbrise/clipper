
#include "query_frontend.hpp"
#include <clipper/query_processor.hpp>

using clipper::QueryProcessor;

int main() {
  QueryProcessor qp;
  RequestHandler<QueryProcessor> rh(std::move(qp), "0.0.0.0", 1337, 1);
  rh.start_listening();
}
