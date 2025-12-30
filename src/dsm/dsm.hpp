#pragma once

#include <functional>
#include <future>
#include <string>

namespace dsm {

void init(const std::string &config_path);
void finalize();

void on_change(int var_id, std::function<void(int)> callback);
void write(int var_id, int value);
std::future<bool> compare_and_exchange_async(int var_id, int expected_value, int new_value);

} // namespace dsm
