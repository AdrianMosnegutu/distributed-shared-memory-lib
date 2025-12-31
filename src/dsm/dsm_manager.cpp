#include "dsm_manager.hpp"
#include "distributed_shared_variable.hpp"
#include <array>
#include <functional>
#include <mpi.h>
#include <mutex>

namespace dsm::internal {

DSMManager::DSMManager(int rank, int world_size, Config config)
    : rank_(rank), world_size_(world_size), config_(std::move(config)),
      producer_(std::make_unique<Producer>(message_queue_)) { // Pass message_queue_ to Producer

  for (const auto &[var_id, subscribers] : config_.subscriptions) {
    for (int subscriber_rank : subscribers) {
      if (subscriber_rank == rank_) {
        auto [it, inserted] = variables_.try_emplace(var_id, subscribers);
        if (inserted) {
          it->second.register_cas_result_handler(std::bind_front(&DSMManager::on_cas_result, this));
          it->second.register_write_result_handler(
              std::bind_front(&DSMManager::on_write_result, this));
        }
        break;
      }
    }
  }

  // Create MPI type for nested Timestamp struct
  const std::array<int, 2> ts_blocklengths = {1, 1};
  const std::array<MPI_Aint, 2> ts_displacements = {offsetof(Timestamp, clock),
                                                    offsetof(Timestamp, rank)};
  const std::array<MPI_Datatype, 2> ts_types = {MPI_INT, MPI_INT};
  MPI_Type_create_struct(2, ts_blocklengths.data(), ts_displacements.data(), ts_types.data(),
                         &mpi_timestamp_type_);
  MPI_Type_commit(&mpi_timestamp_type_);

  // Create MPI type for the main Message struct
  const std::array<int, 6> msg_blocklengths = {1, 1, 1, 1, 1, 1};
  const std::array<MPI_Aint, 6> msg_displacements = {
      offsetof(Message, type), offsetof(Message, ts), offsetof(Message, var_id),
      offsetof(Message, value1), offsetof(Message, value2), offsetof(Message, sender_rank)};
  const std::array<MPI_Datatype, 6> msg_types = {MPI_UINT8_T, mpi_timestamp_type_, MPI_INT, MPI_INT,
                                                 MPI_INT, MPI_INT};
  MPI_Type_create_struct(6, msg_blocklengths.data(), msg_displacements.data(), msg_types.data(),
                         &mpi_message_type_);
  MPI_Type_commit(&mpi_message_type_);

  start_consumer_thread();
}

DSMManager::~DSMManager() {
  stop_consumer_thread(); // Stop the consumer thread first

  // 1. Send a "poison pill" message to our own listener thread to unblock it.
  Message shutdown_msg;
  shutdown_msg.type = MessageType::SHUTDOWN;
  MPI_Send(&shutdown_msg, 1, mpi_message_type_, rank_, 0, MPI_COMM_WORLD);

  // 2. Explicitly destroy the listener, which joins the thread. This is a
  // blocking call that ensures the thread is finished before we proceed.
  producer_.reset();

  // 3. Now that the thread is gone, it's safe to free the MPI resources.
  MPI_Type_free(&mpi_message_type_);
  MPI_Type_free(&mpi_timestamp_type_);
}

void DSMManager::start_consumer_thread() {
  consumer_thread_ = std::thread(&DSMManager::consumer_thread_loop, this);
}

void DSMManager::stop_consumer_thread() {
  if (consumer_thread_.joinable()) {
    Message poison_pill;
    poison_pill.type = MessageType::SHUTDOWN;
    message_queue_.push(poison_pill); // Push a shutdown message
    consumer_thread_.join();
  }
}

void DSMManager::consumer_thread_loop() {
  while (true) {
    std::optional<Message> optional_msg = message_queue_.pop();
    if (!optional_msg.has_value()) {
      continue; // Should not happen unless queue is empty and shutdown is not requested
    }
    Message msg = optional_msg.value();

    if (msg.type == MessageType::SHUTDOWN) {
      break; // Exit loop on shutdown message
    }

    auto it = variables_.find(msg.var_id);
    if (it == variables_.end()) {
      // Potentially log this: received message for unsubscribed variable
      continue;
    }
    auto &var = it->second;

    if (msg.type == MessageType::WRITE_REQUEST || msg.type == MessageType::CAS_REQUEST) {
      // This is the logic previously in request_handler
      var.add_request(msg);

      Message ack_msg;
      ack_msg.type =
          (msg.type == MessageType::WRITE_REQUEST) ? MessageType::WRITE_ACK : MessageType::CAS_ACK;
      ack_msg.ts = msg.ts; // Echo the timestamp of the original request
      ack_msg.var_id = msg.var_id;

      // Ensure that MPI_Send is only called for actual subscribers
      for (int subscriber_rank : var.get_subscribers()) {
        MPI_Send(&ack_msg, 1, mpi_message_type_, subscriber_rank, 0, MPI_COMM_WORLD);
      }
      var.process_requests();
    } else if (msg.type == MessageType::WRITE_ACK || msg.type == MessageType::CAS_ACK) {
      // This is the logic previously in ack_handler
      var.add_ack(msg.ts, msg.sender_rank);
      var.process_requests();
    }
  }
}

void DSMManager::run() { producer_->run(mpi_message_type_); }

const std::map<int, std::vector<int>> &DSMManager::get_subscriptions() const {
  return config_.subscriptions;
}

void DSMManager::register_callback(int var_id, std::function<void(int)> callback) {
  auto it = variables_.find(var_id);
  if (it == variables_.end()) {
    throw std::runtime_error("Attempted to register callback for non-subscribed variable.");
  }
  it->second.register_callback(std::move(callback));
}

std::future<void> DSMManager::write(int var_id, int value) {
  auto it = variables_.find(var_id);
  if (it == variables_.end()) {
    throw std::runtime_error("Attempted to write to non-subscribed variable.");
  }

  auto &var = it->second;
  auto ts = var.get_timestamp();
  ts.clock++;
  ts.rank = rank_;

  Message msg;
  msg.type = MessageType::WRITE_REQUEST;
  msg.ts = ts;
  msg.var_id = var_id;
  msg.value1 = value;

  std::promise<void> promise;
  auto future = promise.get_future();

  {
    std::lock_guard<std::mutex> lock(write_promises_mtx_);
    write_promises_[ts] = std::move(promise);
  }

  const auto &subscribers = config_.subscriptions.at(var_id);
  for (int subscriber_rank : subscribers) {
    MPI_Send(&msg, 1, mpi_message_type_, subscriber_rank, 0, MPI_COMM_WORLD);
  }

  return future;
}

std::future<bool> DSMManager::compare_and_exchange(int var_id, int expected_value, int new_value) {
  auto it = variables_.find(var_id);
  if (it == variables_.end()) {
    throw std::runtime_error("Attempted to CAS non-subscribed variable.");
  }

  auto &var = it->second;
  auto ts = var.get_timestamp();
  ts.clock++;
  ts.rank = rank_;

  Message msg;
  msg.type = MessageType::CAS_REQUEST;
  msg.ts = ts;
  msg.var_id = var_id;
  msg.value1 = expected_value;
  msg.value2 = new_value;

  std::promise<bool> promise;
  auto future = promise.get_future();

  {
    std::lock_guard<std::mutex> lock(cas_promises_mtx_);
    cas_promises_[ts] = std::move(promise);
  }

  const auto &subscribers = config_.subscriptions.at(var_id);
  for (int subscriber_rank : subscribers) {
    MPI_Send(&msg, 1, mpi_message_type_, subscriber_rank, 0, MPI_COMM_WORLD);
  }

  return future;
}

void DSMManager::on_cas_result(const Timestamp &ts, bool success) {
  if (rank_ == ts.rank) {
    resolve_cas_promise(ts, success);
  }
}

void DSMManager::on_write_result(const Timestamp &ts) {
  if (rank_ == ts.rank) {
    resolve_write_promise(ts);
  }
}

void DSMManager::resolve_cas_promise(Timestamp ts, bool success) {
  std::lock_guard<std::mutex> lock(cas_promises_mtx_);
  auto it = cas_promises_.find(ts);

  if (it != cas_promises_.end()) {
    it->second.set_value(success);
    cas_promises_.erase(it);
  }
}

void DSMManager::resolve_write_promise(const Timestamp &ts) {
  std::lock_guard<std::mutex> lock(write_promises_mtx_);
  auto it = write_promises_.find(ts);

  if (it != write_promises_.end()) {
    it->second.set_value();
    write_promises_.erase(it);
  }
}

} // namespace dsm::internal
