#pragma once

#include <functional>
#include <future>
#include <set>

namespace dsm {

// Main API
void init(int argc, char **argv);
void finalize();

bool is_subscribed(int var_id, int rank);
const std::unordered_map<int, std::set<int>> &get_subscriptions();

// Variable-specific API
void on_change(int var_id, std::function<void(int)> callback);
std::future<void> write(int var_id, int value);
std::future<bool> compare_and_exchange(int var_id, int expected_value, int new_value);

} // namespace dsm
