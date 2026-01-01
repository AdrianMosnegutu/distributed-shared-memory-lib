#include "dsm/manager/dsm_manager.hpp"

namespace dsm::internal {

DSMManager::DSMManager(int rank, int world_size, Config config)
    : rank_(rank), world_size_(world_size), config_(std::move(config)),
      promise_manager_(std::make_unique<PromiseManager>()) {

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
      offsetof(Message, type),   offsetof(Message, ts),     offsetof(Message, var_id),
      offsetof(Message, value1), offsetof(Message, value2), offsetof(Message, sender_rank)};
  const std::array<MPI_Datatype, 6> msg_types = {
      MPI_UINT8_T, mpi_timestamp_type_, MPI_INT, MPI_INT, MPI_INT, MPI_INT};
  MPI_Type_create_struct(6, msg_blocklengths.data(), msg_displacements.data(), msg_types.data(),
                         &mpi_message_type_);
  MPI_Type_commit(&mpi_message_type_);

  producer_ = std::make_unique<Producer>(message_queue_, mpi_message_type_);
  consumer_ = std::make_unique<Consumer>(message_queue_, variables_, rank_, mpi_message_type_);

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
}

DSMManager::~DSMManager() {
  // Send a "poison pill" message to our own producer thread to unblock it.
  Message producer_shutdown_msg;
  producer_shutdown_msg.type = MessageType::SHUTDOWN;
  MPI_Send(&producer_shutdown_msg, 1, mpi_message_type_, rank_, 0, MPI_COMM_WORLD);
  producer_.reset();

  // Send a "poison pill" message to our own consumer thread to unblock it.
  Message consumer_shutdown_msg;
  consumer_shutdown_msg.type = MessageType::SHUTDOWN;
  message_queue_.push(consumer_shutdown_msg);
  consumer_.reset();

  // It's safe to free MPI resources after the producer thread is gone.
  MPI_Type_free(&mpi_message_type_);
  MPI_Type_free(&mpi_timestamp_type_);
}

const std::unordered_map<int, std::set<int>> &DSMManager::get_subscriptions() const {
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

  promise_manager_->add_write_promise(ts, std::move(promise));

  const auto &subscribers = config_.subscriptions.at(var_id);
  int primary_rank = var.get_primary_rank();
  if (primary_rank != -1) {
    MPI_Send(&msg, 1, mpi_message_type_, primary_rank, 0, MPI_COMM_WORLD);
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

  promise_manager_->add_cas_promise(ts, std::move(promise));

  const auto &subscribers = config_.subscriptions.at(var_id);
  int primary_rank = var.get_primary_rank();
  if (primary_rank != -1) {
    MPI_Send(&msg, 1, mpi_message_type_, primary_rank, 0, MPI_COMM_WORLD);
  }

  return future;
}

void DSMManager::on_cas_result(const Timestamp &ts, bool success) {
  if (rank_ == ts.rank) {
    promise_manager_->resolve_cas_promise(ts, success);
  }
}

void DSMManager::on_write_result(const Timestamp &ts) {
  if (rank_ == ts.rank) {
    promise_manager_->resolve_write_promise(ts);
  }
}

} // namespace dsm::internal
