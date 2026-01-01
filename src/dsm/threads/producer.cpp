#include "dsm/threads/producer.hpp"
#include <chrono>

namespace dsm::internal {

Producer::Producer(BlockingQueue<Message> &message_queue, MPI_Datatype mpi_message_type)
    : message_queue_(message_queue) {
  stop_token_ = stop_source_.get_token();
  thread_ = std::jthread(&Producer::listen, this, mpi_message_type);
}

Producer::~Producer() {
  if (thread_.joinable()) {
    stop_source_.request_stop();
    thread_.join();
  }
}

void Producer::listen(MPI_Datatype mpi_message_type) {
  int flag = 0;
  MPI_Status status;
  Message msg;

  while (!stop_token_.stop_requested()) {
    MPI_Iprobe(MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &flag, &status);

    if (flag) {
      MPI_Recv(&msg, 1, mpi_message_type, status.MPI_SOURCE, 0, MPI_COMM_WORLD, &status);
      msg.sender_rank = status.MPI_SOURCE;

      message_queue_.push(msg);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

} // namespace dsm::internal
