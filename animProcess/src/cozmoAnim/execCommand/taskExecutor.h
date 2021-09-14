/**
 * File: taskExecutor
 *
 * Author: seichert
 * Created: 01/29/2018
 *
 * Description: Execute arbitrary tasks on a background thread serially.
 * Uses libev to signal the background thread to process its queue.
 *
 * Based on original work from Michael Sung on May 30, 2014, 10:10 AM
 *
 * Copyright: Anki, Inc. 2014-2018
 *
 **/
#ifndef __TaskExecutor_H__
#define	__TaskExecutor_H__

#include "ev++.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

namespace Anki
{

typedef struct _TaskHolder {
  bool sync;
  std::function<void()> task;
  std::chrono::time_point<std::chrono::steady_clock> when;

  bool operator < (const _TaskHolder& th) const
  {
    return (when > th.when);
  }
} TaskHolder;

class TaskExecutor {
public:
  TaskExecutor()
      :TaskExecutor(nullptr) { }
  TaskExecutor(struct ev_loop* loop);
  ~TaskExecutor();
  void Wake(std::function<void()> task);
  void WakeSync(std::function<void()> task);
  void WakeAfter(std::function<void()> task, std::chrono::time_point<std::chrono::steady_clock> when);
  void StopExecution();
protected:
  TaskExecutor(const TaskExecutor&) = delete;
  TaskExecutor& operator=(const TaskExecutor&) = delete;

private:
  void AddTaskHolder(TaskHolder taskHolder);
  void AddTaskHolderToDeferredQueue(TaskHolder taskHolder);
  void InitWatchers();
  void DestroyWatchers();
  void Execute();
  void ProcessDeferredQueue();
  void ProcessTaskQueue();
  void CommonCallback();
  void PipeWatcherCallback(ev::io& w, int revents);
  void TimerWatcherCallback(ev::timer& w, int revents);
  void WakeUpBackgroundThread(const char c = 'x');

private:
  struct ev_loop* _loop;
  std::thread::id _loop_thread_id;
  int _pipeFileDescriptors[2];
  ev::io* _pipeWatcher;
  ev::timer* _timerWatcher;
  std::thread* _taskExecuteThread;
  std::mutex _taskQueueMutex;
  std::vector<TaskHolder> _taskQueue;
  std::mutex _taskDeferredQueueMutex;
  std::vector<TaskHolder> _deferredTaskQueue;
  std::mutex _addSyncTaskMutex;
  std::mutex _syncTaskCompleteMutex;
  std::condition_variable _syncTaskCondition;
  bool _syncTaskDone;
  bool _executing;
};

} // namespace Anki

#endif	/* __TaskExecutor_H__ */
