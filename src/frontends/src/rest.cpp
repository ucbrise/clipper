#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <boost/thread.hpp>
#define BOOST_SPIRIT_THREADSAFE
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <clipper/datatypes.hpp>
#include <clipper/query_processor.hpp>

#include <server_http.hpp>

using namespace boost::property_tree; 
using clipper::DoubleVector;
using clipper::FeedbackAck;
using clipper::Input;
using clipper::Output;
using clipper::QueryProcessor;
using clipper::Response;
using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;

template <typename T>
std::vector<T> as_vector(ptree const& pt, ptree::key_type const& key) {
    std::vector<T> r;
    for (auto& item : pt.get_child(key))
        r.push_back(item.second.get_value<T>());
    return r;
}

class RequestHandler {
    public:
        RequestHandler(QueryProcessor& q, int portno, int num_threads) : server(portno, num_threads), qp(q) {}

        void add_endpoint(std::string endpoint,
                          std::function<void(std::shared_ptr<HttpServer::Response>,
                                        std::shared_ptr<HttpServer::Request>,
                                        QueryProcessor&)> endpoint_fn) {
            QueryProcessor& q = qp;
            server.resource[endpoint]["POST"]=[&q,endpoint_fn](std::shared_ptr<HttpServer::Response> response,
                                                   std::shared_ptr<HttpServer::Request> request) {
                endpoint_fn(response, request, q);
            };
        }

        void start_listening() {
            HttpServer& s = server;
            std::thread server_thread([&s](){
                s.start();
            });

            server_thread.join();
        }
    private:
        HttpServer server;
        QueryProcessor& qp;
};

int main() {
  QueryProcessor qp;
  RequestHandler rh(qp, 1337, 8);
  /* Define predict/update functions, which may be added after initialization */
  auto predict_fn = [](std::shared_ptr<HttpServer::Response> response,
                       std::shared_ptr<HttpServer::Request> request,
                       QueryProcessor& q) {
       try {
           ptree pt;
           read_json(request->content, pt);

           long uid = pt.get<long>("uid");
           std::vector<double> inputs = as_vector<double>(pt, "input");

           std::shared_ptr<Input> input =
               std::make_shared<DoubleVector>(inputs);
           auto prediction = q.predict(
               {"test", uid, input, 20000, "newest_model", {std::make_pair("m", 1)}});
           prediction.then([response](boost::future<Response> f){
               Response r = f.get();
               std::stringstream ss;
               ss << "qid:" << r.query_id_ << " predict:" << r.output_.y_hat_;
               std::string content = ss.str();
               *response << "HTTP/1.1 200 OK\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
           });
       } catch (std::exception&e) {
           *response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << strlen(e.what()) << "\r\n\r\n" << e.what();
       }
  };

  auto update_fn = [](std::shared_ptr<HttpServer::Response> response,
                      std::shared_ptr<HttpServer::Request> request,
                      QueryProcessor& q) {
      try {
          ptree pt;
          read_json(request->content, pt);

          long uid = pt.get<long>("uid");
          std::vector<double> inputs = as_vector<double>(pt, "input");
          float y = pt.get<long>("y");
          ptree vm = pt.get_child("model");
        
//          std::string model = pt.get<std::string>("model");
//          int version = pt.get<int>("version");
          

          std::shared_ptr<Input> input =
              std::make_shared<DoubleVector>(inputs);
          Output output{y, std::make_pair(vm.get<std::string>("name"),vm.get<int>("version"))};
          auto update = q.update(
              {"label", uid, {std::make_pair(input, output)}, "newest_model", {std::make_pair("m", 1)}});
          update.then([response](boost::future<FeedbackAck> f){
              FeedbackAck ack = f.get();
              std::stringstream ss;
              ss << "Feedback received? " << ack;
              std::string content = ss.str();
              *response << "HTTP/1.1 200 OK\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
          });
      } catch (std::exception&e) {
          *response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << strlen(e.what()) << "\r\n\r\n" << e.what();
      }
  };
  rh.add_endpoint("^/predict$", predict_fn);
  rh.add_endpoint("^/update$", update_fn);
  rh.start_listening();
}
