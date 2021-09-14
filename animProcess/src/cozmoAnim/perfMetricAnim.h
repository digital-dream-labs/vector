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


#ifndef __Vector_PerfMetric_Anim_H__
#define __Vector_PerfMetric_Anim_H__

#include "util/perfMetric/iPerfMetric.h"
#include "util/stats/statsAccumulator.h"

#include <string>


namespace Anki {
namespace Vector {

namespace Anim {
  class AnimationStreamer;
}

class PerfMetricAnim : public PerfMetric
{
public:

  explicit PerfMetricAnim(const Anim::AnimContext* context);
  virtual ~PerfMetricAnim();

  virtual void Init(Util::Data::DataPlatform* dataPlatform, WebService::WebService* webService) override final;
  virtual void Update(const float tickDuration_ms,
                      const float tickFrequency_ms,
                      const float sleepDurationIntended_ms,
                      const float sleepDurationActual_ms) override final;

  void SetAnimationStreamer(Anim::AnimationStreamer* animationStreamer) { _animationStreamer = animationStreamer; };

private:

  virtual void InitDumpAccumulators() override final;
  virtual const FrameMetric& UpdateDumpAccumulators(const int frameBufferIndex) override final;
  virtual const FrameMetric& GetBaseFrame(const int frameBufferIndex) override final { return _frameBuffer[frameBufferIndex]; };  
  virtual int AppendFrameData(const DumpType dumpType,
                              const int frameBufferIndex,
                              const int dumpBufferOffset,
                              const bool graphableDataOnly) override final;
  virtual int AppendSummaryData(const DumpType dumpType,
                                const int dumpBufferOffset,
                                const int lineIndex) override final;

  // Frame size:  Base struct is 16 bytes; plus this struct is 20 bytes = 36 bytes total
  // x 1000 frames is roughly 35 KB
  struct FrameMetricAnim : public FrameMetric
  {
    uint32_t _messageCountAnimToRobot;
    uint32_t _messageCountAnimToEngine;
    uint32_t _messageCountRobotToAnim;
    uint32_t _messageCountEngineToAnim;
    uint16_t _relativeStreamTime_ms;
    uint16_t _numLayersRendered;
  };

  FrameMetricAnim*              _frameBuffer = nullptr;

  Util::Stats::StatsAccumulator _accMessageCountRtA;
  Util::Stats::StatsAccumulator _accMessageCountAtR;
  Util::Stats::StatsAccumulator _accMessageCountEtA;
  Util::Stats::StatsAccumulator _accMessageCountAtE;
  Util::Stats::StatsAccumulator _accRelativeStreamTime_ms;
  Util::Stats::StatsAccumulator _accNumLayersRendered;

  Anim::AnimationStreamer*            _animationStreamer = nullptr;
};

static const int kNumFramesInBuffer = 2000;

} // namespace Vector
} // namespace Anki


#endif // __Vector_PerfMetric_Anim_H__
