/**
 * File: iNavMap.h
 *
 * Author: Raul
 * Date:   12/09/2015
 *
 * Description: Public interface for a map of the space navigated by the robot with some memory 
 * features (like decay = forget).
 *
 * Copyright: Anki, Inc. 2015
 **/

#ifndef ANKI_COZMO_INAV_MAP_H
#define ANKI_COZMO_INAV_MAP_H

#include "memoryMap/memoryMapTypes.h"
#include "memoryMap/data/memoryMapData.h"
#include "coretech/common/engine/math/pose.h"

namespace Anki {
namespace Vector {
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Class INavMap
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
class INavMap
{
friend class MapComponent;

public:
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Types		
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -		
		
  // import types from MemoryMapTypes		
  using EContentType          = MemoryMapTypes::EContentType;
  using FullContentArray      = MemoryMapTypes::FullContentArray;
  using NodeTransformFunction = MemoryMapTypes::NodeTransformFunction;
  using NodePredicate         = MemoryMapTypes::NodePredicate;
  using MemoryMapDataPtr      = MemoryMapTypes::MemoryMapDataPtr;
  using MemoryMapRegion       = MemoryMapTypes::MemoryMapRegion;
  
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Construction/Destruction
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  virtual ~INavMap() {}
  
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Query
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  
  // return the size of the area currently explored
  virtual double GetExploredRegionAreaM2() const = 0;
  
  // returns the accumulated area of cells that satisfy the predicate (and region, if supplied)
  virtual float GetArea(const NodePredicate& func, const MemoryMapRegion& region = RealNumbers2f()) const = 0;
  
  // returns true if any node that intersects with the provided regions evaluates `func` as true.
  virtual bool AnyOf(const MemoryMapRegion& region, const NodePredicate& func) const = 0;
  
  // multi-ray variant of the `AnyOf` method implementation may optimize for this case
  virtual std::vector<bool> AnyOf( const Point2f& start, const std::vector<Point2f>& ends, const NodePredicate& pred) const = 0;
  
  // Pack map data to broadcast
  virtual void GetBroadcastInfo(MemoryMapTypes::MapBroadcastData& info) const = 0;

  // populate a list of all data that matches the predicate inside region
  virtual void FindContentIf(const NodePredicate& pred, MemoryMapTypes::MemoryMapDataConstList& output, const MemoryMapRegion& region = RealNumbers2f()) const = 0;
  
protected:
  
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Modification
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  
  // NOTE: Leave modifying calls as protected methods, and access them via the friend classes (at the moment only
  //       MapComponent). The classes manage publication of map data, and need to monitor if the map state has changed

  // add an object with the specified content. 
  virtual bool Insert(const MemoryMapRegion& r, const MemoryMapData& data) = 0;
  virtual bool Insert(const MemoryMapRegion& r, NodeTransformFunction transform) = 0;
  
  // merge the given map into this map by applying to the other's information the given transform
  // although this methods allows merging any INavMap into any INavMap, subclasses are not
  // expected to provide support for merging other subclasses, but only other instances from the same
  // subclass
  virtual bool Merge(const INavMap& other, const Pose3d& transform) = 0;
 
  
  // attempt to apply a transformation function to all nodes in the tree constrained by region
  virtual bool TransformContent(NodeTransformFunction transform, const MemoryMapRegion& region = RealNumbers2f()) = 0;

  // TODO: FillBorder should be local (need to specify a max quad that can perform the operation, otherwise the
  // bounds keeps growing as Cozmo explores). Profiling+Performance required.
  
  // fills inner regions satisfying innerPred( inner node ) && outerPred(neighboring node), converting
  // the inner region to the given data
  // see VIC-2475: this should be modified to work like the TransformContent method does
  virtual bool FillBorder(const NodePredicate& innerPred, const NodePredicate& outerPred, const MemoryMapDataPtr& data) = 0;

}; // class

} // namespace
} // namespace

#endif // 
