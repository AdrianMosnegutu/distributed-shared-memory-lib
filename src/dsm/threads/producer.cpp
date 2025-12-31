#include "dsm/threads/producer.hpp"
#include "dsm/messages/message.hpp"
#include <chrono>

namespace dsm::internal {

Producer::Producer(BlockingQueue<Message> &message_queue) : message_queue_(message_queue) {}

Producer::~Producer() {
  if (thread_.joinable()) {
    stop_source_.request_stop();
    thread_.join();
  }
}

void Producer::run(MPI_Datatype mpi_message_type) {
  stop_token_ = stop_source_.get_token();
  thread_ = std::jthread(&Producer::listen, this, mpi_message_type);
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
