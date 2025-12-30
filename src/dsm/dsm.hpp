#pragma once

#include <functional>
#include <future>
#include <map>
#include <string>

namespace dsm {

// Main API
void init(const std::string &config_path);
void finalize();
const std::map<int, std::vector<int>> &get_subscriptions();

// Variable-specific API
void on_change(int var_id, std::function<void(int)> callback);
std::future<void> write(int var_id, int value);
std::future<bool> compare_and_exchange(int var_id, int expected_value, int new_value);

} // namespace dsm
