#include "dsm/threads/consumer.hpp"
#include <optional>

namespace dsm::internal {

Consumer::Consumer(BlockingQueue<Message> &message_queue,
                   std::unordered_map<int, DistributedSharedVariable> &variables, int rank,
                   MPI_Datatype mpi_message_type)
    : message_queue_(message_queue), variables_(variables), rank_(rank),
      mpi_message_type_(mpi_message_type) {

  stop_token_ = stop_source_.get_token();
  thread_ = std::jthread(&Consumer::listen, this);
}

Consumer::~Consumer() {
  if (thread_.joinable()) {
    stop_source_.request_stop();
    thread_.join();
  }
}

void Consumer::listen() {
  while (!stop_token_.stop_requested()) {
    std::optional<Message> optional_msg = message_queue_.pop();
    if (!optional_msg.has_value()) {
      continue;
    }
    Message msg = optional_msg.value();

    if (msg.type == MessageType::SHUTDOWN) {
      break;
    }

    auto it = variables_.find(msg.var_id);
    if (it == variables_.end()) {
      continue;
    }
    auto &var = it->second;

    if (msg.type == MessageType::WRITE_REQUEST || msg.type == MessageType::CAS_REQUEST) {
      int primary_rank = var.get_primary_rank();

      if (rank_ == primary_rank) {
        // I am the primary. I received a request from a client.
        // Broadcast to other subscribers.
        for (int subscriber_rank : var.get_subscribers()) {
          if (subscriber_rank != rank_) {
            MPI_Send(&msg, 1, mpi_message_type_, subscriber_rank, 0, MPI_COMM_WORLD);
          }
        }
        // Process locally.
        var.add_request(msg);
        var.process_requests();
      } else {
        // I am a secondary. I received a broadcast from the primary.
        var.add_request(msg);
        var.process_requests();
      }
    }
  }
}

} // namespace dsm::internal
