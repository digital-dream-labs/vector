/**
 * File: overheadEdge.cpp
 *
 * Author: Andrew Stein
 * Date:   3/1/2016
 *
 * Description: Defines a container for holding edge information found in 
 *              Cozmo's overhead ground-plane image.
 *
 *
 *
 * Copyright: Anki, Inc. 2017
 **/

#include "engine/overheadEdge.h"

namespace Anki {
namespace Vector {
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void OverheadEdgeChainVector::RemoveChainsShorterThan(const u32 minChainLength)
{
  auto iter = _chains.begin();
  while(iter != _chains.end())
  {
    // filter chains that don't have a minimum number of points
    if (iter->points.size() < minChainLength) {
      iter = _chains.erase(iter);
    } else {
      ++iter;
    }
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void OverheadEdgeChainVector::AddEdgePoint(const OverheadEdgePoint& pointInfo, bool isBorder )
{
  static const f32 kMaxDistBetweenEdges_mm = 5.f; // start new chain after this distance seen
  
  // can we add to the current image chain?
  bool addToCurrentChain = false;
  if ( !_chains.empty() )
  {
    OverheadEdgePointChain& currentChain = _chains.back();
    if ( currentChain.points.empty() )
    {
      // current chain does not have points yet, we can add this one as the first one
      addToCurrentChain = true;
    }
    else
    {
      // there are points, does the chain and this point match border vs no_border flag?
      if ( isBorder == currentChain.isBorder )
      {
        // they do, is the new point close enough to the last point in the current chain?
        const f32 distToPrevPoint = ComputeDistanceBetween(pointInfo.position,
                                                           _chains.back().points.back().position);
        if ( distToPrevPoint <= kMaxDistBetweenEdges_mm )
        {
          // it is close, this point should be added to the current chain
          addToCurrentChain = true;
        }
      }
    }
  }
  
  // if we don't want to add the point to the current chain, then we need to start a new chain
  if ( !addToCurrentChain )
  {
    _chains.emplace_back();
    _chains.back().isBorder = isBorder;
  }
  
  // add to current chain (can be the newly created for this border)
  OverheadEdgePointChain& newCurrentChain = _chains.back();
  
  // if we have an empty chain, set isBorder now
  if ( newCurrentChain.points.empty() ) {
    newCurrentChain.isBorder = isBorder;
  } else {
    DEV_ASSERT(newCurrentChain.isBorder == isBorder, "VisionSystem.AddEdgePoint.BadBorderFlag");
  }
  
  // now add this point
  newCurrentChain.points.emplace_back( pointInfo );
}
  
} // namespace Vector
} // namespace Anki
