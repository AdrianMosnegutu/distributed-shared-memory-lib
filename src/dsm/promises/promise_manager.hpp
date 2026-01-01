#pragma once

#include "dsm/common/message.hpp"
#include <future>
#include <mutex>
#include <unordered_map>

namespace dsm::internal {

class PromiseManager {
public:
  PromiseManager() = default;

  void add_cas_promise(Timestamp ts, std::promise<bool> promise);
  void add_write_promise(Timestamp ts, std::promise<void> promise);

  void resolve_cas_promise(Timestamp ts, bool success);
  void resolve_write_promise(const Timestamp &ts);

private:
  std::unordered_map<Timestamp, std::promise<bool>> cas_promises_;
  std::unordered_map<Timestamp, std::promise<void>> write_promises_;

  std::mutex cas_promises_mtx_;
  std::mutex write_promises_mtx_;
};

} // namespace dsm::internal
