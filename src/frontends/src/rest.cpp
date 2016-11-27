#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <boost/thread.hpp>
#define BOOST_SPIRIT_THREADSAFE
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

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
using clipper::VersionedModelId;
using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;

enum InputType { integer_vec, double_vec, byte_vec, float_vec };
enum OutputType { double_val, int_val };

template <typename T>
std::vector<T> as_vector(ptree const& pt, ptree::key_type const& key) {
  std::vector<T> r;
  for (auto& item : pt.get_child(key)) r.push_back(item.second.get_value<T>());
  return r;
}

std::vector<VersionedModelId> parse_model_json(ptree const& pt, ptree::key_type const& key) {
    std::vector<VersionedModelId> r;
    for (auto& model_pair : pt.get_child(key)) {
        // for (auto& item: model_pair.second()) {
            std::string model_name = model_pair.first;
            int model_version = model_pair.second.get_value<int>();
            r.push_back(std::make_pair(model_name, model_version));
        // }
    }
    return r;
}

std::shared_ptr<Input> decode_input(InputType input_type, ptree& parsed_json) {
    std::shared_ptr<Input> result;
    switch(input_type) {
        case double_vec:
          {
            std::vector<double> inputs = as_vector<double>(parsed_json, "input");
            return std::make_shared<DoubleVector>(inputs);
          }
        default:
            throw std::invalid_argument("Not a valid type");
    }
}

std::shared_ptr<Output> decode_output(OutputType output_type, ptree parsed_json) {
    if (output_type == int_val) { // no use for output_type right now, mainly to pass compiler check
        std::cout << "output type is int_val";
    }
    double y_hat = parsed_json.get<double>("label");
    std::string versioned_model = parsed_json.get<std::string>("model_version");
    return std::make_shared<Output>(Output(y_hat, versioned_model));
}

class RequestHandler {
    public:
        RequestHandler(QueryProcessor& q, int portno, int num_threads) : server(portno, num_threads), qp(q) {}
        RequestHandler(QueryProcessor& q, std::string address, int portno, int num_threads) : server(address, portno, num_threads), qp(q) {}

        void add_application(std::string name, std::vector<VersionedModelId> models,
                             InputType input_type, OutputType output_type, std::string policy, long latency) {
            QueryProcessor& q = qp;
            /* TODO: Error handling for JSON parsing */
            auto predict_fn = [&q,name,input_type,policy,latency,models](std::shared_ptr<HttpServer::Response> response,
                                                                         std::shared_ptr<HttpServer::Request> request) {
                ptree pt;
                read_json(request->content, pt);

                long uid = pt.get<long>("uid");
                std::shared_ptr<Input> input = decode_input(input_type, pt);
                auto prediction = q.predict(
                    {name, uid, input, latency, policy, models});
                prediction.then([response](boost::future<Response> f){
                    Response r = f.get();
                    std::stringstream ss;
                    ss << "qid:" << r.query_id_ << " predict:" << r.output_->y_hat_;
                    std::string content = ss.str();
                    *response << "HTTP/1.1 200 OK\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
                });
            };
            std::string predict_endpoint = "^/" + name + "/predict$";
            server.add_endpoint(predict_endpoint, "POST", predict_fn);
            std::cout << "added endpoint (" + predict_endpoint + ") for " + name + "\n";

            auto update_fn = [&q,name,input_type,output_type,policy,latency,models](std::shared_ptr<HttpServer::Response> response,
                                                                                    std::shared_ptr<HttpServer::Request> request) {
                ptree pt;
                read_json(request->content, pt);

                long uid = pt.get<long>("uid");
                std::shared_ptr<Input> input = decode_input(input_type, pt);
                std::shared_ptr<Output> output = decode_output(output_type, pt);
                auto update = q.update(
                    {name, uid, {std::make_pair(input, output)}, policy, models});
                update.then([response](boost::future<FeedbackAck> f){
                    FeedbackAck ack = f.get();
                    std::stringstream ss;
                    ss << "Feedback received? " << ack;
                    std::string content = ss.str();
                    *response << "HTTP/1.1 200 OK\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
                });
            };
            std::string update_endpoint = "^/" + name + "/update$";
            server.add_endpoint(update_endpoint, "POST", update_fn);
            std::cout << "added endpoint (" + update_endpoint + ") for " + name + "\n";
        }

        void add_endpoint(std::string endpoint, std::string request_method,
                          std::function<void(std::shared_ptr<HttpServer::Response>,
                                             std::shared_ptr<HttpServer::Request>)> endpoint_fn) {
            server.resource[endpoint][request_method] = endpoint_fn;
            std::cout << "added " + endpoint + "\n";
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

/* Assume an input json of the following form:
 * {"name": <new endpoint name>, "models": <array of {"<model_name>": <model_version>} pairs>,
 *  "input_type": <input type string>, "output_type": <output type string>,
 *  "policy": <policy name>, "latency": <latency SLO>}
 */
void application_endpoint(std::shared_ptr<HttpServer::Response> response,
                          std::shared_ptr<HttpServer::Request> request,
                          RequestHandler& rh) {
    ptree pt;
    read_json(request->content, pt);

    std::string name = pt.get<std::string>("name");
    std::vector<VersionedModelId> models = parse_model_json(pt, "models");
    std::string policy = pt.get<std::string>("policy");
    long latency = pt.get<long>("latency");
    std::string input_type_name = pt.get<std::string>("input_type");
    std::string output_type_name = pt.get<std::string>("output_type");
    InputType input_type;
    OutputType output_type = double_val; /* currently set output_type to double by default */
    if (!input_type_name.compare("double_vec")) {
        input_type = double_vec;
    } else if (!input_type_name.compare("integer_vec")) {
        input_type = integer_vec;
    } else {
        throw std::invalid_argument(input_type_name + " is not a valid input type");
    }

    rh.add_application(name, models, input_type, output_type, policy, latency);
    std::string response_string = "Successfully added endpoint: " + name + "\n";
    *response << "HTTP/1.1 200 OK\r\nContent-Length: " << response_string.length() << "\r\n\r\n" << response_string;
}

int main() {
  QueryProcessor qp;
  RequestHandler rh(qp, "0.0.0.0", 1337, 1);

  /* Add function for admin */
  auto application_fn = [&rh](std::shared_ptr<HttpServer::Response> response,
                              std::shared_ptr<HttpServer::Request> request) {
    application_endpoint(response, request, rh);
  };
  rh.add_endpoint("^/admin$", "POST", application_fn);
  rh.start_listening();
}
