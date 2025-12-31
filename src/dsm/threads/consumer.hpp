#pragma once

#include "dsm/data_structures/blocking_queue.hpp"
#include "dsm/messages/message.hpp"
#include "dsm/variables/distributed_shared_variable.hpp"
#include <mpi.h>
#include <thread>
#include <unordered_map>

namespace dsm::internal {

class Consumer {
public:
  explicit Consumer(BlockingQueue<Message> &message_queue,
                    std::unordered_map<int, DistributedSharedVariable> &variables, int rank,
                    MPI_Datatype mpi_message_type);
  ~Consumer();

private:
  void listen();

  BlockingQueue<Message> &message_queue_;
  std::unordered_map<int, DistributedSharedVariable> &variables_;
  int rank_;
  MPI_Datatype mpi_message_type_;

  std::jthread thread_;
  std::stop_token stop_token_;
  std::stop_source stop_source_;
};

} // namespace dsm::internal
