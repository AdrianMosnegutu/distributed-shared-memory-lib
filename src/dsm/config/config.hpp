#pragma once

#include <map>
#include <string>
#include <vector>

namespace dsm::internal {

struct Config {
  int num_processes;
  int num_variables;
  std::map<int, std::vector<int>> subscriptions;
};

Config parse_config(const std::string &file_path);

} // namespace dsm::internal
