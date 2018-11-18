#include <stdio.h>
#include <curl/curl.h>
#include "include/rapidjson/writer.h"
#include "include/rapidjson/stringbuffer.h"
#include <chrono>
#include <iostream>
#include <string> 
#include <chrono>
#include <vector>
#include <unistd.h>

using namespace rapidjson;

size_t WriteCallback(char *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::vector<int> generate_ms() {
    auto hnow = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(hnow.time_since_epoch()) -
              std::chrono::duration_cast<std::chrono::seconds>(hnow.time_since_epoch());

    std::vector<int> v;
    auto n = ms.count();
    for(; n; n/=10)
      v.push_back(n%10);
    
    while (v.size() < 3)
        v.push_back(0);

    return v;
}

int main(void) 
{
    CURL *curl;
    CURLcode res;
    
    /* In windows, this will init the winsock stuff */ 
    curl_global_init(CURL_GLOBAL_ALL);

    /* get a curl handle */ 
    curl = curl_easy_init();

    if (curl) {
        /* First set the URL that is about to receive our POST. This URL can
        just as well be a https:// URL if that is what should receive the
        data. */ 
        curl_easy_setopt(curl, CURLOPT_URL, \
                        "http://localhost:8501/v1/models/model:predict");

        std::string readBuffer;

        /* Keep sending queries to the model server */
        /* Condition? While model server status is okay. */
       while (true) {
            /* Generate random input query, [x1, x2, x3].T */
            // std::string json_query = make_query();
                /* Generate random input numbers */
            std::vector<int> v = generate_ms();

            /* Serialize input query */
            StringBuffer s;
            Writer<StringBuffer> writer(s);
            writer.StartObject();
            writer.Key("instances");
            writer.StartArray();
            for (auto i = v.rbegin(); i != v.rend(); i++) {
                writer.StartArray();
                writer.Uint(*i);
                writer.EndArray();
            }
            writer.EndArray();
            writer.EndObject();
           
            /* use curl to send POST, following tfs REST API */
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, s.GetString());

             /* Now specify we want to POST data */ 
            curl_easy_setopt(curl, CURLOPT_POST, 1L);

            /* send all data to this function  */ 
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);

            /* we pass our the string 'readBuffer' to the callback function */ 
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
           
            /* get result */ 
            res = curl_easy_perform(curl);

            /* Verify to see if the connection is indeed persistent */
            // long connects;
            // long redirects;
            // res = curl_easy_getinfo(curl, CURLINFO_NUM_CONNECTS, &connects);curl_easy_getinfo(curl, CURLINFO_REDIRECT_COUNT, &redirects);
            // printf("It needed %d connects\n", (int)connects);
            // printf("Number of redirects: %d \n", (int)redirects);

            /* Sleep */
            usleep(5000 * 1000);

            /* Show the result */
            // std::cout << "Input Query: " << s.GetString() << std::endl;
            std::cout << readBuffer << std::endl;
       }

    /* cleanup curl stuff */ 
    curl_easy_cleanup(curl);
    /* we're done with libcurl, so clean it up */ 
    curl_global_cleanup();
    }
}
