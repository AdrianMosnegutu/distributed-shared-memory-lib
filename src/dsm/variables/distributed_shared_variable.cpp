#include "dsm/variables/distributed_shared_variable.hpp"

namespace dsm::internal {

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
      std::vector<std::function<void()>> handlers_to_call;
  
      { 
          std::lock_guard<std::mutex> lock(mtx_);
  
          while (!processing_queue_.empty()) {
              const Timestamp &ts = processing_queue_.top();
              RequestState &req = requests_.at(ts);
  
              if (req.acks.size() < subscribers_.size()) {
                  break;
              }
  
              const Message msg = req.msg;
              requests_.erase(ts);
              processing_queue_.pop();
  
              if (msg.type == MessageType::WRITE_REQUEST) {
                  value_ = msg.value1;
                  clock_.clock = std::max(clock_.clock, msg.ts.clock) + 1;
  
                  if (write_result_handler_) {
                      handlers_to_call.emplace_back([this, ts = msg.ts]() { write_result_handler_(ts); });
                  }
                  if (callback_) {
                      handlers_to_call.emplace_back([this, val = value_]() { callback_(val); });
                  }
              } else if (msg.type == MessageType::CAS_REQUEST) {
                  bool success = (value_ == msg.value1);
                  if (success) {
                      value_ = msg.value2;
                  }
  
                  clock_.clock = std::max(clock_.clock, msg.ts.clock) + 1;
  
                  if (cas_result_handler_) {
                      handlers_to_call.emplace_back(
                          [this, ts = msg.ts, success]() { cas_result_handler_(ts, success); });
                  }
                  if (success && callback_) {
                      handlers_to_call.emplace_back([this, val = value_]() { callback_(val); });
                  }
              }
          }
      } 
  
      for (const auto &handler : handlers_to_call) {
          handler();
      }}

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

} // namespace dsm::internal
