cmake_minimum_required(VERSION 3.16)
project(distributed-shared-memory CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(MPI REQUIRED)
find_package(Threads REQUIRED)

add_library(dsm STATIC
    src/dsm/dsm.cpp
    src/dsm/config.cpp
    src/dsm/dsm_manager.cpp
    src/dsm/distributed_shared_variable.cpp
    src/dsm/listener.cpp
)
target_include_directories(dsm PUBLIC src vendor)
target_link_libraries(dsm PRIVATE Threads::Threads)

add_executable(main src/main.cpp)
target_link_libraries(main dsm MPI::MPI_CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
