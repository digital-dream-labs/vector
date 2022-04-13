/**
 * File: quadTreeProcessor.cpp
 *
 * Author: Raul
 * Date:   01/13/2016
 *
 * Description: Class for processing a quadTree. It relies on the quadTree and quadTreeNodes to
 * share the proper information for the Processor.
 *
 * Copyright: Anki, Inc. 2016
 **/
#include "quadTreeProcessor.h"
#include "quadTree.h"

#include "coretech/common/engine/math/bresenhamLine2d.h"

#include "util/console/consoleInterface.h"
#include "util/logging/logging.h"

#define LOG_CHANNEL "quadTreeProcessor"

namespace Anki {
namespace Vector {

CONSOLE_VAR(bool , kRenderSeeds        , "QuadTreeProcessor", false); // renders seeds differently for debugging purposes
CONSOLE_VAR(bool , kRenderBordersFrom  , "QuadTreeProcessor", false); // renders detected borders (origin quad)
CONSOLE_VAR(bool , kRenderBordersToDot , "QuadTreeProcessor", false); // renders detected borders (border center) as dots
CONSOLE_VAR(bool , kRenderBordersToQuad, "QuadTreeProcessor", false); // renders detected borders (destination quad)
CONSOLE_VAR(bool , kRenderBorder3DLines, "QuadTreeProcessor", false); // renders borders returned as 3D lines (instead of quads)
CONSOLE_VAR(float, kRenderZOffset      , "QuadTreeProcessor", 20.0f); // adds Z offset to all quads
CONSOLE_VAR(bool , kDebugFindBorders   , "QuadTreeProcessor", false); // prints debug information in console

#define DEBUG_FIND_BORDER(format, ...)                                                                          \
if ( kDebugFindBorders ) {                                                                                      \
  do{::Anki::Util::sChanneledInfoF(LOG_CHANNEL, "NMQTProcessor", {}, format, ##__VA_ARGS__);}while(0); \
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
QuadTreeProcessor::QuadTreeProcessor()
: _quadTree(nullptr)
, _totalExploredArea_m2(0.0)
, _totalInterestingEdgeArea_m2(0.0)
{

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void QuadTreeProcessor::OnNodeContentChanged(const QuadTreeNode* node, const NodeContent& oldContent)
{
  
  using namespace MemoryMapTypes;
  const EContentType oldType = static_cast<const MemoryMapDataPtr&>(oldContent)->type;
  const EContentType newType = static_cast<const MemoryMapDataPtr&>(node->GetData())->type;

  // type hasn't changed, so we don't need to update any of our caching
  if (oldType == newType) { return; }

  // update exploration area based on the content type
  {
    const bool wasEmpty = (oldType == EContentType::Unknown);
    const MemoryMapDataPtr data = node->GetData();

    const bool needsToRemove = !wasEmpty &&  (node->IsSubdivided() || (newType == EContentType::Unknown));
    const bool needsToAdd    =  wasEmpty && !(node->IsSubdivided() || (newType == EContentType::Unknown));
    if ( needsToRemove )
    {
      const float side_m = MM_TO_M(node->GetSideLen());
      const float area_m2 = side_m*side_m;
      _totalExploredArea_m2 -= area_m2;
    }
    else if ( needsToAdd )
    {
      const float side_m = MM_TO_M(node->GetSideLen());
      const float area_m2 = side_m*side_m;
      _totalExploredArea_m2 += area_m2;
    }
  }

  // update interesting edge
  {
    const bool shouldBeCountedOld = (oldType == EContentType::InterestingEdge);
    const bool shouldBeCountedNew = (newType == EContentType::InterestingEdge);
    const bool needsToRemove =  shouldBeCountedOld && !shouldBeCountedNew;
    const bool needsToAdd    = !shouldBeCountedOld &&  shouldBeCountedNew;
    if ( needsToRemove )
    {
      const float side_m = MM_TO_M(node->GetSideLen());
      const float area_m2 = side_m*side_m;
      _totalInterestingEdgeArea_m2 -= area_m2;
    }
    else if ( needsToAdd )
    {
      const float side_m = MM_TO_M(node->GetSideLen());
      const float area_m2 = side_m*side_m;
      _totalInterestingEdgeArea_m2 += area_m2;
    }
  }

  // if old content type is cached
  if ( IsCached(oldType) )
  {
    // remove the node from that cache
    DEV_ASSERT(_nodeSets[oldType].find(node) != _nodeSets[oldType].end(),
               "QuadTreeProcessor.OnNodeContentTypeChanged.InvalidRemove");
    _nodeSets[oldType].erase( node );
  }

  if ( IsCached(newType) )
  {
    // add node to that cache
    DEV_ASSERT(_nodeSets[newType].find(node) == _nodeSets[newType].end(),
               "QuadTreeProcessor.OnNodeContentTypeChanged.InvalidInsert");
    _nodeSets[newType].insert(node);
  }
}
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void QuadTreeProcessor::OnNodeDestroyed(const QuadTreeNode* node)
{
  // if old content type is cached
  const EContentType oldContent = static_cast<const MemoryMapDataPtr&>(node->GetData())->type;
  if ( IsCached(oldContent) )
  {
    // remove the node from that cache
    DEV_ASSERT(_nodeSets[oldContent].find(node) != _nodeSets[oldContent].end(),
               "QuadTreeProcessor.OnNodeDestroyed.InvalidNode");
    _nodeSets[oldContent].erase(node);
  }

  // remove the area for this node if it was counted before
  {
    const bool wasOutOld = (oldContent == EContentType::Unknown);
    const bool needsToRemove = !wasOutOld;
    if ( needsToRemove )
    {
      const float side_m = MM_TO_M(node->GetSideLen());
      const float area_m2 = side_m*side_m;
      _totalExploredArea_m2 -= area_m2;
    }
  }
  
  // remove interesting edge area if it was counted before
  {
    const bool shouldBeCountedOld = (oldContent == EContentType::InterestingEdge);
    const bool needsToRemove =  shouldBeCountedOld;
    if ( needsToRemove )
    {
      const float side_m = MM_TO_M(node->GetSideLen());
      const float area_m2 = side_m*side_m;
      _totalInterestingEdgeArea_m2 -= area_m2;
    }
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::vector<bool>
QuadTreeProcessor::AnyOfRays( const Point2f& start, 
                              const std::vector<Point2f>& ends, 
                              const NodePredicate& pred) const
{
  std::vector<bool> results(ends.size(), false);
  std::unordered_map<Point2i ,bool> localCache;

  // start by rasterizing the line described by [start,end]
  const auto startBres = QuadTreeTypes::GetIntegralCoordinateOfNode(start, 
    _quadTree->GetCenter(), 
    _quadTree->GetContentPrecisionMM(), 
    _quadTree->GetMaxHeight());
  const auto maxTreeHeight = _quadTree->GetMaxHeight();

  for(int rayIdx = 0; rayIdx<ends.size(); ++rayIdx) {
    BresenhamLinePixelIterator bresIter(startBres, 
      QuadTreeTypes::GetIntegralCoordinateOfNode(ends[rayIdx], 
        _quadTree->GetCenter(), 
        _quadTree->GetContentPrecisionMM(), 
        _quadTree->GetMaxHeight()));
    while(!bresIter.Done()) {
      const Point2i& rasterPoint = bresIter.Get();
      auto got = localCache.find(rasterPoint);
      if(got == localCache.end()) {
        // compute new result and insert into the cache
        const QuadTreeNode* qnode = nullptr;
        auto getNode = [&qnode] (const QuadTreeNode& n) { qnode = &n; };
        ((const QuadTree*)_quadTree)->Fold(getNode, GetAddressForNodeCenter(rasterPoint, maxTreeHeight));
        const bool result = qnode && pred( static_cast<const MemoryMapDataPtr&>(qnode->GetData()) );
        got = localCache.insert({rasterPoint, result}).first;
      }
      if(got->second) {
        results[rayIdx] = true;
        break; // skip computing the rest of the ray
      }
      bresIter.Next();
    }
  }
  return results;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
QuadTreeProcessor::NodeSet QuadTreeProcessor::GetNodesToFill(const NodePredicate& innerPred, const NodePredicate& outerPred)
{
  NodeSet output;
  NodeSet unexpandedNodes;

  // find any node of typeToFill that satisfies pred(node, neighbor)
  for (const auto& keyValuePair : _nodeSets ) {
    for (const auto& node : keyValuePair.second ) {
      // first check if node is typeToFill
      if ( innerPred( static_cast<const MemoryMapDataPtr&>(node->GetData()) ) ) {
        // check if this nodes has a neighbor of any typesToFillFrom
        for(const auto& neighbor : node->GetNeighbors()) {
          if( outerPred( static_cast<const MemoryMapDataPtr&>(neighbor->GetData()) ) ) {
            unexpandedNodes.emplace( node );
            break;
          }
        }
      }
    }
  }

  // expand all nodes for fill
  while(!unexpandedNodes.empty()) {
    // get the next node and add it to the output list
    auto front = unexpandedNodes.begin();
    const QuadTreeNode* node = *front;
    output.insert(node);
    unexpandedNodes.erase(front);

    // get all of this nodes neighbors of the same type
    for(const auto& neighbor : node->GetNeighbors()) {
      if ( innerPred( static_cast<const MemoryMapDataPtr&>(neighbor->GetData()) ) && (output.find(neighbor) == output.end()) ) {
        unexpandedNodes.emplace( neighbor );
      }
    }
  } // all nodes expanded
  
  return output;
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool QuadTreeProcessor::FillBorder(const NodePredicate& innerPred, const NodePredicate& outerPred, const MemoryMapDataPtr& data)
{
  bool changed = false;
  for( const auto& node : GetNodesToFill(innerPred, outerPred) ) {
    changed |= _quadTree->Transform( node->GetAddress(), [&data] (auto) { return data; } );
  }

  return changed;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool QuadTreeProcessor::HasContentType(EContentType nodeType) const
{
  DEV_ASSERT_MSG(IsCached(nodeType), "QuadTreeProcessor.HasContentType", "%s is not cached",
                 EContentTypeToString(nodeType));
  
  // check if any nodes for that type are cached currently
  auto nodeSetMatch = _nodeSets.find(nodeType);
  const bool hasAny = (nodeSetMatch != _nodeSets.end()) && !nodeSetMatch->second.empty();
  return hasAny;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool QuadTreeProcessor::IsCached(EContentType contentType)
{
  switch( contentType ) {
    case EContentType::ObstacleObservable:
    case EContentType::ObstacleProx:
    case EContentType::ObstacleUnrecognized:
    case EContentType::InterestingEdge:
    case EContentType::NotInterestingEdge:
    case EContentType::Cliff:
    {
      return true;
    }
    case EContentType::Unknown:
    case EContentType::ClearOfObstacle:
    case EContentType::ClearOfCliff:
    case EContentType::_Count:
    {
      return false;
    }
  }
}
} // namespace Vector
} // namespace Anki
