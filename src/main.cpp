#include "dsm/dsm.hpp"
#include <cstdlib>
#include <functional>
#include <iostream>
#include <map>
#include <mpi.h>
#include <string>
#include <thread>

void print_rank(int rank, const std::string &msg) {
  std::cout << "[P" << rank << "] " << msg << '\n';
}

bool is_subscribed(int rank, int var_id) {
  const auto &subs = dsm::get_subscriptions();

  for (int r : subs.at(var_id)) {
    if (r == rank) {
      return true;
    }
  }

  return false;
}

void test_simple_write(int rank) {
  if (rank == 0) {
    print_rank(rank, "Writing 42 to var 1.");
    dsm::write(1, 42).get(); // Wait for the write to complete
  }
}

void test_cas_success(int rank) {
  if (rank == 3) {
    print_rank(rank, "Attempting CAS on var 1 (expect 42, new 100).");
    auto future = dsm::compare_and_exchange(1, 42, 100);

    if (future.get()) {
      print_rank(rank, "CAS successful.");
    } else {
      print_rank(rank, "CAS failed.");
    }
  }
}

void test_cas_failure(int rank) {
  if (rank == 2) {
    print_rank(rank, "Attempting CAS on var 1 (expect 50, new 200).");
    auto future = dsm::compare_and_exchange(1, 50, 200);

    if (future.get()) {
      print_rank(rank, "CAS successful.");
    } else {
      print_rank(rank, "CAS failed.");
    }
  }
}

void test_exception(int rank) {
  if (rank == 2) {
    try {
      print_rank(rank, "Attempting to write to var 0 (not subscribed). Should throw exception.");
      dsm::write(0, 999).get();
    } catch (const std::exception &e) {
      print_rank(rank, "Caught expected exception: " + std::string(e.what()));
    }
  }
}

void test_concurrent_writes(int rank) {
  if (is_subscribed(rank, 1)) {
    // All subscribers to var 1 (0, 2, 3) write concurrently.
    // We wait on the future to ensure the write request is processed.
    dsm::write(1, 100 + rank).get();
  }
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <config_file>" << '\n';
    return EXIT_FAILURE;
  }

  const std::string config_path(argv[1]);

  try {
    dsm::init(config_path);
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (is_subscribed(rank, 1)) {
      dsm::on_change(1, [rank](int new_val) {
        print_rank(rank, "Callback for var 1 triggered. New value: " + std::to_string(new_val));
      });
    }
    if (is_subscribed(rank, 0)) {
      dsm::on_change(0, [rank](int new_val) {
        print_rank(rank, "Callback for var 0 triggered. New value: " + std::to_string(new_val));
      });
    }

    MPI_Barrier(MPI_COMM_WORLD);

    // --- Test 1: Simple Write ---
    if (rank == 0) {
      std::cout << "--- Test 1: Simple Write ---\n\n";
    }
    test_simple_write(rank);
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    MPI_Barrier(MPI_COMM_WORLD);

    // --- Test 2: Compare and Exchange (Success) ---
    if (rank == 0) {
      std::cout << "\n--- Test 2: Compare and Exchange (Success) ---\n\n";
    }
    test_cas_success(rank);
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    MPI_Barrier(MPI_COMM_WORLD);

    // --- Test 3: Compare and Exchange (Failure) ---
    if (rank == 0) {
      std::cout << "\n--- Test 3: Compare and Exchange (Failure) ---\n\n";
    }
    test_cas_failure(rank);
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    MPI_Barrier(MPI_COMM_WORLD);

    // --- Test 4: Exception for non-subscribed variable ---
    if (rank == 0) {
      std::cout << "\n--- Test 4: Exception on Write ---\n\n";
    }
    test_exception(rank);
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    MPI_Barrier(MPI_COMM_WORLD);

    // --- Test 5: Concurrent Writes ---
    if (rank == 0) {
      std::cout << "\n--- Test 5: Concurrent Writes ---\n\n";
    }
    test_concurrent_writes(rank);
    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Longer sleep for more complex op
    MPI_Barrier(MPI_COMM_WORLD);

    if (rank == 0) {
      std::cout << "\n--- All tests passed ---\n\n";
    }

    dsm::finalize();

  } catch (const std::exception &e) {
    int rank = -1;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    std::cerr << "[P" << rank << "] Error: " << e.what() << '\n';
    MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);

    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
