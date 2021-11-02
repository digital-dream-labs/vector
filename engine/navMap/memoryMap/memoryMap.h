/**
 * File: memoryMap.h
 *
 * Author: Raul
 * Date:   12/09/2015
 *
 * Description: QuadTree map of the space navigated by the robot with some memory features (like decay = forget).
 *
 * Copyright: Anki, Inc. 2015
 **/

#ifndef ANKI_COZMO_MEMORY_MAP_H
#define ANKI_COZMO_MEMORY_MAP_H

#include "engine/navMap/iNavMap.h"
#include "engine/navMap/quadTree/quadTree.h"
#include "engine/navMap/quadTree/quadTreeProcessor.h"

#include <shared_mutex>

namespace Anki {
namespace Vector {
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
class MemoryMap : public INavMap
{
friend class CST_NavMap;    // allow access for webots test for NavMap

public:

  using EContentType           = MemoryMapTypes::EContentType;
  using FullContentArray       = MemoryMapTypes::FullContentArray;
  using MemoryMapRegion        = MemoryMapTypes::MemoryMapRegion;
  using MemoryMapDataConstList = MemoryMapTypes::MemoryMapDataConstList;

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Construction/Destruction
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  MemoryMap();
  virtual ~MemoryMap();

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // From INavMemoryMap
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  
  
  // add data to the memory map defined by poly
  virtual bool Insert(const MemoryMapRegion& r, const MemoryMapData& data) override;
  virtual bool Insert(const MemoryMapRegion& r, NodeTransformFunction transform) override;
  
  // merge the given map into this map by applying to the other's information the given transform
  // although this methods allows merging any INavMemoryMap into any INavMemoryMap, subclasses are not
  // expected to provide support for merging other subclasses, but only other instances from the same
  // subclass
  virtual bool Merge(const INavMap& other, const Pose3d& transform) override;
  
  // fills inner regions satisfying innerPred( inner node ) && outerPred(neighboring node), converting
  // the inner region to the given data
  bool FillBorder(const NodePredicate& innerPred, const NodePredicate& outerPred, const MemoryMapDataPtr& data) override;
  
  // attempt to apply a transformation function to any node intersecting the region
  virtual bool TransformContent(NodeTransformFunction transform, const MemoryMapRegion& region) override;
  
  // populate a list of all data that matches the predicate inside poly
  virtual void FindContentIf(const NodePredicate& pred, MemoryMapDataConstList& output, const MemoryMapRegion& region) const override;
  
  // return the size of the area currently explored
  virtual double GetExploredRegionAreaM2() const override;
    
  // evaluates f along any node that the region collides with. returns true if any call to NodePredicate returns true
  virtual bool AnyOf(const MemoryMapRegion& p, const NodePredicate& f) const override;

  // multi-ray variant of the AnyOf method
  // implementation may optimize for this case
  virtual std::vector<bool> AnyOf( const Point2f& start, const std::vector<Point2f>& ends, const NodePredicate& pred) const override;

  // returns the accumulated area of cells that satisfy the predicate (and region, if supplied)
  virtual float GetArea(const NodePredicate& f, const MemoryMapRegion& r) const override;

  // Broadcast the memory map
  virtual void GetBroadcastInfo(MemoryMapTypes::MapBroadcastData& info) const override;

private:
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Attributes
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  
  // processor for this quadtree
  QuadTreeProcessor _processor;

  // underlaying data container
  QuadTree          _quadTree;

  // safe thread access for planner
  mutable std::shared_timed_mutex _writeAccess;
  
}; // class
  
} // namespace
} // namespace

#endif // ANKI_COZMO_MEMORY_MAP_H
