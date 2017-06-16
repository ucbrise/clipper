#include <clipper/json_util.hpp>
#include <clipper/logging.hpp>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>

#include "include/bench_utils.hpp"

using namespace clipper;

namespace bench_utils {

const std::string CIFAR_DATA_PATH_PROMPT =
    "Enter a path to the CIFAR10 binary data set: ";
const std::string NUM_THREADS_PROMPT =
    "Enter the number of threads of execution: ";
const std::string NUM_BATCHES_PROMPT =
    "Enter the number of request batches to be sent by each thread: ";
const std::string BATCH_SIZE_PROMPT =
    "Enter the number of requests per batch: ";
const std::string BATCH_DELAY_MILLIS_PROMPT =
    "Enter the delay between batches, in milliseconds: ";
const std::string BATCH_DELAY_MICROS_PROMPT =
    "Enter the delay between batches, in microseconds: ";
const std::string LATENCY_OBJECTIVE_PROMPT =
    "Enter the latency objective, in microseconds: ";
const std::string REPORT_DELAY_SECONDS_PROMPT =
    "Enter the delay between reporting metrics, in seconds: ";
const std::string REPORTS_PATH_PROMPT =
    "Enter the path to the file for contain benchmark reports: ";
const std::string REPORTS_PATH_VERBOSE_PROMPT =
    "Enter the path to the file for contain verbose benchmark reports: ";
const std::string POISSON_DELAY_PROMPT =
    "Enter \"true\" if you want the delay between request batches to be drawn "
    "from a "
    "poisson distribution: ";
const std::string MODEL_NAME_PROMPT =
    "Enter the name of the model you want queried: ";
const std::string MODEL_VERSION_PROMPT =
    "Enter the version of the model you want queried: ";

std::string _get_prompt(std::string var) {
  if (var == CIFAR_DATA_PATH) {
    return CIFAR_DATA_PATH_PROMPT;
  } else if (var == NUM_THREADS) {
    return NUM_THREADS_PROMPT;
  } else if (var == NUM_BATCHES) {
    return NUM_BATCHES_PROMPT;
  } else if (var == BATCH_SIZE) {
    return BATCH_SIZE_PROMPT;
  } else if (var == BATCH_DELAY_MILLIS) {
    return BATCH_DELAY_MILLIS_PROMPT;
  } else if (var == LATENCY_OBJECTIVE) {
    return LATENCY_OBJECTIVE_PROMPT;
  } else if (var == REPORT_DELAY_SECONDS) {
    return REPORT_DELAY_SECONDS_PROMPT;
  } else if (var == REPORTS_PATH) {
    return REPORTS_PATH_PROMPT;
  } else if (var == REPORTS_PATH_VERBOSE) {
    return REPORTS_PATH_VERBOSE_PROMPT;
  } else if (var == BATCH_DELAY_MICROS) {
    return BATCH_DELAY_MICROS_PROMPT;
  } else if (var == POISSON_DELAY) {
    return POISSON_DELAY_PROMPT;
  } else if (var == MODEL_NAME) {
    return MODEL_NAME_PROMPT;
  } else if (var == MODEL_VERSION) {
    return MODEL_VERSION_PROMPT;
  }
  return "";
}

std::unordered_map<std::string, std::string> get_config_from_prompt(
    std::string setup_message, std::vector<std::string> desired_vars) {
  std::unordered_map<std::string, std::string> responses;
  std::cout << setup_message << std::endl;

  std::string prompt;
  for (std::string desired_var : desired_vars) {
    prompt = _get_prompt(desired_var);
    std::cout << prompt;
    std::cin >> responses[desired_var];
  }
  return responses;
}

std::unordered_map<std::string, std::string> get_config_from_json(
    std::string json_path, std::vector<std::string> desired_vars) {
  std::ifstream json_file(json_path);
  std::stringstream buffer;
  buffer << json_file.rdbuf();
  std::string json_text = buffer.str();
  rapidjson::Document d;
  json::parse_json(json_text, d);

  std::unordered_map<std::string, std::string> responses;
  std::string response;
  for (std::string desired_var : desired_vars) {
    response = json::get_string(d, desired_var.c_str());
    responses[desired_var] = response;
  }
  return responses;
}

std::unordered_map<int, std::vector<std::vector<double>>> load_cifar(
    std::unordered_map<std::string, std::string> &config) {
  std::string cifar_data_path = config.find(CIFAR_DATA_PATH)->second;

  // A loose check to ensure that the binary dataset (not the python-compatible
  // dataset) is being used
  if (cifar_data_path.find(".bin") == std::string::npos) {
    log_error(
        "BENCH",
        "Please specify the full path of the binary CIFAR-100 data file.");
    exit(1);  // doesn't seem to actually exit the script
  }

  std::ifstream cifar_file(cifar_data_path, std::ios::binary);
  std::istreambuf_iterator<char> cifar_data(cifar_file);
  std::unordered_map<int, std::vector<std::vector<double>>> vecs_map;
  for (int i = 0; i < 10000; i++) {
    int label = static_cast<int>(*cifar_data);
    cifar_data++;
    std::vector<uint8_t> cifar_byte_vec;
    cifar_byte_vec.reserve(3072);
    std::copy_n(cifar_data, 3072, std::back_inserter(cifar_byte_vec));
    cifar_data++;
    std::vector<double> cifar_double_vec(cifar_byte_vec.begin(),
                                         cifar_byte_vec.end());

    std::unordered_map<int, std::vector<std::vector<double>>>::iterator
        label_vecs = vecs_map.find(label);
    if (label_vecs != vecs_map.end()) {
      label_vecs->second.push_back(cifar_double_vec);
    } else {
      std::vector<std::vector<double>> new_label_vecs;
      new_label_vecs.push_back(cifar_double_vec);
      vecs_map.emplace(label, new_label_vecs);
    }
  }
  return vecs_map;
}

std::vector<std::vector<double>> concatenate_cifar_datapoints(
    std::unordered_map<int, std::vector<std::vector<double>>> cifar_data) {
  std::vector<std::vector<double>> planes_vecs = cifar_data.find(0)->second;
  std::vector<std::vector<double>> birds_vecs = cifar_data.find(2)->second;

  planes_vecs.insert(planes_vecs.end(), birds_vecs.begin(), birds_vecs.end());
  return planes_vecs;
}

std::string get_str(const std::string &key,
                    std::unordered_map<std::string, std::string> &config) {
  return config.find(key)->second;
}

int get_int(const std::string &key,
            std::unordered_map<std::string, std::string> &config) {
  return std::stoi(get_str(key, config));
}

long get_long(const std::string &key,
              std::unordered_map<std::string, std::string> &config) {
  return static_cast<long>(get_int(key, config));
}

bool get_bool(const std::string &key,
              std::unordered_map<std::string, std::string> &config) {
  return get_str(key, config) == "true";
}

}  // namespace bench_utils
