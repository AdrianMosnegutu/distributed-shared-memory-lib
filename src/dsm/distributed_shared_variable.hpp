#pragma once

#include "message.hpp"
#include <functional>
#include <mutex>
#include <queue>
#include <set>
#include <unordered_map>
#include <vector>

namespace dsm {

struct RequestState {
  Message msg;
  std::set<int> acks;
};

class DistributedSharedVariable {
public:
  using CasResultHandler = std::function<void(const Timestamp &, bool)>;

  DistributedSharedVariable() = default;
  DistributedSharedVariable(std::vector<int> subscribers);

  void add_request(const Message &msg);
  void add_ack(const Timestamp &ts, int sender_rank);
  void process_requests();

  int get_value() const;
  Timestamp get_timestamp() const;
  const std::vector<int> &get_subscribers() const;

  void register_callback(std::function<void(int)> callback);

  template <typename Handler> void register_cas_result_handler(Handler &&handler) {
    std::lock_guard<std::mutex> lock(mtx_);
    cas_result_handler_ = std::forward<Handler>(handler);
  }

private:
  int value_{0};
  Timestamp clock_{.clock = 0, .rank = 0};

  const std::vector<int> subscribers_;

  std::function<void(int)> callback_;
  CasResultHandler cas_result_handler_;

  std::unordered_map<Timestamp, RequestState> requests_;
  std::priority_queue<Timestamp, std::vector<Timestamp>, std::greater<>> processing_queue_;

  mutable std::mutex mtx_;
};

} // namespace dsm
