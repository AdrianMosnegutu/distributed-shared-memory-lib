#ifndef BLOCKING_QUEUE_HPP
#define BLOCKING_QUEUE_HPP

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>

namespace dsm {

template <typename T> class BlockingQueue {
public:
  void push(T value) {
    {
      std::unique_lock<std::mutex> lock(mtx_);
      queue_.push(std::move(value));
    }
    cv_.notify_one();
  }

  std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this]{ return !queue_.empty(); });
    
        if (queue_.empty()) {
            return std::nullopt;
        }

    T value = std::move(queue_.front());
    queue_.pop();
    return value;
  }

  bool empty() const {
    std::unique_lock<std::mutex> lock(mtx_);
    return queue_.empty();
  }

  size_t size() const {
    std::unique_lock<std::mutex> lock(mtx_);
    return queue_.size();
  }

private:
  std::queue<T> queue_;
  mutable std::mutex mtx_;
  std::condition_variable cv_;
};

} // namespace dsm

#endif // BLOCKING_QUEUE_HPP
