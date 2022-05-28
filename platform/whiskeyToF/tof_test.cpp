
#define RELEASE

#include "whiskeyToF/tof.h"

#include "util/logging/logging.h"

#include <iomanip>
#include <inttypes.h>
#include <unistd.h>
#include <chrono>
#include <csignal>
#include <thread>

#ifdef PRINT_NAMED_ERROR
#undef PRINT_NAMED_ERROR
#endif
#define PRINT_NAMED_ERROR(a, b, ...) printf(a b "\n", ##__VA_ARGS__);

#ifdef PRINT_NAMED_WARNING
#undef PRINT_NAMED_WARNING
#endif
#define PRINT_NAMED_WARNING(a, b, ...) printf(a b "\n", ##__VA_ARGS__);

#ifdef PRINT_NAMED_INFO
#undef PRINT_NAMED_INFO
#endif
#define PRINT_NAMED_INFO(a, b, ...) printf(a b "\n", ##__VA_ARGS__);

using namespace Anki::Vector;

uint32_t GetTimeStamp(void)
{
  auto currTime = std::chrono::steady_clock::now();
  return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(currTime.time_since_epoch()).count());
}

namespace
{
int shutdown = 0;
}

static void Shutdown(int signum)
{
  printf("shutdown\n");
  shutdown = signum;
}

int main(int argc, char** argv)
{
  signal(SIGTERM, Shutdown);
  signal(SIGINT, Shutdown);
  
  bool pause = false;
  
  ToFSensor::getInstance()->SetupSensors([](ToFSensor::CommandResult res)
                                         {
                                           if((int)res < 0)
                                           {
                                             printf("Failed to setup\n");
                                             exit(1);
                                           }
                                         });

  if(argc > 1)
  {
    if(argv[1][0] == 'c')
    {
      uint32_t dist = 0;
      float reflectance = 0.f;
      
      if(argc > 2)
      {
        char* end;
        dist = (uint32_t)strtoimax(argv[2], &end, 10);
      }
      
      if(argc > 3)
      {
        char* end;
        reflectance = strtof(argv[3], &end);
      }

      PRINT_NAMED_INFO("ToFTest",
                       "Calibrating at %u with reflectance %f",
                       dist,
                       reflectance);
      
      ToFSensor::getInstance()->PerformCalibration(dist, reflectance, nullptr);

      ToFSensor::getInstance()->SetupSensors(nullptr);
    }
    else if(argv[1][0] == 'p')
    {
      pause = true;
    }
  }

  ToFSensor::getInstance()->StartRanging([](ToFSensor::CommandResult res)
                                         {
                                           if((int)res < 0)
                                           {
                                             printf("Failed to start ranging\n");
                                             exit(1);
                                           }
                                         });

  while(shutdown == 0)
  {
    bool isUpdated = false;
    RangeDataRaw data = ToFSensor::getInstance()->GetData(isUpdated);

    static uint32_t s = GetTimeStamp();
    if(pause && GetTimeStamp() - s > 3000)
    {
      s = GetTimeStamp();
      static bool b = false;
      if(b)
      {
        printf("STARTING\n");
        ToFSensor::getInstance()->StartRanging([](ToFSensor::CommandResult res)
                                         {
                                           if((int)res < 0)
                                           {
                                             printf("Failed to start ranging\n");
                                             exit(1);
                                           }
                                         });
      }
      else
      {
        printf("STOPPING\n");
        ToFSensor::getInstance()->StopRanging([](ToFSensor::CommandResult res)
                                         {
                                           if((int)res < 0)
                                           {
                                             printf("Failed to stop ranging\n");
                                             exit(1);
                                           }
                                         });
      }
      b = !b;
    }

    
    if(!isUpdated)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
      continue;
    }

    static RangeDataRaw lastValid = data;
      
    std::stringstream ss;
    for(int i = 0; i < 4; i++)
    {
      for(int j = 0; j < 4; j++)
      {
        char status = 0;
        if(data.data[i*4 + j].numObjects > 0)
        {
          status = data.data[i*4 + j].readings[0].status;

          if(data.data[i*4 + j].readings[0].status == 0)
          {
            ss << std::setw(7) << (uint32_t)(data.data[i*4 + j].processedRange_mm);
            lastValid.data[i*4 + j] = data.data[i*4 + j];
          }
          else
          {
            ss << std::setw(7) << (uint32_t)(lastValid.data[i*4 + j].processedRange_mm);
          }
        }
        else
        {
          ss << std::setw(7) << (uint32_t)(lastValid.data[i*4 + j].processedRange_mm);
          status = -1;
        }
        
        ss << "[" << std::setw(2) << (int)status << "]";
      }
      ss << "\n";
    }
    printf("%s\n", ss.str().c_str());    
  }

  printf("stopping\n");
  ToFSensor::getInstance()->StopRanging([](ToFSensor::CommandResult res)
                                         {
                                           if((int)res < 0)
                                           {
                                             printf("Failed to stop ranging\n");
                                             exit(1);
                                           }
                                         });

  ToFSensor::removeInstance();
  
  return 0;
}


