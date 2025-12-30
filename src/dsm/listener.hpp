#pragma once

#include "message.hpp"
#include <functional>
#include <mpi.h>
#include <thread>

namespace dsm::internal {

class Listener {
public:
  using RequestHandler = std::function<void(const Message &)>;
  using AckHandler = std::function<void(const Message &, int)>;

  Listener() = default;
  ~Listener() = default;

  // Methods to subscribe to events
  void on_request(RequestHandler handler);
  void on_ack(AckHandler handler);

  void run(MPI_Datatype mpi_message_type);

private:
  void listen(std::stop_token stop_token, MPI_Datatype mpi_message_type);

  RequestHandler request_handler_;
  AckHandler ack_handler_;

  std::jthread thread_;
  std::stop_source stop_source_;
};

} // namespace dsm::internal
