/** This is the redox code we need to add to query_processing.cpp. */
#include "redox.hpp"

using namespace std;
using redox::Redox;
using redox::Command;

int main() {
    redox::Redox rdx; // Initialize Redox
    if(!rdx.connect("localhost", 6379)) return 1;
    // The rest of the code is on trigger by recieving a query
    rdx.publish("query_rcvd", query_id);
    bool failed = true;
    while failed {
        rdx.command<vector<string>>({"LRANGE", query_id, "0", "-1"},
                                    [](Command<vector<string>>& c){
                                        if(c.ok()) {
                                            failed = false;
                                            for (const string& s : c.reply()) {
                                                // Add model id to vector that stores models to send to
                                            }
                                        }
                                    }
                                    );
    }
    rdx.commandSync<string>({"DELETE", query_id});
    // Processing to get model ids from model_ids string
    // Code to pass query to appropriate containers
    // results = array of results from model containers
    std::vector<string> cmd{"RPUSH", query_id};
    cmd.insert(
                cmd.end(),
                std::make_move_iterator(results.begin()),
                std::make_move_iterator(results.end())
                );
    rdx.commandSync<string>(cmd);
    rdx.publish("results_rcvd", query_id);
    auto& final_result_obj = rdx.commandSync<string>({"GET", std::to_string(query_id) + "_final"});
    while(!final_result_obj.ok()) {
        final_result_obj = rdx.commandSync<string>({"GET", std::to_string(query_id) + "_final"});
    }
    std::string final_result = final_result_obj.reply()
    // Pass final_result back upstream to client.
}
