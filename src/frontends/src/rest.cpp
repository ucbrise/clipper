#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#define BOOST_THREAD_VERSION 4
#include <boost/thread.hpp>
#include <boost/thread/future.hpp>
#define BOOST_SPIRIT_THREADSAFE
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <clipper/datatypes.hpp>
#include <clipper/query_processor.hpp>

#include "server_http.hpp"

using namespace std;
using namespace clipper;
using namespace boost::property_tree; 
typedef SimpleWeb::Server<SimpleWeb::HTTP> HttpServer;

template <typename T>
std::vector<T> as_vector(ptree const& pt, ptree::key_type const& key) {
    std::vector<T> r;
    for (auto& item : pt.get_child(key))
        r.push_back(item.second.get_value<T>());
    return r;
}

class RequestHandler {
    HttpServer server;
    public:
        RequestHandler(QueryProcessor& q, int portno, int num_threads) : server(portno, num_threads) {
            server.resource["^/predict$"]["POST"]=[&q](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
                try {
                    ptree pt;
                    read_json(request->content, pt);

                    long uid = pt.get<long>("uid");
                    vector<double> inputs = as_vector<double>(pt, "input");

                    std::shared_ptr<Input> input =
                        std::make_shared<DoubleVector>(inputs);
                    auto prediction = q.predict(
                        {"test", uid, input, 20000, "newest_model", {std::make_pair("m", 1)}});
                    prediction.then([response](boost::future<Response> f){
                        Response r = f.get();
                        stringstream ss; 
                        ss << "qid:" << r.query_id_ << " predict:" << r.output_->y_hat_;
                        string content = ss.str();
                        *response << "HTTP/1.1 200 OK\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
                    });
                } catch (exception&e) {
                    *response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << strlen(e.what()) << "\r\n\r\n" << e.what();
                }
            };

            server.resource["^/update$"]["POST"]=[&q](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
                try {
                    ptree pt;
                    read_json(request->content, pt);

                    long uid = pt.get<long>("uid");
                    vector<double> inputs = as_vector<double>(pt, "input");

                    std::shared_ptr<Input> input =
                        std::make_shared<DoubleVector>(inputs);
                    std::shared_ptr<Output> output = 
                        std::make_shared<Output>(Output(10.0, "model"));
                    auto update = q.update(
                        {"label", uid, {std::make_pair(input, output)}, "newest_model", {std::make_pair("m", 1)}});
                    update.then([response](boost::future<FeedbackAck> f){
                        FeedbackAck ack = f.get();
                        stringstream ss;
                        ss << "Feedback received? " << ack;
                        string content = ss.str();
                        *response << "HTTP/1.1 200 OK\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
                    });
                } catch (exception&e) {
                    *response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << strlen(e.what()) << "\r\n\r\n" << e.what();
                }
            };
        }

        void start_listening() {
            HttpServer& s = server;
            thread server_thread([&s](){
                s.start();
            });

            server_thread.join();
        }
};

int main() {
  QueryProcessor qp;
  RequestHandler rh(qp, 1337, 8);
  rh.start_listening();
}
