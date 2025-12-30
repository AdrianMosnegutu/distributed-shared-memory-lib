#pragma once

#include "message.hpp"
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <vector>

namespace dsm {

class DSMManager; // Forward declaration

struct RequestState {
  Message msg;
  std::set<int> acks;
};

class DistributedSharedVariable {
public:
  DistributedSharedVariable() = default;
  DistributedSharedVariable(DSMManager *manager, std::vector<int> subscribers);

  void add_request(const Message &msg);
  void add_ack(const Timestamp &ts, int sender_rank);
  void process_requests();

  int get_value() const;
  Timestamp get_timestamp() const;
  const std::vector<int> &get_subscribers() const;

  void register_callback(std::function<void(int)> callback);

private:
  DSMManager *manager_{nullptr};
  int value_{0};
  Timestamp clock_{.clock = 0, .rank = 0};
  std::vector<int> subscribers_;
  std::function<void(int)> callback_;

  std::map<Timestamp, RequestState> request_queue_;
  mutable std::mutex mtx_;
};

} // namespace dsm
