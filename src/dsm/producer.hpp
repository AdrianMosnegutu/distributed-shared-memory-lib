#pragma once

#include "blocking_queue.hpp"
#include "message.hpp"
#include <mpi.h>
#include <thread>

namespace dsm::internal {

class Producer {
public:
  explicit Producer(BlockingQueue<Message> &message_queue);
  ~Producer();

  void run(MPI_Datatype mpi_message_type);

private:
  void listen(std::stop_token stop_token, MPI_Datatype mpi_message_type);

  BlockingQueue<Message> &message_queue_;

  std::jthread thread_;
  std::stop_source stop_source_;
};

} // namespace dsm::internal
