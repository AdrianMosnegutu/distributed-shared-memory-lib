#pragma once

#include "config.hpp"
#include "listener.hpp"
#include "message.hpp"
#include <functional>
#include <future>
#include <memory>
#include <mpi.h>
#include <mutex>
#include <unordered_map>

namespace dsm::internal {

class DistributedSharedVariable; // Forward declaration

class DSMManager {
public:
  DSMManager(int rank, int world_size, Config config);
  ~DSMManager();

  void run();

  const std::map<int, std::vector<int>> &get_subscriptions() const;

  void register_callback(int var_id, std::function<void(int)> callback);

  std::future<void> write(int var_id, int value);
  std::future<bool> compare_and_exchange(int var_id, int expected_value, int new_value);

  void resolve_cas_promise(Timestamp ts, bool success);
  void resolve_write_promise(const Timestamp &ts);

private:
  void on_cas_result(const Timestamp &ts, bool success);
  void on_write_result(const Timestamp &ts);

  int rank_;
  int world_size_;
  Config config_;

  std::unique_ptr<Listener> listener_;

  std::unordered_map<int, DistributedSharedVariable> variables_;

  std::unordered_map<Timestamp, std::promise<bool>> cas_promises_;
  std::unordered_map<Timestamp, std::promise<void>> write_promises_;

  std::mutex cas_promises_mtx_;
  std::mutex write_promises_mtx_;

  MPI_Datatype mpi_message_type_;
  MPI_Datatype mpi_timestamp_type_;
};

} // namespace dsm::internal
