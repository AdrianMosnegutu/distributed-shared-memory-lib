#pragma once

#include "config.hpp"
#include "message.hpp"
#include <functional>
#include <future>
#include <map>
#include <mpi.h>
#include <mutex>
#include <thread>

namespace dsm {

class DistributedSharedVariable; // Forward declaration

class DSMManager {
  friend class DistributedSharedVariable;

public:
  DSMManager(int rank, int world_size, Config config);
  ~DSMManager();

  void run();
  void stop();

  void register_callback(int var_id, std::function<void(int)> callback);
  void write(int var_id, int value);
  std::future<bool> compare_and_exchange(int var_id, int expected_value, int new_value);

  void resolve_cas_promise(Timestamp ts, bool success);

private:
  void listener_thread(std::stop_token stop_token);

  int rank_;
  int world_size_;
  Config config_;

  std::map<int, DistributedSharedVariable> variables_;
  std::map<Timestamp, std::promise<bool>> cas_promises_;
  std::mutex cas_promises_mtx_;

  std::jthread listener_;
  std::stop_source stop_source_;
  MPI_Datatype mpi_message_type_;
  MPI_Datatype mpi_timestamp_type_;
};

} // namespace dsm
