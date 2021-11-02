/**
 * File: quadTreeNode.cpp
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
#include "quadTreeNode.h"
#include "engine/navMap/memoryMap/data/memoryMapData.h"

#include "util/math/math.h"

namespace Anki {
namespace Vector {

static_assert( !std::is_copy_assignable<QuadTreeNode>::value, "QuadTreeNode was designed non-copyable" );
static_assert( !std::is_copy_constructible<QuadTreeNode>::value, "QuadTreeNode was designed non-copyable" );
static_assert( !std::is_move_assignable<QuadTreeNode>::value, "QuadTreeNode was designed non-movable" );
static_assert( !std::is_move_constructible<QuadTreeNode>::value, "QuadTreeNode was designed non-movable" );

namespace {
  // helper type for recursing on all children of this node
  static const RealNumbers2f kNodeRegion = RealNumbers2f();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
QuadTreeNode::QuadTreeNode(const QuadTreeNode* parent, EQuadrant quadrant)
: _boundingBox({0,0}, {0,0})
, _parent(parent)
, _quadrant(quadrant)
{
  if (_parent) { 
    float halfLen = _parent->GetSideLen() * .25f;
    _sideLen      = _parent->GetSideLen() * .5f;
    _center       = _parent->GetCenter() + Quadrant2Vec(_quadrant) * halfLen;
    _maxHeight    = _parent->GetMaxHeight() - 1;
    _boundingBox  = AxisAlignedQuad(_center - Point2f(halfLen), _center + Point2f(halfLen));

    _destructorCallback = _parent->_destructorCallback;
    _modifiedCallback   = _parent->_modifiedCallback;
  }
  
  ResetAddress();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
QuadTreeNode::~QuadTreeNode()
{
  _destructorCallback(this);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void QuadTreeNode::ResetAddress()
{
  _address.clear();
  if(_parent) { 
    _address = NodeAddress(_parent->GetAddress()); 
    _address.push_back(_quadrant);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool QuadTreeNode::Subdivide()
{
  if ( (_maxHeight == 0) || IsSubdivided() ) { return false; }
  
  // create new children, push our data to them, then clear our own data
  _childrenPtr.emplace_back( new QuadTreeNode(this, EQuadrant::PlusXPlusY) );   // up L
  _childrenPtr.emplace_back( new QuadTreeNode(this, EQuadrant::PlusXMinusY) );  // up R
  _childrenPtr.emplace_back( new QuadTreeNode(this, EQuadrant::MinusXPlusY) );  // lo L
  _childrenPtr.emplace_back( new QuadTreeNode(this, EQuadrant::MinusXMinusY) ); // lo R

  for ( auto& childPtr : _childrenPtr ) {
    childPtr->ForceSetContent( NodeContent(_content) );
  }

  ForceSetContent(NodeContent());

  return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void QuadTreeNode::TryAutoMerge()
{
  if (!IsSubdivided()) {
    return;
  }

  // can't merge if any children are subdivided
  for (const auto& child : _childrenPtr) {
    if ( child->IsSubdivided() ) {
      return;
    }
  }
  
  bool allChildrenEqual = true;
  
  // check if all children classified the same content (assumes node content equality is transitive)
  for(size_t i=0; i<_childrenPtr.size()-1; ++i)
  {
    allChildrenEqual &= (_childrenPtr[i]->GetData() == _childrenPtr[i+1]->GetData());
  }
  
  // we can merge and set that type on this parent
  if ( allChildrenEqual )
  {
    // do a copy since merging will destroy children
    auto content = _childrenPtr[0]->GetData();
    ForceSetContent(std::move(content));

    _childrenPtr.clear();    
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void QuadTreeNode::ForceSetContent(NodeContent&& newContent)
{
  std::swap(_content, newContent);
  _modifiedCallback(this, newContent);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void QuadTreeNode::ChangeParent(const QuadTreeNode* newParent) 
{ 
  _parent = newParent; 
  FoldFunctor reset = [] (QuadTreeNode& node) { node.ResetAddress(); };
  Fold(reset);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void QuadTreeNode::SwapChildrenAndContent(QuadTreeNode* otherNode)
{
  // swap children
  std::swap(_childrenPtr, otherNode->_childrenPtr);

  // notify the children of the parent change
  for ( auto& childPtr : _childrenPtr ) {
    childPtr->ChangeParent( this );
  }

  // notify the children of the parent change
  for ( auto& childPtr : otherNode->_childrenPtr ) {
    childPtr->ChangeParent( otherNode );
  }

  // swap contents by use of copy, since changes have to be notified to the processor
  auto myPrevContent = _content;
  ForceSetContent( std::move( otherNode->_content ));
  otherNode->ForceSetContent(std::move(myPrevContent));
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
const QuadTreeNode* QuadTreeNode::GetChild(EQuadrant quadrant) const
{
  const QuadTreeNode* ret =
    ( _childrenPtr.empty() ) ?
    ( nullptr ) :
    ( _childrenPtr[(std::underlying_type<EQuadrant>::type)quadrant].get() );
  return ret;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
QuadTreeNode* QuadTreeNode::GetChild(EQuadrant quadrant)
{
  QuadTreeNode* ret =
    ( _childrenPtr.empty() ) ?
    ( nullptr ) :
    ( _childrenPtr[(std::underlying_type<EQuadrant>::type)quadrant].get() );
  return ret;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void QuadTreeNode::AddSmallestDescendants(EDirection direction, std::vector<const QuadTreeNode*>& descendants) const
{
  if ( !IsSubdivided() ) {
    descendants.emplace_back( this );
  } else {
    for (const auto& child : _childrenPtr) {
      if(!IsSibling(child->_quadrant, direction)) { 
        child->AddSmallestDescendants(direction, descendants); 
      };
    }
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
const QuadTreeNode* QuadTreeNode::FindSingleNeighbor(EDirection direction) const
{
  if (IsRootNode()) { return nullptr; }
  
  EQuadrant destination = GetQuadrantInDirection(_quadrant, direction);

  // if stepping in the current direction keeps us under the same parent node
  if ( IsSibling(_quadrant, direction) ) {
    return _parent->GetChild( destination );
  } 

  const QuadTreeNode* parentNeighbor = _parent->FindSingleNeighbor( direction );
  if (parentNeighbor) {
    const QuadTreeNode* directNeighbor = parentNeighbor->GetChild( destination );
    
    // if directNeighbor exits, then use that, otherwise return our parents neighbor
    return directNeighbor ? directNeighbor : parentNeighbor;
  }
  
  return nullptr;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::vector<const QuadTreeNode*> QuadTreeNode::GetNeighbors() const
{
  std::vector<const QuadTreeNode*> neighbors;
  
  const QuadTreeNode* plusX  = FindSingleNeighbor(EDirection::PlusX);
  const QuadTreeNode* minusX = FindSingleNeighbor(EDirection::MinusX);
  const QuadTreeNode* minusY = FindSingleNeighbor(EDirection::MinusY);
  const QuadTreeNode* plusY  = FindSingleNeighbor(EDirection::PlusY);
 
  if (plusX ) { plusX->AddSmallestDescendants(EDirection::MinusX, neighbors);  }
  if (minusX) { minusX->AddSmallestDescendants(EDirection::PlusX, neighbors);  }
  if (minusY) { minusY->AddSmallestDescendants(EDirection::PlusY, neighbors); }
  if (plusY ) { plusY->AddSmallestDescendants(EDirection::MinusY, neighbors); }

  return neighbors;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Fold Implementations
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void QuadTreeNode::Fold(FoldFunctor& accumulator, const NodeAddress& addr, FoldDirection dir)
{
  if (FoldDirection::BreadthFirst == dir) { accumulator(*this); }

  if (IsSubdivided() && (addr.size() > _address.size())) { 
    GetChild(addr[_address.size()])->Fold(accumulator, addr, dir); 
  }

  if (FoldDirection::DepthFirst == dir) { accumulator(*this); }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void QuadTreeNode::Fold(const FoldFunctorConst& accumulator, const NodeAddress& addr, FoldDirection dir) const
{
  if (FoldDirection::BreadthFirst == dir) { accumulator(*this); }

  if (IsSubdivided() && (addr.size() > _address.size())) { 
    GetChild(addr[_address.size()])->Fold(accumulator, addr, dir); 
  }

  if (FoldDirection::DepthFirst == dir) { accumulator(*this); }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
/*
  For calls that are constrained by some convex region, first we can potentially avoid excess collision checks
  if the current node is fully contained by the Fold Region. In the example below, nodes 1 through 6 need intersection 
  checks, but nodes A through D do not since their parent is fully contained by Fold Region
 
                    +-----------------+------------------+
                    |                 |                  |
                    |                 |                  |
                    |                 |                  |
                    |         1       |        2         |
                    |                 |                  |
                    |    . . . . . . . . .<- Fold        |
                    |    .            |  .   Region      |
                    +----+----#########--+---------------+
                    |    .    # A | B #  .               |
                    |    4    #---+---#  .               |
                    |    .    # D | C #  .               |
                    +----+----#########  .     3         |
                    |    .    |       |  .               |
                    |    6 . .|. .5. .|. .               |
                    |         |       |                  |
                    +---------+-------+------------------+

*/

namespace {
  inline u8 GetChildFilterMask(const Point2f& center, const AxisAlignedQuad& aabb) {
    u8 childFilter = 0b1111; // Bit field represents quadrants (-x, -y), (-x, +y), (+x, -y), (+x, +y)

    if ( aabb.GetMinVertex().x() > center.x() ) { childFilter &= 0b0011; } // only +x (top) nodes
    if ( aabb.GetMaxVertex().x() < center.x() ) { childFilter &= 0b1100; } // only -x (bot) nodes
    if ( aabb.GetMinVertex().y() > center.y() ) { childFilter &= 0b0101; } // only +y (left) nodes
    if ( aabb.GetMaxVertex().y() < center.y() ) { childFilter &= 0b1010; } // only -y (right) nodes

    return childFilter;
  }  
}

void QuadTreeNode::Fold(FoldFunctor& accumulator, const FoldableRegion& region, FoldDirection dir)
{
  // node and region are disjoint
  if ( !region.IntersectsQuad(_boundingBox) ) { return; }
  
  if (FoldDirection::BreadthFirst == dir) { accumulator(*this); } 

  if ( region.ContainsQuad(_boundingBox) ) { 
    for ( const auto& cPtr : _childrenPtr ) {
      cPtr->Fold(accumulator, kNodeRegion, dir); 
    }
  } else {
    u8 childFilter = IsSubdivided() ? GetChildFilterMask(_center, region.GetBoundingBox()) : 0;        
    for ( const auto& cPtr : _childrenPtr ) { 
      if (childFilter & 0x1) { cPtr->Fold(accumulator, region, dir); }
      if ((childFilter >>= 1) == 0) { break; };
    }
  }

  if (FoldDirection::DepthFirst == dir) { accumulator(*this); }
}

void QuadTreeNode::Fold(const FoldFunctorConst& accumulator, const FoldableRegion& region, FoldDirection dir) const
{
  // node and region are disjoint
  if ( !region.IntersectsQuad(_boundingBox) ) { return; }
  
  if (FoldDirection::BreadthFirst == dir) { accumulator(*this); } 

  if ( region.ContainsQuad(_boundingBox) ) { 
    for ( const auto& cPtr : _childrenPtr ) {
      cPtr->Fold(accumulator, kNodeRegion, dir); 
    }
  } else {
    u8 childFilter = IsSubdivided() ? GetChildFilterMask(_center, region.GetBoundingBox()) : 0;        
    for ( const auto& cPtr : _childrenPtr ) { 
      if (childFilter & 0x1) { cPtr->Fold(accumulator, region, dir); }
      if ((childFilter >>= 1) == 0) { break; };
    }
  }

  if (FoldDirection::DepthFirst == dir) { accumulator(*this); }
}

} // namespace Vector
} // namespace Anki
