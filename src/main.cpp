#include "dsm/dsm.hpp"
#include <functional>
#include <iostream>
#include <map>
#include <mpi.h>
#include <thread>
#include <vector>

void print_rank(int rank, const std::string &msg) {
  std::cout << "[P" << rank << "] " << msg << '\n';
}

void test_simple_write(int rank) {
  if (rank == 0) {
    print_rank(rank, "Writing 42 to var 1.");
    dsm::write(1, 42);
  }
}

void test_cas_success(int rank) {
  if (rank == 3) {
    print_rank(rank, "Attempting CAS on var 1 (expect 42, new 100).");
    auto future = dsm::compare_and_exchange_async(1, 42, 100);

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
    auto future = dsm::compare_and_exchange_async(1, 50, 200);

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
      dsm::write(0, 999);
    } catch (const std::exception &e) {
      print_rank(rank, "Caught expected exception: " + std::string(e.what()));
    }
  }
}

void test_concurrent_writes(int rank, const std::function<bool(int)> &is_subscribed) {
  if (is_subscribed(1)) {
    // All subscribers to var 1 (0, 2, 3) write concurrently.
    // The value from the process with the highest rank (3) should be the final one.
    dsm::write(1, 100 + rank);
  }
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <config_file>" << '\n';
    return 1;
  }

  try {
    dsm::init(argv[1]);
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // Manually define subscriptions for the test based on config file
    std::map<int, std::vector<int>> subs = {{0, {0, 1}}, {1, {0, 2, 3}}, {2, {1, 3}}};
    auto is_subscribed = [&](int var_id) {
      for (int r : subs.at(var_id)) {
        if (r == rank) {
          return true;
        }
      }
      return false;
    };

    if (is_subscribed(1)) {
      dsm::on_change(1, [rank](int new_val) {
        print_rank(rank, "Callback for var 1 triggered. New value: " + std::to_string(new_val));
      });
    }
    if (is_subscribed(0)) {
      dsm::on_change(0, [rank](int new_val) {
        print_rank(rank, "Callback for var 0 triggered. New value: " + std::to_string(new_val));
      });
    }

    MPI_Barrier(MPI_COMM_WORLD);

    // --- Test 1: Simple Write ---
    test_simple_write(rank);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    MPI_Barrier(MPI_COMM_WORLD);
    if (rank == 0) {
      std::cout << "\n--- End of Test 1 ---\n" << '\n';
    }
    MPI_Barrier(MPI_COMM_WORLD);

    // --- Test 2: Compare and Exchange (Success) ---
    test_cas_success(rank);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    MPI_Barrier(MPI_COMM_WORLD);
    if (rank == 0) {
      std::cout << "\n--- End of Test 2 ---\n" << '\n';
    }
    MPI_Barrier(MPI_COMM_WORLD);

    // --- Test 3: Compare and Exchange (Failure) ---
    test_cas_failure(rank);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    MPI_Barrier(MPI_COMM_WORLD);
    if (rank == 0) {
      std::cout << "\n--- End of Test 3 ---\n" << '\n';
    }
    MPI_Barrier(MPI_COMM_WORLD);

    // --- Test 4: Exception for non-subscribed variable ---
    test_exception(rank);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    MPI_Barrier(MPI_COMM_WORLD);
    if (rank == 0) {
      std::cout << "\n--- End of Test 4 ---\n" << '\n';
    }
    MPI_Barrier(MPI_COMM_WORLD);

    // --- Test 5: Concurrent Writes ---
    if (rank == 0) {
      print_rank(rank, "Testing concurrent writes to var 1.");
    }
    test_concurrent_writes(rank, is_subscribed);
    std::this_thread::sleep_for(std::chrono::seconds(2));
    MPI_Barrier(MPI_COMM_WORLD);
    if (rank == 0) {
      std::cout << "\n--- End of Test 5 ---\n" << '\n';
    }

    dsm::finalize();

  } catch (const std::exception &e) {
    int rank = -1;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    std::cerr << "[P" << rank << "] Error: " << e.what() << '\n';
    MPI_Abort(MPI_COMM_WORLD, 1);
    return 1;
  }

  return 0;
}
