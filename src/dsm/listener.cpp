#include "listener.hpp"
#include "message.hpp"
#include <chrono>

namespace dsm::internal {

void Listener::on_request(RequestHandler handler) { request_handler_ = std::move(handler); }

void Listener::on_ack(AckHandler handler) { ack_handler_ = std::move(handler); }

void Listener::run(MPI_Datatype mpi_message_type) {
  thread_ = std::jthread(&Listener::listen, this, stop_source_.get_token(), mpi_message_type);
}

void Listener::listen(std::stop_token stop_token, MPI_Datatype mpi_message_type) {
  while (!stop_token.stop_requested()) {
    int flag = 0;
    MPI_Status status;
    MPI_Iprobe(MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &flag, &status);

    if (flag) {
      Message msg;
      MPI_Recv(&msg, 1, mpi_message_type, status.MPI_SOURCE, 0, MPI_COMM_WORLD, &status);
      int sender_rank = status.MPI_SOURCE;

      switch (msg.type) {
      case MessageType::WRITE_REQUEST:
      case MessageType::CAS_REQUEST:
        if (request_handler_) {
          request_handler_(msg);
        }
        break;

      case MessageType::WRITE_ACK:
      case MessageType::CAS_ACK:
        if (ack_handler_) {
          ack_handler_(msg, sender_rank);
        }
        break;

      case MessageType::SHUTDOWN:
        return; // Exit the thread function
      }
    }

    // Avoid busy-waiting
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

} // namespace dsm::internal
