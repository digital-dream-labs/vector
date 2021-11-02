/**
 * File: memoryMap.cpp
 *
 * Author: Raul
 * Date:   12/09/2015
 *
 * Description: Map of the space navigated by the robot with some memory features (like decay = forget).
 *
 * Copyright: Anki, Inc. 2015
 **/
#include "memoryMap.h"

#include "memoryMapTypes.h"
#include "data/memoryMapData_ProxObstacle.h"
#include "data/memoryMapData_Cliff.h"
#include "engine/robot.h"

#include "coretech/common/engine/math/pose.h"
#include "coretech/common/engine/math/quad_fwd.h"
#include "coretech/common/engine/math/fastPolygon2d.h"

#include "util/console/consoleInterface.h"

#include <chrono>
#include <type_traits>
#include <unordered_map>
#include <mutex>
 

namespace Anki {
namespace Vector {

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Helpers
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

CONSOLE_VAR(bool, kMapPerformanceTestsEnabled, "ProxSensorComponent", false);
CONSOLE_VAR(int,  kMapPerformanceTestsSampleWindow, "ProxSensorComponent", 128);
CONSOLE_VAR(bool, kRenderProxBeliefs, "ProxSensorComponent", false);

namespace
{

#define MONITOR_PERFORMANCE(eval) (kMapPerformanceTestsEnabled) ? PerformanceMonitor([&]() {return eval;}, __FILE__ ":" + std::string(__func__)) : eval

struct PerformanceRecord { double avgTime_us = 0; u32 samples = 0; };
static std::unordered_map<std::string, PerformanceRecord> sPerformanceRecords;

static void UpdatePerformanceRecord(const double& time_us, const std::string& recordName) {
  auto& record = sPerformanceRecords[recordName];
  if (record.samples > kMapPerformanceTestsSampleWindow ) {
    // approximate rolling window average to save memory
    record.avgTime_us += (time_us - record.avgTime_us) / kMapPerformanceTestsSampleWindow;
  } else {
    record.avgTime_us += time_us / kMapPerformanceTestsSampleWindow;
  }
  
  // print info (faster than modulo for powers of 2)
  if ((++record.samples & kMapPerformanceTestsSampleWindow - 1) == 0) {
    DEV_ASSERT((kMapPerformanceTestsSampleWindow & (kMapPerformanceTestsSampleWindow - 1)) == 0,
      "Performance sample window not a power of 2");
    PRINT_NAMED_INFO("PerformanceMonitor", "Average time for '%s' is %f us", recordName.c_str(), record.avgTime_us);
  }
}

// handle non-void methods
template<typename T>
static auto PerformanceMonitor(T f, const std::string& method, 
  typename std::enable_if<!std::is_void<decltype(f())>::value>::type* = nullptr) -> decltype(f()) {

  const auto start = std::chrono::system_clock::now();
  const auto retv = f();
  const auto time_us = (std::chrono::system_clock::now() - start).count();

  UpdatePerformanceRecord(time_us, method);
  return retv;
}

// handle void methods
template<typename T>
static auto PerformanceMonitor(T f, const std::string& method, 
  typename std::enable_if<std::is_void<decltype(f())>::value>::type* = nullptr) -> decltype(f()) {
    
  const auto start = std::chrono::system_clock::now();
  f();
  const auto time_us = (std::chrono::system_clock::now() - start).count();

  UpdatePerformanceRecord(time_us, method);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
ColorRGBA GetNodeVizColor(MemoryMapDataPtr node)
{
  // scale used to help visualize confidence levels for obstacles
  float scale = 1.f;
  if (node->type == EContentType::ObstacleProx && kRenderProxBeliefs) {
    u8 val = MemoryMapData::MemoryMapDataCast<MemoryMapData_ProxObstacle>(node)->GetObstacleConfidence();
    scale = std::fmax(0.f, std::fmin(val/100.f, 1.f));
  }

  // check for special coloring rules per content type
  // e.g. visually observed cliffs versus drop sensor cliffs
  // e.g. confidence visualziation for prox obstacles (as a gradient)
  Anki::ColorRGBA color = Anki::NamedColors::WHITE; // default that any other rule overrides

  using namespace ExternalInterface;
  switch(node->GetExternalContentType())
  {
    case ENodeContentTypeEnum::Unknown                : { color = Anki::NamedColors::DARKGRAY; color.SetAlpha(0.2f); break; }
    case ENodeContentTypeEnum::ClearOfObstacle        : { color = Anki::NamedColors::GREEN;    color.SetAlpha(0.5f); break; }
    case ENodeContentTypeEnum::ClearOfCliff           : { color = Anki::NamedColors::DARKGREEN;color.SetAlpha(0.8f); break; }
    case ENodeContentTypeEnum::ObstacleCube           : { color = Anki::NamedColors::RED;      color.SetAlpha(0.5f); break; }
    case ENodeContentTypeEnum::ObstacleUnrecognized   : { color = Anki::NamedColors::BLACK;    color.SetAlpha(0.5f); break; }
    case ENodeContentTypeEnum::Cliff                  : { 
      const auto& cliffData = MemoryMapData::MemoryMapDataCast<MemoryMapData_Cliff>(node);
      if(!cliffData->isFromCliffSensor && cliffData->isFromVision) {
        color = ColorRGBA(1.f, 0.84f, 0.f, 0.75f);   // gold
      } else if(cliffData->isFromCliffSensor && cliffData->isFromVision) {
        color = ColorRGBA(1.f, 0.41f, 0.70f, 0.75f); // pink
      } else if(cliffData->isFromCliffSensor && !cliffData->isFromVision) {
        color = Anki::NamedColors::BLACK;
      }
      color.SetAlpha(0.8f); 
      break; 
    }
    case ENodeContentTypeEnum::InterestingEdge        : { color = Anki::NamedColors::MAGENTA;  color.SetAlpha(0.5f); break; }
    case ENodeContentTypeEnum::NotInterestingEdge     : { color = Anki::NamedColors::PINK;     color.SetAlpha(0.8f); break; }
    case ENodeContentTypeEnum::ObstacleProx           : { 
      color = (Anki::NamedColors::CYAN * scale) + (Anki::NamedColors::GREEN * (1 - scale));  
      color.SetAlpha(0.5f + .5*scale); 
      break; 
    }
    case ENodeContentTypeEnum::ObstacleProxExplored   : { 
      color = (Anki::NamedColors::BLUE * scale) + (Anki::NamedColors::GREEN * (1 - scale));  
      color.SetAlpha(0.5f + .5*scale); 
      break; 
    }
  }

  return color;
}

}; // namespace

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// MemoryMap
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
MemoryMap::MemoryMap()
: _processor()
, _quadTree(
    std::bind( &QuadTreeProcessor::OnNodeDestroyed, &_processor, std::placeholders::_1 ),
    std::bind( &QuadTreeProcessor::OnNodeContentChanged, &_processor, std::placeholders::_1, std::placeholders::_2 )
)
{
  _processor.SetRoot( &_quadTree );
}

MemoryMap::~MemoryMap()
{
  _processor.SetRoot( nullptr );
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool MemoryMap::Merge(const INavMap& other, const Pose3d& transform)
{
  DEV_ASSERT(dynamic_cast<const MemoryMap*>(&other), "MemoryMap.Merge.UnsupportedClass");
  const MemoryMap& otherMap = static_cast<const MemoryMap&>(other);
  std::unique_lock<std::shared_timed_mutex> lock(_writeAccess);
  return MONITOR_PERFORMANCE( _quadTree.Merge( otherMap._quadTree, transform ) );
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool MemoryMap::FillBorder(const NodePredicate& innerPred, const NodePredicate& outerPred, const MemoryMapDataPtr& newData)
{
  // ask the processor to do it
  std::unique_lock<std::shared_timed_mutex> lock(_writeAccess);
  return MONITOR_PERFORMANCE( _processor.FillBorder(innerPred, outerPred, newData) );
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool MemoryMap::TransformContent(NodeTransformFunction transform, const MemoryMapRegion& region)
{
  std::unique_lock<std::shared_timed_mutex> lock(_writeAccess);
  return MONITOR_PERFORMANCE( _quadTree.Transform(region, transform) );
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
double MemoryMap::GetExploredRegionAreaM2() const
{
  // delegate on processor
  std::shared_lock<std::shared_timed_mutex> lock(_writeAccess);
  const double area = _processor.GetExploredRegionAreaM2();
  return area;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool MemoryMap::AnyOf(const MemoryMapRegion& r, const NodePredicate& f) const
{
  bool retv = false;  
  std::shared_lock<std::shared_timed_mutex> lock(_writeAccess);
  _quadTree.Fold( [&](const auto& node) { retv |= f( static_cast<const MemoryMapDataPtr&>(node.GetData())); }, r);
  return retv;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::vector<bool> MemoryMap::AnyOf( const Point2f& start, const std::vector<Point2f>& ends, const NodePredicate& pred) const
{
  std::shared_lock<std::shared_timed_mutex> lock(_writeAccess);
  return _processor.AnyOfRays(start, ends, pred);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
float MemoryMap::GetArea(const NodePredicate& pred, const MemoryMapRegion& region) const
{
  float retv = 0.f;  
  std::shared_lock<std::shared_timed_mutex> lock(_writeAccess);
  _quadTree.Fold( [&](const auto& node) { 
    if ( pred( static_cast<const MemoryMapDataPtr&>(node.GetData())) ) { 
      retv += Util::Square(node.GetSideLen());} 
    }, region);
  return retv;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool MemoryMap::Insert(const MemoryMapRegion& r, const MemoryMapData& data)
{
  // clone data to make into a shared pointer.
  const auto& dataPtr = data.Clone();
  std::unique_lock<std::shared_timed_mutex> lock(_writeAccess);

  NodeTransformFunction trfm = [&dataPtr] (const MemoryMapDataPtr& currentData) { 
    currentData->SetLastObservedTime(dataPtr->GetLastObservedTime());
    return currentData->CanOverrideSelfWithContent(dataPtr) ? dataPtr : currentData; 
  };
  return MONITOR_PERFORMANCE( _quadTree.Insert(r, trfm) );
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool MemoryMap::Insert(const MemoryMapRegion& r, NodeTransformFunction transform)
{
  // clone data to make into a shared pointer.
  std::unique_lock<std::shared_timed_mutex> lock(_writeAccess);
  return MONITOR_PERFORMANCE( _quadTree.Insert(r, transform) );
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void MemoryMap::GetBroadcastInfo(MemoryMapTypes::MapBroadcastData& info) const 
{ 
  // get data for each node
  QuadTreeTypes::FoldFunctorConst accumulator = 
    [&info, this] (const QuadTreeNode& node) {
      // if the root done, populate header info
      if ( node.IsRootNode() ){
        std::stringstream instanceId;
        instanceId << "QuadTree_" << this;

        info.mapInfo = ExternalInterface::MemoryMapInfo(
          node.GetMaxHeight(),
          node.GetSideLen(),
          node.GetCenter().x(),
          node.GetCenter().y(),
          1.f,
          instanceId.str());
      }

      // leaf node
      if ( !node.IsSubdivided() )
      {
        const MemoryMapDataPtr& nodeData = node.GetData();
        const auto& vizColor = GetNodeVizColor(nodeData).AsRGBA();
        
        info.quadInfo.emplace_back(
          nodeData->GetExternalContentType(), 
          node.GetMaxHeight(), 
          vizColor);
        
        info.quadInfoFull.emplace_back(vizColor,
                                       node.GetCenter().x(),
                                       node.GetCenter().y(),
                                       node.GetSideLen());
      }
    };

  std::shared_lock<std::shared_timed_mutex> lock(_writeAccess);
  _quadTree.Fold(accumulator);
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void MemoryMap::FindContentIf(const NodePredicate& pred, MemoryMapDataConstList& output, const MemoryMapRegion& region) const
{
  QuadTreeTypes::FoldFunctorConst accumulator = [&output, &pred] (const QuadTreeNode& node) {
    const MemoryMapDataPtr& data = node.GetData();
    if( pred(data) ) { 
      output.insert( data );
    }
  };

  std::shared_lock<std::shared_timed_mutex> lock(_writeAccess);
  MONITOR_PERFORMANCE( _quadTree.Fold(accumulator, region) );
}

} // namespace Vector
} // namespace Anki
