#include "distributed_shared_variable.hpp"
#include "dsm_manager.hpp"

namespace dsm {

DistributedSharedVariable::DistributedSharedVariable(DSMManager *manager,
                                                     std::vector<int> subscribers)
    : manager_(manager), subscribers_(std::move(subscribers)) {}

void DistributedSharedVariable::add_request(const Message &msg) {
  std::lock_guard<std::mutex> lock(mtx_);
  if (request_queue_.count(msg.ts) == 0) {
    request_queue_[msg.ts] = RequestState{.msg = msg, .acks = {}};
  }
}

void DistributedSharedVariable::add_ack(const Timestamp &ts, int sender_rank) {
  std::lock_guard<std::mutex> lock(mtx_);
  if (request_queue_.count(ts) > 0) {
    request_queue_.at(ts).acks.insert(sender_rank);
  }
}

void DistributedSharedVariable::process_requests() {
  std::lock_guard<std::mutex> lock(mtx_);

  while (!request_queue_.empty()) {
    auto it = request_queue_.begin();
    RequestState &req = it->second;

    // A request can only be processed if it has been acknowledged by all subscribers.
    if (req.acks.size() < subscribers_.size()) {
      break; // The first request in the queue is not ready, so none after it are either.
    }

    const Message &msg = req.msg;

    if (msg.type == MessageType::WRITE_REQUEST) {
      value_ = msg.value1;
      clock_.clock = std::max(clock_.clock, msg.ts.clock) + 1; // Increment clock after processing
      if (callback_) {
        callback_(value_);
      }
    } else if (msg.type == MessageType::CAS_REQUEST) {
      bool success = (value_ == msg.value1);
      if (success) {
        value_ = msg.value2;
      }

      clock_.clock = std::max(clock_.clock, msg.ts.clock) + 1; // Increment clock after processing

      if (manager_ != nullptr && manager_->rank_ == msg.ts.rank) {
        manager_->resolve_cas_promise(msg.ts, success);
      }

      if (success && callback_) {
        callback_(value_);
      }
    }

    request_queue_.erase(it); // Remove the processed request
  }
}

int DistributedSharedVariable::get_value() const {
  std::lock_guard<std::mutex> lock(mtx_);
  return value_;
}

Timestamp DistributedSharedVariable::get_timestamp() const {
  std::lock_guard<std::mutex> lock(mtx_);
  return clock_;
}

const std::vector<int> &DistributedSharedVariable::get_subscribers() const { return subscribers_; }

void DistributedSharedVariable::register_callback(std::function<void(int)> callback) {
  std::lock_guard<std::mutex> lock(mtx_);
  callback_ = std::move(callback);
}

} // namespace dsm
