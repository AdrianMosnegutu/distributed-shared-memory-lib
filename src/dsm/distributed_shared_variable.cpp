#include "distributed_shared_variable.hpp"

namespace dsm {

DistributedSharedVariable::DistributedSharedVariable(std::vector<int> subscribers)
    : subscribers_(std::move(subscribers)) {}

void DistributedSharedVariable::add_request(const Message &msg) {
  std::lock_guard<std::mutex> lock(mtx_);
  if (requests_.count(msg.ts) == 0) {
    requests_.emplace(msg.ts, RequestState{.msg = msg, .acks = {}});
    processing_queue_.push(msg.ts);
  }
}

void DistributedSharedVariable::add_ack(const Timestamp &ts, int sender_rank) {
  std::lock_guard<std::mutex> lock(mtx_);
  if (requests_.count(ts) > 0) {
    requests_.at(ts).acks.insert(sender_rank);
  }
}

void DistributedSharedVariable::process_requests() {
  std::lock_guard<std::mutex> lock(mtx_);

  while (!processing_queue_.empty()) {
    const Timestamp &ts = processing_queue_.top();
    RequestState &req = requests_.at(ts);

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

      if (cas_result_handler_) {
        cas_result_handler_(msg.ts, success);
      }

      if (success && callback_) {
        callback_(value_);
      }
    }

    requests_.erase(ts);
    processing_queue_.pop();
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
