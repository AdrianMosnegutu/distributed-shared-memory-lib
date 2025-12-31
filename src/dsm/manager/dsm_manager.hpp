#include "dsm/config/config.hpp"
#include "dsm/data_structures/blocking_queue.hpp"
#include "dsm/messages/message.hpp"
#include "dsm/promises/promise_manager.hpp"
#include "dsm/threads/consumer.hpp"
#include "dsm/threads/producer.hpp"
#include <functional>
#include <future>
#include <memory>
#include <mpi.h>
#include <set>
#include <unordered_map>

namespace dsm::internal {

class DistributedSharedVariable; // Forward declaration

class DSMManager {
public:
  explicit DSMManager(int rank, int world_size, Config config);
  ~DSMManager();

  const std::unordered_map<int, std::set<int>> &get_subscriptions() const;

  void register_callback(int var_id, std::function<void(int)> callback);

  std::future<void> write(int var_id, int value);
  std::future<bool> compare_and_exchange(int var_id, int expected_value, int new_value);

private:
  void on_cas_result(const Timestamp &ts, bool success);
  void on_write_result(const Timestamp &ts);

  int rank_;
  int world_size_;
  Config config_;

  BlockingQueue<Message> message_queue_;
  std::unique_ptr<Producer> producer_;
  std::unique_ptr<Consumer> consumer_;
  std::unique_ptr<PromiseManager> promise_manager_;

  std::unordered_map<int, DistributedSharedVariable> variables_;

  MPI_Datatype mpi_message_type_;
  MPI_Datatype mpi_timestamp_type_;
};

} // namespace dsm::internal
