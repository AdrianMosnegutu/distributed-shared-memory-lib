#include "dsm/promises/promise_manager.hpp"

namespace dsm::internal {

void PromiseManager::add_cas_promise(Timestamp ts, std::promise<bool> promise) {
  std::lock_guard<std::mutex> lock(cas_promises_mtx_);
  cas_promises_[ts] = std::move(promise);
}

void PromiseManager::add_write_promise(Timestamp ts, std::promise<void> promise) {
  std::lock_guard<std::mutex> lock(write_promises_mtx_);
  write_promises_[ts] = std::move(promise);
}

void PromiseManager::resolve_cas_promise(Timestamp ts, bool success) {
  std::lock_guard<std::mutex> lock(cas_promises_mtx_);
  auto it = cas_promises_.find(ts);

  if (it != cas_promises_.end()) {
    it->second.set_value(success);
    cas_promises_.erase(it);
  }
}

void PromiseManager::resolve_write_promise(const Timestamp &ts) {
  std::lock_guard<std::mutex> lock(write_promises_mtx_);
  auto it = write_promises_.find(ts);

  if (it != write_promises_.end()) {
    it->second.set_value();
    write_promises_.erase(it);
  }
}

} // namespace dsm::internal
