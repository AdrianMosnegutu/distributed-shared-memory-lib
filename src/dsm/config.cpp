#include "config.hpp"
#include "../../vendor/json.hpp"
#include <fstream>
#include <stdexcept>

namespace dsm {

using json = nlohmann::json;

Config parse_config(const std::string &file_path) {
  std::ifstream f(file_path);
  if (!f) {
    throw std::runtime_error("Could not open config file: " + file_path);
  }

  json data = json::parse(f);

  Config config;
  config.num_processes = data.at("processes").get<int>();
  config.num_variables = data.at("variables").get<int>();

  for (auto const &[var_id_str, subs] : data.at("subscriptions").items()) {
    int var_id = std::stoi(var_id_str);
    config.subscriptions[var_id] = subs.get<std::vector<int>>();
  }

  return config;
}

} // namespace dsm
