#include <iostream>
#include <string>
#include <vector>

#include <boost/thread.hpp>
#define BOOST_SPIRIT_THREADSAFE
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/query_processor.hpp>
#include <server_http.hpp>

using namespace boost::property_tree; 
using clipper::VersionedModelId;
using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;

enum InputType { integer_vec, double_vec, byte_vec, float_vec };
enum OutputType { double_val, int_val };

std::vector<VersionedModelId> parse_model_json(ptree const& pt, ptree::key_type const& key) {
    std::vector<VersionedModelId> r;
    for (auto& entry: pt.get_child(key)) {
        for (auto& model_pair: entry.second) {
            std::string model_name = model_pair.first;
            int model_version = model_pair.second.get_value<int>();
            r.push_back(std::make_pair(model_name, model_version));
        }
    }
    return r;
}

class AdminServer {
    public:
        AdminServer(int portno, int num_threads) : server(portno, num_threads) {}
        AdminServer(std::string address, int portno, int num_threads) : server(address, portno, num_threads) {}

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
};

/* Assume an input json of the following form:
 * {"name": <new endpoint name>, "models": <array of {"<model_name>": <model_version>} pairs>,
 *  "input_type": <input type string>, "output_type": <output type string>,
 *  "policy": <policy name>, "latency": <latency SLO>}
 */
void application_endpoint(std::shared_ptr<HttpServer::Response> response,
                          std::shared_ptr<HttpServer::Request> request,
                          RequestHandler& rh) {
    try {
        ptree pt;
        read_json(request->content, pt);

        std::string name = pt.get<std::string>("name");
        std::vector<VersionedModelId> models = parse_model_json(pt, "models");
        std::string policy = pt.get<std::string>("policy");
        long latency = pt.get<long>("latency");
        std::string input_type_name = pt.get<std::string>("input_type");
        std::string output_type_name = pt.get<std::string>("output_type");
        InputType input_type;
        OutputType output_type = output_type;
        if (!input_type_name.compare("double_vec")) {
            input_type = double_vec;
        } else {
            throw std::invalid_argument(input_type_name + " is not a valid input type");
        }

        if (!output_type_name.compare("double_val")) {
            output_type = double_val;
        } else {
            throw std::invalid_argument(output_type_name + " is not a valid output type");
        }

        /* Specifically, mock out this call */
        rh.add_application(name, models, input_type, output_type, policy, latency);
        std::string response_string = "Successfully added endpoint: " + name + "\n";
        *response << "HTTP/1.1 200 OK\r\nContent-Length: " << response_string.length() << "\r\n\r\n" << response_string;
    } catch (const ptree_error &e) {
        *response << "HTTP/1.1 200 OK\r\nContent-Length: " << std::strlen(e.what()) << "\r\n\r\n" << e.what() << "\n";
    } catch (const std::invalid_argument &e) {
        *response << "HTTP/1.1 200 OK\r\nContent-Length: " << std::strlen(e.what()) << "\r\n\r\n" << e.what() << "\n";
    }
}

int main() {
  AdminServer admin_server("0.0.0.0", 1337, 1);
  /* Mock out RequestHandler, for testing */
  /* Add function for admin */
  auto application_fn = [&rh](std::shared_ptr<HttpServer::Response> response,
                              std::shared_ptr<HttpServer::Request> request) {
    application_endpoint(response, request, rh);
  };
  admin_server.add_endpoint("^/admin$", "POST", application_fn);
  admin_server.start_listening();
}
