#include "config.hpp"

namespace dsm::internal {

Config get_example_config() {
  return {
    .num_variables = 3,
    .subscriptions = {
      {0, {0, 1}},
      {1, {0, 2, 3}},
      {2, {1, 3}}
    }
  };
}

} // namespace dsm::internal
