#pragma once

#include <set>
#include <unordered_map>

namespace dsm::internal {

struct Config {
  int num_variables;
  std::unordered_map<int, std::set<int>> subscriptions;
};

Config get_example_config();

} // namespace dsm::internal
