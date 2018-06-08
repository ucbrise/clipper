/** This is the redox code we need to add to query_processing.cpp.
 Need to check if redox supports list actions. */
#include "redox.hpp"

using namespace std;
using redox::Redox;
using redox::Command;

int main() {
    redox::Redox rdx; // Initialize Redox
    if(!rdx.connect("localhost", 6379)) return 1;
    // The rest of the code is on trigger by recieving a query
    rdx.publish("query_rcvd", query_id);
    auto& model_ids = rdx.commandSync<string>({"LRANGE 0 -1", query_id});
    while(!model_ids.ok()) {
        model_ids = rdx.commandSync<string>({"LRANGE 0 -1", query_id});
    }
    rdx.commandSync<string>({"DELETE", query_id});
    // Processing to get model ids from model_ids string
    // Code to pass query to appropriate containers
    // results = array of results from model containers
    for (const string&result : results) {
        rdx.commandSync<string>({"RPUSH", query_id, result}); // Need to check if redox supports this.
    }
    rdx.publish("results_rcvd", query_id);
    auto& final_result_obj = rdx.commandSync<string>({"GET", std::to_string(query_id) + "_final"});
    while(!final_result_obj.ok()) {
        final_result_obj = rdx.commandSync<string>({"GET", std::to_string(query_id) + "_final"});
    }
    std::string final_result = final_result_obj.reply()
    // Pass final_result back upstream to client.
}
