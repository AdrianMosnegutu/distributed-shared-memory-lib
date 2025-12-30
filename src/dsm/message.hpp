#pragma once

#include <cstdint>
#include <functional>

namespace dsm {

enum class MessageType : uint8_t {

  WRITE_REQUEST,

  CAS_REQUEST,

  WRITE_ACK,

  CAS_ACK,

  SHUTDOWN

};

struct Timestamp {
  int clock;
  int rank;

  auto operator<=>(const Timestamp &) const = default;
};

struct Message {
  MessageType type;
  Timestamp ts;
  int var_id;
  int value1; // Used for write value or expected CAS value
  int value2; // Used for new CAS value
};

} // namespace dsm

namespace std {

template <> struct hash<dsm::Timestamp> {
  size_t operator()(const dsm::Timestamp &ts) const {
    // A simple hash combination function.
    // Shift the first hash and XOR it with the second.
    return (hash<int>()(ts.clock) << 1) ^ hash<int>()(ts.rank);
  }
};

} // namespace std
