/**
 * File: threadedPrintStressTester.h
 *
 * Author: Brad Neuman
 * Created: 2017-01-18
 *
 * Description: Stress tester to print a bunch of log messages from different threads
 *
 * Copyright: Anki, Inc. 2017
 *
 **/

#ifndef __Cozmo_Basestation_ThreadedPrintStressTester_H__
#define __Cozmo_Basestation_ThreadedPrintStressTester_H__

#include <chrono>
#include <future>
#include <sstream>
#include <thread>

#include "util/console/consoleInterface.h"
#include "util/logging/logging.h"
#include "util/random/randomGenerator.h"

namespace Anki {
namespace Vector {

namespace {
CONSOLE_VAR(int, kStressTest_numThreads, "RobotDataLoader", 5);
}

class ThreadedPrintStressTester
{
public:

  // NOTE: this can only be used once. If you want to use it multiple times, you'd need to create a new tester
  // object....
  
  void Start() {
    std::shared_future<void> stopTestFuture( _stopTestPromise.get_future() );

    const int numThreads = kStressTest_numThreads;
    
    _threadStarted.resize(numThreads);
      
    for(int i=0; i<numThreads; ++i) {
      _threadStopped.emplace_back(
        std::async(std::launch::async, std::bind(&ThreadedPrintStressTester::Worker, this, i, stopTestFuture)) );
    }

    // wait for them to start
    for(int i=0; i<numThreads; ++i) {
      PRINT_CH_INFO("Loading", "StartStressTest.WaitForThread",
                    "Waiting for thread %d to start", i);
      _threadStarted[i].get_future().wait();
    }

    PRINT_CH_INFO("Loading", "StartStressTest.ThreadsStarted",
                  "Started %d worker threads", numThreads);    
    
  }
      

  void Stop() {
    PRINT_CH_INFO("Loading", "StartStressTest.StopTest",
                  "Sending message to stop test");

    _stopTestPromise.set_value();

    const size_t numThreadsRunning = _threadStopped.size();
    for( size_t i = 0; i < numThreadsRunning; ++i ) {
      PRINT_CH_INFO("Loading", "StopStressTest.WaitForThread",
                    "Waiting for thread %zu to stop", i);
      _threadStopped[i].wait();
    }

    PRINT_CH_INFO("Loading", "StopStressTest.Done",
                  "All %zu tests stopped", numThreadsRunning);

    _threadStarted.clear();
    _threadStopped.clear();
  }


private:

  std::promise<void> _stopTestPromise;

  std::vector< std::promise<void> > _threadStarted;

  std::vector< std::future<void> > _threadStopped;

  void Worker(int workerID, std::shared_future<void> stopTestFuture) {
    // tell main thread we are done
    _threadStarted[workerID].set_value();

    std::stringstream threadIDSS;
    threadIDSS << std::this_thread::get_id();
    
    PRINT_CH_INFO("Loading", "StressTestWorker",
                  "Started worker id %d on thread %s",
                  workerID,
                  threadIDSS.str().c_str());

    // use a custom rng, seeded with thread id
    Anki::Util::RandomGenerator rng( workerID );

    int printCount = 0;

    while( true ) {
      auto timeToWait = std::chrono::milliseconds( rng.RandInt(10) );

      if( stopTestFuture.wait_for(timeToWait) == std::future_status::ready ) {
        PRINT_CH_INFO("Loading", "StressTestWorkerStop",
                      "Stopping worker %d in thread %s",
                      workerID,
                      threadIDSS.str().c_str());
        return;
      }

      const char* channel = rng.RandDbl() < 0.5 ? "Unfiltered" : "Unnamed";
      
      switch( rng.RandInt(3) ) {
        case 0: {
          PRINT_CH_DEBUG(channel, "StressTest.WorkerPrint",
                         "worker %d in thread %s: %d",
                         workerID,
                         threadIDSS.str().c_str(),
                         printCount++);
          break;
        }

        case 1: {
          PRINT_CH_INFO(channel, "StressTest.WorkerPrint",
                        "worker %d in thread %s: %d",
                        workerID,
                        threadIDSS.str().c_str(),
                        printCount++);
          break;
        }

        case 2: {
          PRINT_NAMED_WARNING("StressTest.WorkerPrint",
                              "worker %d in thread %s: %d",
                              workerID,
                              threadIDSS.str().c_str(),
                              printCount++);
          break;
        }

        default: break;
      }
    }
  }

};

}
}

#endif
