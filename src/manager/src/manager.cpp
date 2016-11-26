#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

// #include <boost/thread.hpp>

#include <clipper/constants.hpp>
#include <clipper/datatypes.hpp>
#include <redox.hpp>

// #include <clipper/datatypes.hpp>
// #include <clipper/query_processor.hpp>

bool send_sync_cmd(std::shared_ptr<redox::Redox> redis,
                   std::vector<std::string> cmd_vec) {
  bool ok = true;
  redox::Command<std::string>& cmd = redis->commandSync<std::string>(cmd_vec);
  if (!cmd.ok()) {
    std::cerr << "Error with command \"" << redis->vecToStr(cmd_vec)
              << "\": " << cmd.lastError() << std::endl;
    ok = false;
  } else {
    std::cout << "Successfully issued command \"" << redis->vecToStr(cmd_vec)
              << "\": " << std::endl;
  }
  cmd.free();
  return ok;
}

std::vector<std::string> get_model(std::shared_ptr<redox::Redox> redis,
                                   const std::string& key) {
  std::vector<std::string> v;
  std::vector<std::string> cmd_vec = {"HGETALL", key};
  redox::Command<std::vector<std::string>>& cmd =
      redis->commandSync<std::vector<std::string>>(cmd_vec);
  if (!cmd.ok()) {
    std::cerr << "Error with command \"" << redis->vecToStr(cmd_vec)
              << "\": " << cmd.lastError() << std::endl;
  } else {
    std::cout << "Successfully issued command \"" << redis->vecToStr(cmd_vec)
              << "\": " << std::endl;
    v = cmd.reply();
  }
  cmd.free();
  return v;
}

bool add_model(std::shared_ptr<redox::Redox> redis, std::string name,
               int version) {
  size_t model_id =
      clipper::versioned_model_hash(std::make_pair(name, version));

  return send_sync_cmd(redis, {"HMSET", std::to_string(model_id), "model_name",
                               name, "model_version", std::to_string(version),
                               "load", std::to_string(0)});
}

int main() {
  std::shared_ptr<redox::Redox> redis = std::make_shared<redox::Redox>();
  redis->connect("localhost", clipper::REDIS_PORT);
  send_sync_cmd(redis, {"SELECT", std::to_string(clipper::REDIS_MODEL_DB_NUM)});
  // set up keyspace notifications
  send_sync_cmd(redis, {"CONFIG", "SET", "notify-keyspace-events", "AK"});

  redox::Subscriber keyspace_sub;
  keyspace_sub.connect("localhost", clipper::REDIS_PORT);
  std::ostringstream subscription;
  subscription << "__keyspace@" << std::to_string(clipper::REDIS_MODEL_DB_NUM)
               << "__:*";
  // subscription << "__key*__:*";
  std::string sub_str = subscription.str();
  std::cout << "Subscription string: " << sub_str << std::endl;
  keyspace_sub.psubscribe(
      sub_str, [redis](const std::string& topic, const std::string& msg) {
        std::cout << "TOPIC: " << topic << ", MESSAGE: " << msg << std::endl;
        size_t split_idx = topic.find_first_of(":");
        std::string key = topic.substr(split_idx + 1);
        auto model_data = get_model(redis, key);
        std::unordered_map<std::string, std::string> model_map;
        for (auto m = model_data.begin(); m != model_data.end(); ++m) {
          model_map[*m] = *(++m);
        }
        std::cout << "FOUND MODEL: {" << std::endl;
        for (auto m : model_map) {
          std::cout << "\t" << m.first << ": " << m.second << std::endl;
        }
        std::cout << "}" << std::endl;
      });
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  add_model(redis, "svm", 1);
  std::this_thread::sleep_for(std::chrono::milliseconds(3000));
}
