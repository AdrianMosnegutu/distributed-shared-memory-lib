#include "dsm_manager.hpp"
#include "distributed_shared_variable.hpp"
#include <array>
#include <functional>
#include <mpi.h>

namespace dsm {

DSMManager::DSMManager(int rank, int world_size, Config config)
    : rank_(rank), world_size_(world_size), config_(std::move(config)) {
  for (const auto &[var_id, subscribers] : config_.subscriptions) {
    for (int subscriber_rank : subscribers) {
      if (subscriber_rank != rank_) {
        continue;
      }

      auto [it, inserted] = variables_.try_emplace(var_id, subscribers);
      if (inserted) {
        it->second.register_cas_result_handler(std::bind_front(&DSMManager::on_cas_result, this));
      }

      break;
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
  const std::array<int, 5> msg_blocklengths = {1, 1, 1, 1, 1};
  const std::array<MPI_Aint, 5> msg_displacements = {
      offsetof(Message, type), offsetof(Message, ts), offsetof(Message, var_id),
      offsetof(Message, value1), offsetof(Message, value2)};
  const std::array<MPI_Datatype, 5> msg_types = {MPI_UINT8_T, mpi_timestamp_type_, MPI_INT, MPI_INT,
                                                 MPI_INT};
  MPI_Type_create_struct(5, msg_blocklengths.data(), msg_displacements.data(), msg_types.data(),
                         &mpi_message_type_);
  MPI_Type_commit(&mpi_message_type_);
}

DSMManager::~DSMManager() {
  MPI_Type_free(&mpi_message_type_);
  MPI_Type_free(&mpi_timestamp_type_);
}

void DSMManager::run() {
  listener_ = std::jthread(&DSMManager::listener_thread, this, stop_source_.get_token());
}

void DSMManager::stop() { stop_source_.request_stop(); }

void DSMManager::listener_thread(std::stop_token stop_token) {
  while (!stop_token.stop_requested()) {
    int flag = 0;
    MPI_Status status;
    MPI_Iprobe(MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &flag, &status);

    if (flag) {
      Message msg;
      MPI_Recv(&msg, 1, mpi_message_type_, status.MPI_SOURCE, 0, MPI_COMM_WORLD, &status);
      int sender_rank = status.MPI_SOURCE;

      auto it = variables_.find(msg.var_id);
      if (it == variables_.end()) {
        continue; // Not subscribed to this variable
      }
      auto &var = it->second;

      switch (msg.type) {
      case MessageType::WRITE_REQUEST:
      case MessageType::CAS_REQUEST: {
        var.add_request(msg);
        Message ack_msg;
        ack_msg.type = (msg.type == MessageType::WRITE_REQUEST) ? MessageType::WRITE_ACK
                                                                : MessageType::CAS_ACK;
        ack_msg.ts = msg.ts; // Echo the timestamp of the original request
        ack_msg.var_id = msg.var_id;
        for (int subscriber_rank : var.get_subscribers()) {
          MPI_Send(&ack_msg, 1, mpi_message_type_, subscriber_rank, 0, MPI_COMM_WORLD);
        }
        break;
      }

      case MessageType::WRITE_ACK:
      case MessageType::CAS_ACK: {
        var.add_ack(msg.ts, sender_rank);
        break;
      }
      }

      var.process_requests();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

void DSMManager::register_callback(int var_id, std::function<void(int)> callback) {
  auto it = variables_.find(var_id);
  if (it == variables_.end()) {
    throw std::runtime_error("Attempted to register callback for non-subscribed variable.");
  }
  it->second.register_callback(std::move(callback));
}

void DSMManager::write(int var_id, int value) {
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

  const auto &subscribers = config_.subscriptions.at(var_id);
  for (int subscriber_rank : subscribers) {
    MPI_Send(&msg, 1, mpi_message_type_, subscriber_rank, 0, MPI_COMM_WORLD);
  }
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

void DSMManager::resolve_cas_promise(Timestamp ts, bool success) {
  std::lock_guard<std::mutex> lock(cas_promises_mtx_);
  auto it = cas_promises_.find(ts);

  if (it != cas_promises_.end()) {
    it->second.set_value(success);
    cas_promises_.erase(it);
  }
}

} // namespace dsm
