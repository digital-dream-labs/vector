/**
 * File: perfMetricAnim
 *
 * Author: Paul Terry
 * Created: 11/28/2018
 *
 * Description: Lightweight performance metric recording: for vic-anim
 *
 * Copyright: Anki, Inc. 2018
 *
 **/


#include "cozmoAnim/animation/animationStreamer.h"
#include "cozmoAnim/animContext.h"
#include "cozmoAnim/animProcessMessages.h"
#include "cozmoAnim/perfMetricAnim.h"
#include "util/cpuProfiler/cpuProfiler.h"
#include "util/perfMetric/perfMetricImpl.h"
#include "util/stats/statsAccumulator.h"


#define LOG_CHANNEL "PerfMetric"

namespace Anki {
namespace Vector {




PerfMetricAnim::PerfMetricAnim(const Anim::AnimContext* context)
  : _frameBuffer(nullptr)
{
  _headingLine1 = "                       Anim     Anim    Sleep    Sleep     Over      RtA   AtR   EtA   AtE  Anim Layer";
  _headingLine2 = "                   Duration     Freq Intended   Actual    Sleep    Count Count Count Count  Time Count";
  _headingLine2Extra = "";
  _headingLine1CSV = ",,Anim,Anim,Sleep,Sleep,Over,RtA,AtR,EtA,AtE,Anim,Layer";
  _headingLine2CSV = ",,Duration,Freq,Intended,Actual,Sleep,Count,Count,Count,Count,Time,Count";
  _headingLine2ExtraCSV = "";
}

PerfMetricAnim::~PerfMetricAnim()
{
#if ANKI_PERF_METRIC_ENABLED
  OnShutdown();

  delete[] _frameBuffer;
#endif
}

void PerfMetricAnim::Init(Util::Data::DataPlatform* dataPlatform, WebService::WebService* webService)
{
#if ANKI_PERF_METRIC_ENABLED
  _frameBuffer = new FrameMetricAnim[kNumFramesInBuffer];

  _fileNameSuffix = "Anim";
  
  InitInternal(dataPlatform, webService);
#endif
}


// This is called at the end of the tick
void PerfMetricAnim::Update(const float tickDuration_ms,
                            const float tickFrequency_ms,
                            const float sleepDurationIntended_ms,
                            const float sleepDurationActual_ms)
{
#if ANKI_PERF_METRIC_ENABLED
  ANKI_CPU_PROFILE("PerfMetricAnim::Update");

  ExecuteQueuedCommands();

  if (_isRecording)
  {
    FrameMetricAnim& frame = _frameBuffer[_nextFrameIndex];

    if (_bufferFilled)
    {
      // Move the 'first frame start time' up by the frame's time we're about to overwrite
      _firstFrameTime = IncrementFrameTime(_firstFrameTime, frame._tickTotal_ms);
    }

    frame._tickExecution_ms     = tickDuration_ms;
    frame._tickTotal_ms         = tickFrequency_ms;
    frame._tickSleepIntended_ms = sleepDurationIntended_ms;
    frame._tickSleepActual_ms   = sleepDurationActual_ms;

    frame._messageCountAnimToRobot  = AnimProcessMessages::GetMessageCountAtR();
    frame._messageCountAnimToEngine = AnimProcessMessages::GetMessageCountAtE();
    frame._messageCountRobotToAnim  = AnimProcessMessages::GetMessageCountRtA();
    frame._messageCountEngineToAnim = AnimProcessMessages::GetMessageCountEtA();
    frame._relativeStreamTime_ms    = _animationStreamer->GetRelativeStreamTime_ms();
    frame._numLayersRendered        = _animationStreamer->GetNumLayersRendered();

    if (++_nextFrameIndex >= kNumFramesInBuffer)
    {
      _nextFrameIndex = 0;
      _bufferFilled = true;
    }
  }

  UpdateWaitMode();
#endif
}


void PerfMetricAnim::InitDumpAccumulators()
{
  _accMessageCountRtA.Clear();
  _accMessageCountAtR.Clear();
  _accMessageCountEtA.Clear();
  _accMessageCountAtE.Clear();
}


const PerfMetric::FrameMetric& PerfMetricAnim::UpdateDumpAccumulators(const int frameBufferIndex)
{
  const FrameMetricAnim& frame = _frameBuffer[frameBufferIndex];
  _accMessageCountRtA += frame._messageCountRobotToAnim;
  _accMessageCountAtR += frame._messageCountAnimToRobot;
  _accMessageCountEtA += frame._messageCountEngineToAnim;
  _accMessageCountAtE += frame._messageCountAnimToEngine;
  _accRelativeStreamTime_ms += frame._relativeStreamTime_ms;
  _accNumLayersRendered     += frame._numLayersRendered;

  return _frameBuffer[frameBufferIndex];  // Return the base class data
}


int PerfMetricAnim::AppendFrameData(const DumpType dumpType,
                                    const int frameBufferIndex,
                                    const int dumpBufferOffset,
                                    const bool graphableDataOnly)
{
  const FrameMetricAnim& frame = _frameBuffer[frameBufferIndex];
#define ANIM_LINE_DATA_VARS \
  frame._messageCountRobotToAnim, frame._messageCountAnimToRobot,\
  frame._messageCountEngineToAnim, frame._messageCountAnimToEngine,\
  frame._relativeStreamTime_ms, frame._numLayersRendered

  static const char* kFormatLine = "    %5i %5i %5i %5i %5i %5i\n";
  static const char* kFormatLineCSV = ",%i,%i,%i,%i,%i,%i\n";

  const int lenOut = snprintf(&_dumpBuffer[dumpBufferOffset], kSizeDumpBuffer - dumpBufferOffset,
                              dumpType == DT_FILE_CSV ? kFormatLineCSV : kFormatLine,
                              ANIM_LINE_DATA_VARS);
  return lenOut;
}


int PerfMetricAnim::AppendSummaryData(const DumpType dumpType,
                                      const int dumpBufferOffset,
                                      const int lineIndex)
{
  DEV_ASSERT_MSG(lineIndex < kNumLinesInSummary, "PerfMetricAnim.AppendSummaryData",
                 "lineIndex %d out of range", lineIndex);

#define ANIM_SUMMARY_LINE_VARS(StatCall)\
  _accMessageCountRtA.StatCall(), _accMessageCountAtR.StatCall(),\
  _accMessageCountEtA.StatCall(), _accMessageCountAtE.StatCall(),\
  _accRelativeStreamTime_ms.StatCall(), _accNumLayersRendered.StatCall()

  static const char* kFormatLine = "    %5.1f %5.1f %5.1f %5.1f %5.0f %5.0f\n";
  static const char* kFormatLineCSV = ",%.1f,%.1f,%.1f,%.1f,%.0f,%.0f\n";

#define APPEND_SUMMARY_LINE(StatCall)\
  lenOut = snprintf(&_dumpBuffer[dumpBufferOffset],    - dumpBufferOffset,\
                    dumpType == DT_FILE_CSV ? kFormatLineCSV : kFormatLine,\
                    ANIM_SUMMARY_LINE_VARS(StatCall));

  int lenOut = 0;
  switch (lineIndex)
  {
    case 0:   APPEND_SUMMARY_LINE(GetMin);   break;
    case 1:   APPEND_SUMMARY_LINE(GetMax);   break;
    case 2:   APPEND_SUMMARY_LINE(GetMean);  break;
    case 3:   APPEND_SUMMARY_LINE(GetStd);   break;
  }
  return lenOut;
}


} // namespace Vector
} // namespace Anki
