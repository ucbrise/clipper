#include <frontends/rest.hpp>

using clipper::QueryProcessor;

int main() {
  QueryProcessor qp;
  RequestHandler rh(qp, "0.0.0.0", 1337, 1);
  rh.start_listening();
}
