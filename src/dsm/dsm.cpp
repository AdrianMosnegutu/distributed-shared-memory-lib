#include "dsm.hpp"
#include "config.hpp"
#include "dsm_manager.hpp"
#include <memory>
#include <mpi.h>
#include <stdexcept>

namespace dsm {

namespace {

// Global instance of the DSM manager.
std::unique_ptr<internal::DSMManager> manager = nullptr;
int rank = -1;
int world_size = -1;

} // namespace

void init(const std::string &config_path) {
  int provided;
  MPI_Init_thread(nullptr, nullptr, MPI_THREAD_MULTIPLE, &provided);

  if (provided < MPI_THREAD_MULTIPLE) {
    throw std::runtime_error(
        "MPI implementation does not provide the required MPI_THREAD_MULTIPLE support.");
  }

  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  auto config = internal::parse_config(config_path);

  if (world_size != config.num_processes) {
    throw std::runtime_error("MPI world size does not match number of processes in config file.");
  }

  manager = std::make_unique<internal::DSMManager>(rank, world_size, std::move(config));
  manager->run(); // Starts the listener thread
}

void finalize() {
  // Use a barrier to ensure all processes are ready to shut down. This
  // prevents race conditions where one process sends a message to another that
  // has already started cleaning up.
  MPI_Barrier(MPI_COMM_WORLD);

  // Resetting the manager triggers the RAII chain:
  // ~DSMManager -> ~Listener -> ~jthread -> join()
  // This cleans up all local resources and stops the listener thread.
  manager.reset();

  // It is good practice to have another barrier here to ensure all processes
  // have finished their cleanup before finalizing MPI, though it is often
  // not strictly necessary if the above logic is sound.
  MPI_Barrier(MPI_COMM_WORLD);

  MPI_Finalize();
}

const std::map<int, std::vector<int>> &get_subscriptions() {
  if (!manager) {
    throw std::runtime_error("DSM not initialized.");
  }
  return manager->get_subscriptions();
}

void on_change(int var_id, std::function<void(int)> callback) {
  if (!manager) {
    throw std::runtime_error("DSM not initialized.");
  }
  manager->register_callback(var_id, std::move(callback));
}

std::future<void> write(int var_id, int value) {
  if (!manager) {
    throw std::runtime_error("DSM not initialized.");
  }
  return manager->write(var_id, value);
}

std::future<bool> compare_and_exchange(int var_id, int expected_value, int new_value) {
  if (!manager) {
    throw std::runtime_error("DSM not initialized.");
  }
  return manager->compare_and_exchange(var_id, expected_value, new_value);
}

} // namespace dsm
