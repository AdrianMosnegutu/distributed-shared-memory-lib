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
  thread_ = std::jthread(&Producer::listen, this, stop_source_.get_token(), mpi_message_type);
}

void Producer::listen(std::stop_token stop_token, MPI_Datatype mpi_message_type) {
  while (!stop_token.stop_requested()) {
    int flag = 0;
    MPI_Status status;
    MPI_Iprobe(MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &flag, &status);

    if (flag) {
      Message msg;
      MPI_Recv(&msg, 1, mpi_message_type, status.MPI_SOURCE, 0, MPI_COMM_WORLD, &status);
      msg.sender_rank = status.MPI_SOURCE;

      message_queue_.push(msg);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

} // namespace dsm::internal
