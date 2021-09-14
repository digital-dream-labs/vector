/**
 * File: taskExecutor
 *
 * Author: seichert
 * Created: 07/15/14
 *
 * Description: Execute arbitrary tasks on
 * a background thread serially.
 *
 * Based on original work from Michael Sung on May 30, 2014, 10:10 AM
 *
 * Copyright: Anki, Inc. 2014
 *
 **/

#include "taskExecutor.h"
#include <algorithm>
#include <fcntl.h>
#include <unistd.h>

namespace Anki
{

TaskExecutor::TaskExecutor(struct ev_loop* loop)
    : _loop(loop)
    , _pipeFileDescriptors{-1, -1}
    , _pipeWatcher(nullptr)
    , _timerWatcher(nullptr)
    , _taskExecuteThread(nullptr)
    , _syncTaskDone(false)
    , _executing(true)
{
  // TODO: Should really be checking return values
#if defined(ANKI_PLATFORM_LINUX) || defined(ANKI_PLATFORM_VICOS)
  (void) pipe2(_pipeFileDescriptors, O_NONBLOCK);
#else
  (void) pipe(_pipeFileDescriptors);
  (void) fcntl(_pipeFileDescriptors[0], F_SETFL, O_NONBLOCK);
  (void) fcntl(_pipeFileDescriptors[1], F_SETFL, O_NONBLOCK);
#endif
  if (loop) {
    InitWatchers();
  } else {
    _taskExecuteThread = new std::thread(&TaskExecutor::Execute, this);
  }
}

TaskExecutor::~TaskExecutor()
{
  StopExecution();
  if (_pipeFileDescriptors[1] >= 0) {
    (void) close(_pipeFileDescriptors[1]); _pipeFileDescriptors[1] = -1;
  }
  if (_pipeFileDescriptors[0] >= 0) {
    (void) close(_pipeFileDescriptors[0]); _pipeFileDescriptors[0] = -1;
  }
}

void TaskExecutor::WakeUpBackgroundThread(const char c)
{
  if (_pipeFileDescriptors[1] >= 0) {
    char buf[1] = {c};
    (void) write(_pipeFileDescriptors[1], buf, sizeof(buf));
  }
}

void TaskExecutor::StopExecution()
{
  // Cause Execute and ProcessDeferredQueue to break out of their while loops
  _executing = false;

  // Clear the _taskQueue.  Use a scope so that the mutex is only locked
  // while clearing the queue and notifying the background thread.
  {
    std::lock_guard<std::mutex> lock(_taskQueueMutex);
    _taskQueue.clear();
  }

  // Clear the _deferredTaskQueue and also use a scope.
  {
    std::lock_guard<std::mutex> lock(_taskDeferredQueueMutex);
    _deferredTaskQueue.clear();
  }

  // Wake up the background thread so that it exits
  WakeUpBackgroundThread('q');

  // Join the background threads.  We created the threads in the constructor, so they
  // should be cleaned up in our destructor.
  try {
    if (_taskExecuteThread && _taskExecuteThread->joinable()) {
      _taskExecuteThread->join();
    }
  } catch ( ... )
  {
    // Suppress exceptions
  }
  DestroyWatchers();
}

void TaskExecutor::Wake(std::function<void()> task)
{
  if (!_executing) {
    return;
  }
  WakeAfter(std::move(task), std::chrono::time_point<std::chrono::steady_clock>::min());
}

void TaskExecutor::WakeSync(std::function<void()> task)
{
  if (!_executing) {
    return;
  }
  if (std::this_thread::get_id() == _loop_thread_id) {
    task();
    return;
  }
  std::lock_guard<std::mutex> lock(_addSyncTaskMutex);
  if (!_executing) {
    return;
  }

  TaskHolder taskHolder;
  taskHolder.sync = true;
  taskHolder.task = std::move(task);
  taskHolder.when = std::chrono::time_point<std::chrono::steady_clock>::min();
  _syncTaskDone = false;

  AddTaskHolder(std::move(taskHolder));

  std::unique_lock<std::mutex> lk(_syncTaskCompleteMutex);
  _syncTaskCondition.wait(lk, [this]{return _syncTaskDone || !_executing;});
}

void TaskExecutor::WakeAfter(std::function<void()> task, std::chrono::time_point<std::chrono::steady_clock> when)
{
  if (!_executing) {
    return;
  }
  TaskHolder taskHolder;
  taskHolder.sync = false;
  taskHolder.task = std::move(task);
  taskHolder.when = when;

  auto now = std::chrono::steady_clock::now();
  if (now >= when) {
    AddTaskHolder(std::move(taskHolder));
  } else {
    AddTaskHolderToDeferredQueue(std::move(taskHolder));
  }
}

void TaskExecutor::AddTaskHolder(TaskHolder taskHolder)
{
  std::lock_guard<std::mutex> lock(_taskQueueMutex);
  if (!_executing) {
    return;
  }
  _taskQueue.push_back(std::move(taskHolder));
  WakeUpBackgroundThread();
}

void TaskExecutor::AddTaskHolderToDeferredQueue(TaskHolder taskHolder)
{
  std::lock_guard<std::mutex> lock(_taskDeferredQueueMutex);
  if (!_executing) {
    return;
  }
  _deferredTaskQueue.push_back(std::move(taskHolder));
  // Sort the tasks so that the next one due is at the back of the queue
  std::sort(_deferredTaskQueue.begin(), _deferredTaskQueue.end());
  WakeUpBackgroundThread();
}

void TaskExecutor::CommonCallback() {
  if (_executing) {
    ProcessTaskQueue();
    ProcessDeferredQueue();
  } else {
    if (_loop && _taskExecuteThread) {
      ev_unloop(_loop, EVUNLOOP_ALL);
    }
  }
}

void TaskExecutor::PipeWatcherCallback(ev::io& w, int revents)
{
  if (revents & ev::READ) {
    char buf[1];
    ssize_t bytesRead;
    do {
      bytesRead = read(w.fd, buf, sizeof(buf));
    } while (bytesRead > 0);
  }
  CommonCallback();
}

void TaskExecutor::TimerWatcherCallback(ev::timer& w, int revents)
{
  CommonCallback();
}

void TaskExecutor::InitWatchers()
{
  _loop_thread_id = std::this_thread::get_id();
  _pipeWatcher = new ev::io(_loop);
  _pipeWatcher->set <TaskExecutor, &TaskExecutor::PipeWatcherCallback> (this);
  _timerWatcher = new ev::timer(_loop);
  _timerWatcher->set <TaskExecutor, &TaskExecutor::TimerWatcherCallback> (this);
  _pipeWatcher->start (_pipeFileDescriptors[0], ev::READ);
}

void TaskExecutor::DestroyWatchers()
{
  delete _timerWatcher; _timerWatcher = nullptr;
  delete _pipeWatcher; _pipeWatcher = nullptr;
}

void TaskExecutor::Execute()
{
  _loop = ev_loop_new(EVBACKEND_SELECT);
  InitWatchers();
  ev_loop(_loop, 0);
  DestroyWatchers();
  ev_loop_destroy(_loop); _loop = nullptr;
}

void TaskExecutor::ProcessTaskQueue()
{
  std::vector<TaskHolder> taskQueue;
  {
    // Briefly lock the mutex to move the task queue to a local variable.
    // This prevents us from blocking other threads that want to add to
    // the task queue.
    std::lock_guard<std::mutex> lock(_taskQueueMutex);
    taskQueue = std::move(_taskQueue);
    _taskQueue.clear();
  }
  for (auto const& taskHolder : taskQueue) {
    if (_executing) {
      taskHolder.task();
      if (taskHolder.sync) {
        std::lock_guard<std::mutex> lk(_syncTaskCompleteMutex);
        _syncTaskDone = true;
        _syncTaskCondition.notify_one();
      }
    }
  }
}

void TaskExecutor::ProcessDeferredQueue()
{
  std::lock_guard<std::mutex> lock(_taskDeferredQueueMutex);
  bool endLoop = false;
  while (_executing && !_deferredTaskQueue.empty() && !endLoop) {
    auto now = std::chrono::steady_clock::now();
    auto& taskHolder = _deferredTaskQueue.back();
    if (now >= taskHolder.when) {
      AddTaskHolder(std::move(taskHolder));
      _deferredTaskQueue.pop_back();
    } else {
      endLoop = true;
      using ev_tstamp_duration = std::chrono::duration<ev_tstamp, std::ratio<1, 1>>;
      ev_tstamp_duration duration =
          std::chrono::duration_cast<ev_tstamp_duration>(taskHolder.when - std::chrono::steady_clock::now());
      ev_tstamp after = duration.count();
      _timerWatcher->start(after);
    }
  }
}

} // namespace Anki
