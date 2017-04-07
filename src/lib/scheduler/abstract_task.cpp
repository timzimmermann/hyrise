#include "abstract_task.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "current_scheduler.hpp"
#include "worker.hpp"

namespace opossum {

TaskID AbstractTask::id() const { return _id; }

NodeID AbstractTask::node_id() const { return _node_id; }

bool AbstractTask::is_ready() const { return _predecessor_counter == 0; }

bool AbstractTask::is_done() const { return _done; }

bool AbstractTask::is_scheduled() const { return _is_scheduled; }

std::string AbstractTask::description() const {
  return _description.empty() ? "{Task with id: " + std::to_string(_id) + "}" : _description;
}

void AbstractTask::set_description(const std::string& description) {
  if (IS_DEBUG && _is_scheduled) {
    throw std::logic_error("Possible race: Don't set description after the Task was scheduled");
  }

  _description = description;
}

void AbstractTask::set_id(TaskID id) { _id = id; }

void AbstractTask::set_as_predecessor_of(std::shared_ptr<AbstractTask> successor) {
  if (IS_DEBUG && _is_scheduled) {
    throw std::logic_error("Possible race: Don't set dependencies after the Task was scheduled");
  }
  successor->_on_predecessor_added();
  _successors.emplace_back(successor);
}

void AbstractTask::set_node_id(NodeID node_id) { _node_id = node_id; }

void AbstractTask::mark_as_scheduled() {
  auto already_scheduled = _is_scheduled.exchange(true);

  if (IS_DEBUG && already_scheduled) {
    throw std::logic_error("Task was already scheduled!");
  }
}

bool AbstractTask::try_mark_as_enqueued() { return !_is_enqueued.exchange(true); }

void AbstractTask::set_done_callback(const std::function<void()>& done_callback) {
  if (IS_DEBUG && _is_scheduled) {
    throw std::logic_error("Possible race: Don't set callback after the Task was scheduled");
  }

  _done_callback = done_callback;
}

void AbstractTask::schedule(NodeID preferred_node_id, SchedulePriority priority) {
  mark_as_scheduled();

  if (CurrentScheduler::is_set()) {
    CurrentScheduler::get()->schedule(shared_from_this(), preferred_node_id, priority);
  } else {
    // If the Task isn't ready, it will execute() once its dependency counter reaches 0
    if (is_ready()) execute();
  }
}

void AbstractTask::join() {
  if (IS_DEBUG && !_is_scheduled) {
    throw std::logic_error("Task must be scheduled before it can be waited for");
  }

  /**
   * When join() is called from a Task, i.e. from a Worker Thread, let the worker handle the join()-ing (via
   * _wait_for_tasks()), otherwise just join right here
   */
  if (CurrentScheduler::is_set()) {
    auto worker = Worker::get_this_thread_worker();
    worker->_wait_for_tasks({shared_from_this()});
  } else {
    _join_without_replacement_worker();
  }
}

void AbstractTask::_join_without_replacement_worker() {
  std::unique_lock<std::mutex> lock(_done_mutex);
  _done_condition_variable.wait(lock, [&]() { return _done; });
}

void AbstractTask::execute() {
#if IS_DEBUG
  auto already_started = _started.exchange(true);
  if (already_started) {
    throw std::logic_error("Possible bug: Trying to execute the same task twice");
  }

  if (!is_ready()) {
    throw std::logic_error("Task must not be executed before its dependencies are done");
  }
#endif

  on_execute();

  for (auto& successor : _successors) {
    successor->_on_predecessor_done();
  }

  if (_done_callback) _done_callback();

  {
    std::unique_lock<std::mutex> lock(_done_mutex);
    _done = true;
  }
  _done_condition_variable.notify_all();
}

void AbstractTask::_on_predecessor_added() { _predecessor_counter++; }

void AbstractTask::_on_predecessor_done() {
  auto new_predecessor_count = --_predecessor_counter;  // atomically decrement
  if (new_predecessor_count == 0) {
    if (CurrentScheduler::is_set()) {
      auto worker = Worker::get_this_thread_worker();
      if (IS_DEBUG && !worker) {
        throw std::logic_error("No worker");
      }

      worker->queue()->push(shared_from_this(), static_cast<uint32_t>(SchedulePriority::High));
    } else {
      if (_is_scheduled) execute();
      // Otherwise it will get execute()d once it is scheduled. It is entirely possible for Tasks to "become ready"
      // before they are being scheduled in a no-Scheduler context. Think:
      //
      // task1->set_as_predecessor_of(task2);
      // task2->set_as_predecessor_of(task3);
      //
      // task3->schedule(); <-- Does nothing
      // task1->schedule(); <-- Executes Task1, Task2 becomes ready but is not executed, since it is not yet scheduled
      // task2->schedule(); <-- Executes Task2, Task3 becomes ready, executes Task3
    }
  }
}

}  // namespace opossum