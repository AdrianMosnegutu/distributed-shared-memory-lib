#include "dsm.hpp"
#include "config.hpp"
#include "dsm_manager.hpp"
#include <mpi.h>
#include <stdexcept>

namespace dsm {

namespace {
// Global instance of the DSM manager.
DSMManager *manager = nullptr;
int rank = -1;
int world_size = -1;
} // namespace

void init(const std::string &config_path) {
  MPI_Init(nullptr, nullptr);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  auto config = parse_config(config_path);

  if (world_size != config.num_processes) {
    throw std::runtime_error("MPI world size does not match number of processes in config file.");
  }

  manager = new DSMManager(rank, world_size, std::move(config));
  manager->run(); // Starts the listener thread
}

void finalize() {
  manager->stop(); // Stops the listener thread
  delete manager;
  MPI_Finalize();
}

void on_change(int var_id, std::function<void(int)> callback) {
  if (!manager) {
    throw std::runtime_error("DSM not initialized.");
  }
  manager->register_callback(var_id, std::move(callback));
}

void write(int var_id, int value) {
  if (!manager) {
    throw std::runtime_error("DSM not initialized.");
  }
  manager->write(var_id, value);
}

std::future<bool> compare_and_exchange_async(int var_id, int expected_value, int new_value) {
  if (!manager) {
    throw std::runtime_error("DSM not initialized.");
  }
  return manager->compare_and_exchange(var_id, expected_value, new_value);
}

} // namespace dsm
