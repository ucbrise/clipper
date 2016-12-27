
#include "query_frontend.hpp"
#include <clipper/query_processor.hpp>

int main() {
  query_frontend::RequestHandler<clipper::QueryProcessor> rh("0.0.0.0", 1337,
                                                             1);
  rh.start_listening();
}
