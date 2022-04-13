/**
 * File: quadTreeNode.h
 *
 * Author: Raul
 * Date:   12/09/2015
 *
 * Description: Nodes in the nav mesh, represented as quad tree nodes.
 * Note nodes can work with a processor to speed up algorithms and searches, however this implementation supports
 * working with one processor only for any given node. Do not use more than one processor instance for nodes, or
 * otherwise leaks and bad pointer references will happen.
 *
 * Copyright: Anki, Inc. 2015
 **/

#ifndef ANKI_COZMO_QUAD_TREE_NODE_H
#define ANKI_COZMO_QUAD_TREE_NODE_H

#include "quadTreeTypes.h"

#include "util/helpers/noncopyable.h"

namespace Anki {
namespace Vector {

using namespace QuadTreeTypes;

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
class QuadTreeNode : private Util::noncopyable
{
  friend class QuadTree;
public:
  ~QuadTreeNode();
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Accessors
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  bool                   IsRootNode()     const { return _parent == nullptr; }
  bool                   IsSubdivided()   const { return !_childrenPtr.empty(); }
  uint8_t                GetMaxHeight()   const { return _maxHeight; }
  float                  GetSideLen()     const { return _sideLen; }
  const Point2f&         GetCenter()      const { return _center; }
  const NodeContent&     GetData()        const { return _content; }
  const NodeAddress&     GetAddress()     const { return _address; }
  const AxisAlignedQuad& GetBoundingBox() const { return _boundingBox; }

  // run the provided accumulator function recursively over the tree for all nodes intersecting with region (if provided).
  // NOTE: any recursive call through the QTN should be implemented by fold so all collision checks happen in a consistant manner
  void Fold(const FoldFunctorConst& accumulator, const FoldableRegion& region = RealNumbers2f(), FoldDirection dir = FoldDirection::BreadthFirst) const;
  void Fold(const FoldFunctorConst& accumulator, const NodeAddress& addr, FoldDirection dir = FoldDirection::BreadthFirst) const;
  
  // finds all the leaf nodes that are neighbors with this node
  std::vector<const QuadTreeNode*> GetNeighbors() const;

protected:

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Initialization
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  
  // Leave the constructor as a protected member so only the root node or other Quad tree nodes can create new nodes
  // it will allow subdivision as long as level is greater than 0
  QuadTreeNode(const QuadTreeNode* parent = nullptr, EQuadrant quadrant = EQuadrant::Root);
    
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Modification
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  
  // updates the address incase tree structure changes (expands and shifts)
  void ResetAddress();
  
  // split the current node
  bool Subdivide();

  // checks if all children are the same type, if so it removes the children and merges back to a single parent
  void TryAutoMerge();
  
  // force sets the type and updates shared container
  void ForceSetContent(NodeContent&& newContent);
  
  // sets a new parent to this node. Used on expansions
  void ChangeParent(const QuadTreeNode* newParent);
  
  // swaps children and content with 'otherNode', updating the children's parent pointer
  void SwapChildrenAndContent(QuadTreeNode* otherNode);
  
  // run the provided accumulator function recursively over the tree for all nodes intersecting with region (if provided).
  // NOTE: mutable recursive calls should remain private to ensure tree invariants are held
  void Fold(FoldFunctor& accumulator, const FoldableRegion& region = RealNumbers2f(), FoldDirection dir = FoldDirection::BreadthFirst);
  void Fold(FoldFunctor& accumulator, const NodeAddress& addr, FoldDirection dir = FoldDirection::BreadthFirst);
  
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Exploration
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  
  // get the child in the given quadrant, or null if this node is not subdivided
  const QuadTreeNode* GetChild(EQuadrant quadrant) const;
  QuadTreeNode* GetChild(EQuadrant quadrant);

  // iterate until we reach the nodes that have a border in the given direction, and add them to the vector
  // NOTE: this method is expected to NOT clear the vector before adding descendants
  void AddSmallestDescendants(EDirection direction, std::vector<const QuadTreeNode*>& descendants) const;
   
  // find the neighbor of the same or higher level in the given direction
  const QuadTreeNode* FindSingleNeighbor(EDirection direction) const;

  
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Attributes
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  
  // NOTE: try to minimize padding in these attributes

  // children when subdivided. Can be empty or have 4 nodes
  std::vector< std::unique_ptr<QuadTreeNode> > _childrenPtr;

  // coordinates of this quad
  Point2f _center;
  float   _sideLen;

  AxisAlignedQuad _boundingBox;

  // parent node
  const QuadTreeNode* _parent;

  // our level
  uint8_t _maxHeight;

  // quadrant within the parent
  EQuadrant _quadrant;
  NodeAddress _address;
  
  // information about what's in this quad
  NodeContent _content;

  // callbacks to notify external system if an element has changed or been destroyed
  std::function<void (const QuadTreeNode*)> _destructorCallback;
  std::function<void (const QuadTreeNode*, const NodeContent&)> _modifiedCallback;
    
}; // class
  

} // namespace
} // namespace

#endif //
