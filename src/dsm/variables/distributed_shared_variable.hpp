#pragma once

#include "dsm/common/message.hpp"
#include <functional>
#include <mutex>
#include <queue>
#include <set>
#include <unordered_map>
#include <vector>

namespace dsm::internal {

class DistributedSharedVariable {
public:
  using CasResultHandler = std::function<void(const Timestamp &, bool)>;
  using WriteResultHandler = std::function<void(const Timestamp &)>;

  DistributedSharedVariable() = default;
  explicit DistributedSharedVariable(std::set<int> subscribers);

  void add_request(const Message &msg);
  void process_requests();

  int get_value() const;
  Timestamp get_timestamp() const;
  const std::set<int> &get_subscribers() const;
  int get_primary_rank() const;

  void register_callback(std::function<void(int)> callback);

  template <typename Handler> void register_write_result_handler(Handler &&handler) {
    std::lock_guard<std::mutex> lock(mtx_);
    write_result_handler_ = std::forward<Handler>(handler);
  }

  template <typename Handler> void register_cas_result_handler(Handler &&handler) {
    std::lock_guard<std::mutex> lock(mtx_);
    cas_result_handler_ = std::forward<Handler>(handler);
  }

private:
  int value_{0};
  Timestamp clock_{.clock = 0, .rank = 0};

  std::set<int> subscribers_;
  int primary_rank_;
  std::function<void(int)> callback_;

  CasResultHandler cas_result_handler_;
  WriteResultHandler write_result_handler_;

  std::unordered_map<Timestamp, Message> requests_;
  std::priority_queue<Timestamp, std::vector<Timestamp>, std::greater<>> processing_queue_;

  mutable std::mutex mtx_;
};

} // namespace dsm::internal
