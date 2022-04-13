/**
 * File: quadTreeProcessor.h
 *
 * Author: Raul
 * Date:   01/13/2016
 *
 * Description: Helper class for processing a quadTree. It performs a bunch of caching operations for 
 * quick access to important data, without having to explicitly travel the whole tree. We want to use this
 * class specifically for any operations that want to query all Data directly, but don't have constraints
 * on where it is located in the tree.
 *
 * Copyright: Anki, Inc. 2016
 **/

#ifndef ANKI_COZMO_QUAD_TREE_PROCESSOR_H
#define ANKI_COZMO_QUAD_TREE_PROCESSOR_H

#include "engine/navMap/quadTree/quadTreeTypes.h"
#include "engine/navMap/memoryMap/memoryMapTypes.h"
#include "engine/navMap/memoryMap/data/memoryMapData.h"

#include "util/helpers/templateHelpers.h"
#include "coretech/common/engine/math/fastPolygon2d.h"

#include <unordered_map>
#include <unordered_set>
#include <map>

namespace Anki {
namespace Vector {
  
class QuadTree;
using namespace MemoryMapTypes;
using namespace QuadTreeTypes;

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
class QuadTreeProcessor
{
public:

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Initialization
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  
  // constructor
  QuadTreeProcessor();

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Notifications from nodes
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  
  // set root
  // NOTE: (mrw) used to set type from Invalid -> Valid so that only SetRoot and subdivide could create
  //       a validated node in the tree. Any other type of node instantiation would be flagged so that
  //       we know it was not attached to a tree. We could consider adding that safety mechanism back.
  void SetRoot(QuadTree* tree) { _quadTree = tree; };

  // notification when the content type changes for the given node
  void OnNodeContentChanged(const QuadTreeNode* node, const NodeContent& oldContent);

  // notification when a node is going to be removed entirely
  void OnNodeDestroyed(const QuadTreeNode* node);
  
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Processing
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  // return the size of the area currently explored
  inline double GetExploredRegionAreaM2() const { return _totalExploredArea_m2; }
  
  // fills inner regions satisfying innerPred( inner node ) && outerPred(neighboring node), converting
  // the inner region to the given data
  bool FillBorder(const NodePredicate& innerPred, const NodePredicate& outerPred, const MemoryMapDataPtr& data);
  
  // returns true if there are any nodes of the given type, false otherwise
  bool HasContentType(EContentType type) const;

  // multi-ray based collision checking optimization with memoization of result point info
  std::vector<bool> AnyOfRays(const Point2f& start, const std::vector<Point2f>& ends, const NodePredicate& pred) const;
 
private:

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Types
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  using NodeSet = std::unordered_set<const QuadTreeNode*>;
  
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Query
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  
  // obtains a set of nodes of type typeToFill that satisfy innerPred(node of type typeToFill) && outerPred(neighboring node)
  NodeSet GetNodesToFill(const NodePredicate& innerPred, const NodePredicate& outerPred);
  
  // true if we have a need to cache the given content type, false otherwise
  static bool IsCached(EContentType contentType);
  
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Attributes
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  
  using NodeSetPerType = std::unordered_map<EContentType, NodeSet, Anki::Util::EnumHasher>;
  
  // cache of nodes/quads classified per type for faster processing
  NodeSetPerType _nodeSets;
  
  // pointer to the root of the tree
  QuadTree* _quadTree;
  
  // area of all quads that have been explored
  double _totalExploredArea_m2;
  
  // area of all quads that are currently interesting edges
  double _totalInterestingEdgeArea_m2;
}; // class
  
} // namespace
} // namespace

#endif //
