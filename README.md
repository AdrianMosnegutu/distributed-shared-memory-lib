# C++ Distributed Shared Memory (DSM)

This project is a C++ implementation of a Distributed Shared Memory (DSM) system. It provides a high-level abstraction for sharing variables among a set of distributed processes, simplifying parallel programming by eliminating the need for manual message passing. The communication between processes is handled by the Message Passing Interface (MPI).

## Features

*   **Shared Variables:** Allows processes to share variables of any type that supports serialization.
*   **Consistency:** Implements a Total Order Broadcast consistency model using Lamport timestamps to ensure that all processes see the same sequence of updates.
*   **Asynchronous API:** Provides a non-blocking, asynchronous API for write and compare-and-swap (CAS) operations using `std::future` and `std::promise`.
*   **Callback System:** Supports event-driven programming with callbacks for value changes and CAS operation resolutions.
*   **Producer-Consumer Architecture:** Decouples message reception from processing using a thread-safe blocking queue, improving performance and responsiveness.

## Build and Run

### Prerequisites

*   A C++20 compliant compiler
*   CMake (version 3.16 or higher)
*   An MPI implementation (e.g., OpenMPI, MPICH)

### Build

To build the project, run the following command from the root directory:

```bash
make build
```

This will create the `build` directory and compile the source code.

### Run

The application is executed using MPI. The `Makefile` provides a convenient `run` target that reads the number of processes from the configuration file and starts the application.

```bash
make run
```

This is equivalent to a command like:
`mpirun -np 4 --oversubscribe ./build/main config/dsm_config.json`

## Implementation Details

The DSM system is built around a central `DSMManager` that runs on each process. This manager is responsible for:

*   Managing the lifecycle of shared variables (`DistributedSharedVariable`).
*   Handling inter-process communication via MPI.
*   Coordinating updates to ensure consistency.

### Consistency Model

The system uses a Total Order Broadcast protocol based on Lamport timestamps to guarantee that all write and CAS operations are applied in the same order on all participating processes. This is achieved through a request-acknowledgment mechanism:

1.  A process wishing to modify a variable broadcasts a request message to all subscribers of that variable.
2.  Upon receiving a request, each process adds it to a local priority queue (ordered by Lamport timestamp) and broadcasts an acknowledgment message.
3.  A request is only processed once it has received acknowledgments from all other subscribers, ensuring that all processes have seen the same set of requests before any of them are applied.

### Concurrency

The system is multi-threaded and uses a producer-consumer model to handle MPI messages. A dedicated producer thread listens for incoming MPI messages and places them into a thread-safe blocking queue. A consumer thread then processes these messages, applying updates to the shared variables. This design separates the concerns of network communication and application logic, leading to a more efficient and scalable system.
