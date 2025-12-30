#pragma once

#include <cstdint>

namespace dsm {

enum class MessageType : uint8_t { WRITE_REQUEST, CAS_REQUEST, WRITE_ACK, CAS_ACK };

struct Timestamp {
  int clock;
  int rank;

  bool operator>(const Timestamp &other) const {
    if (clock != other.clock) {
      return clock > other.clock;
    }
    return rank > other.rank;
  }

  bool operator<(const Timestamp &other) const {
    if (clock != other.clock) {
      return clock < other.clock;
    }
    return rank < other.rank;
  }
};

struct Message {
  MessageType type;
  Timestamp ts;
  int var_id;
  int value1; // Used for write value or expected CAS value
  int value2; // Used for new CAS value
};

} // namespace dsm
