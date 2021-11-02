/**
 * File: quadTree.h
 *
 * Author: Raul
 * Date:   12/09/2015
 *
 * Description: Mesh representation of known geometry and obstacles for/from navigation with quad trees.
 *
 * Copyright: Anki, Inc. 2015
 **/

#ifndef ANKI_COZMO_QUAD_TREE_H
#define ANKI_COZMO_QUAD_TREE_H

#include "quadTreeNode.h"

namespace Anki {

class Pose3d;

namespace Vector {

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
class QuadTree : public QuadTreeNode
{
public:

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Initialization
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  
  // constructor/destructor
  QuadTree(
    std::function<void (const QuadTreeNode*)> destructorCallback,
    std::function<void (const QuadTreeNode*, const NodeContent&)> modifiedCallback
  );
  
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Accessors
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  // returns the precision of content data in the memory map. For example, if you add a point, and later query for it,
  // the region that the point generated to store the point could have an error of up to this length.
  float GetContentPrecisionMM() const;

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Operations
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  // notify the QT that the given region has the specified content. If a NodeTransformFunction is specified instead of 
  // data, that node will subdivide as necessary and then apply the transform to the default leaf data
  bool Insert(const FoldableRegion& region, NodeTransformFunction transform);
  
  // modify content bounded by region. Note that if the region extends outside the current size of the root node,
  // it will not expand the root node
  bool Transform(const FoldableRegion& region, NodeTransformFunction transform);
  bool Transform(const NodeAddress& address, NodeTransformFunction transform);
  
  // merge the given quadtree into this quad tree, applying to the quads from other the given transform
  bool Merge(const QuadTree& other, const Pose3d& transform);


private:

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Node operations
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  // Expand the root node so that the given quad/point/triangle is included in the navMesh, up to the max root size limit.
  // shiftAllowedCount: number of shifts we can do if the root reaches the max size upon expanding (or already is at max.)
  bool ExpandToFit(const AxisAlignedQuad& region);  

  // quadTrees are always the highest level node, so we cannot change it's parent. If needed, a Merge can insert
  // a quadtree into an existing tree
  void ChangeParent(const QuadTreeNode* newParent) = delete;

  // moves this node's center towards the required points, so that they can be included in this node
  // returns true if the root shifts, false if it can't shift to accomodate all points or the points are already contained
  bool ShiftRoot(const AxisAlignedQuad& region);

  // Convert this node into a parent of its level, delegating its children to the new child that substitutes it
  // In order for a quadtree to be valid, the only way this could work without further operations is calling this
  // on a root node. Such responsibility lies in the caller, not in this node
  // Returns true if successfully expanded, false otherwise
  // maxRootLevel: it won't upgrade if the root is already higher level than the specified
  bool UpgradeRootLevel(const Point2f& direction, uint8_t maxRootLevel);

}; // class
  
} // namespace
} // namespace

#endif //
