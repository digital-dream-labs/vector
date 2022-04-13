/**
 * File: quadTree.cpp
 *
 * Author: Raul
 * Date:   12/09/2015
 *
 * Description: Mesh representation of known geometry and obstacles for/from navigation with quad trees.
 *
 * Copyright: Anki, Inc. 2015
 **/

#include "quadTree.h"
#include "quadTreeProcessor.h"

#include "engine/navMap/memoryMap/data/memoryMapData.h"

#include "coretech/common/engine/math/pose.h"
#include "coretech/common/engine/math/fastPolygon2d.h"

#include "util/logging/logging.h"
#include "util/math/math.h"

#include <sstream>

namespace Anki {
namespace Vector {
  
class Robot;

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
namespace {
constexpr float kQuadTreeInitialRootSideLength = 128.0f;
constexpr uint8_t kQuadTreeInitialMaxDepth = 4;
constexpr uint8_t kQuadTreeMaxRootDepth = 8;
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
QuadTree::QuadTree(
  std::function<void (const QuadTreeNode*)> destructorCallback,
  std::function<void (const QuadTreeNode*, const NodeContent&)> modifiedCallback
)
{
  _sideLen            = kQuadTreeInitialRootSideLength;
  _maxHeight          = kQuadTreeInitialMaxDepth;
  _quadrant           = EQuadrant::Root;
  _address            = {};
  _boundingBox        = AxisAlignedQuad(_center - Point2f(_sideLen*.5f), _center + Point2f(_sideLen*.5));

  _destructorCallback = destructorCallback;
  _modifiedCallback   = modifiedCallback;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
float QuadTree::GetContentPrecisionMM() const
{
  // return the length of the smallest quad allowed
  const float minSide_mm = kQuadTreeInitialRootSideLength / (1 << kQuadTreeInitialMaxDepth); // 1 << x = pow(2,x)
  return minSide_mm;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool QuadTree::Insert(const FoldableRegion& region, NodeTransformFunction transform)
{
  // if the root does not contain the region, expand
  const AxisAlignedQuad aabb = region.GetBoundingBox();
  if ( !_boundingBox.Contains( aabb ) )
  {
    ExpandToFit( aabb );
  }
  
  // run the insert on the expanded QT
  bool contentChanged = false;
  FoldFunctor accumulator = [&] (QuadTreeNode& node)
  {
    auto newData = transform(node.GetData());
    auto currentData = static_cast<const decltype(newData)&>(node.GetData());
    if ( currentData != newData ) {
      // split node since we are unsure if the incoming region will fill the entire area
      node.Subdivide();
      
      // if we are at the max depth
      if ( !node.IsSubdivided() ) {
        node.ForceSetContent( std::move(newData) );
        contentChanged = true;
      } 
    }
  };
  Fold(accumulator, region);

  // try to cleanup tree
  if (contentChanged) {
    FoldFunctor merge = [] (QuadTreeNode& node) { node.TryAutoMerge(); };
    Fold(merge, region, FoldDirection::DepthFirst);
  }

  return contentChanged;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool QuadTree::Transform(const FoldableRegion& region, NodeTransformFunction transform)
{
  // run the transform
  bool contentChanged = false;
  FoldFunctor trfm = [&] (QuadTreeNode& node)
    {
      auto newData = transform(node.GetData());
      if ((node.GetData() != newData) && !node.IsSubdivided()) 
      {
        node.ForceSetContent( std::move(newData) );
        contentChanged = true;
      }
    };

  Fold(trfm, region);

  // try to cleanup tree
  if (contentChanged) {
    FoldFunctor merge = [] (QuadTreeNode& node) { node.TryAutoMerge(); };
    Fold(merge, region, FoldDirection::DepthFirst);
  }

  return contentChanged;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool QuadTree::Transform(const NodeAddress& address, NodeTransformFunction transform)
{
  // run the transform
  bool contentChanged = false;
  FoldFunctor trfm = [&] (QuadTreeNode& node)
    {
      auto newData = transform(node.GetData());
      if ((node.GetData() != newData) && !node.IsSubdivided()) 
      {
        node.ForceSetContent( std::move(newData) );
        contentChanged = true;
      }
    };

  Fold(trfm, address);

  // try to cleanup tree
  if (contentChanged) {
    FoldFunctor merge = [] (QuadTreeNode& node) { node.TryAutoMerge(); };
    Fold(merge, address);
  }
  
  return contentChanged;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool QuadTree::Merge(const QuadTree& other, const Pose3d& transform)
{
  // TODO rsam for the future, when we merge with transform, poses or directions stored as extra info are invalid
  // since they were wrt a previous origin!
  Pose2d transform2d(transform);

  // obtain all leaf nodes from the map we are merging from
  std::vector<const QuadTreeNode*> leafNodes;
  const FoldFunctorConst getLeaves = [&leafNodes](const auto& node) { 
    if (!node.IsSubdivided()) { leafNodes.push_back(&node); } 
  };
  other.Fold( getLeaves );
  
  // note regarding quad size limit: when we merge one map into another, this map can expand or shift the root
  // to accomodate the information that we are receiving from 'other'. 'other' is considered to have more up to
  // date information than 'this', so it should be ok to let it destroy as much info as it needs by shifting the root
  // towards them. In an ideal world, it would probably come to a compromise to include as much information as possible.
  // This I expect to happen naturally, since it's likely that 'other' won't be fully expanded in the opposite direction.
  // It can however happen in Cozmo during explorer mode, and it's debatable which information is more relevant.
  // A simple idea would be to limit leafNodes that we add back to 'this' by some distance, for example, half the max
  // root length. That would allow 'this' to keep at least half a root worth of information with respect the new one
  // we are bringing in.
  
  // iterate all those leaf nodes, adding them to this tree
  bool changed = false;
  for( const auto& nodeInOther : leafNodes ) {
      // NOTE: there's a precision problem when we add back the quads; when we add a non-axis aligned quad to the map,
      // we modify (if applicable) all quads that intersect with that non-aa quad. When we merge this information into
      // a different map, we have lost precision on how big the original non-aa quad was, since we have stored it
      // with the resolution of the memory map quad size. In general, when merging information from the past, we should
      // not rely on precision, but there ar things that we could do to mitigate this issue, for example:
      // a) reducing the size of the aaQuad being merged by half the size of the leaf nodes
      // or
      // b) scaling down aaQuad to account for this error
      // eg: transformedQuad2d.Scale(0.9f);
      // At this moment is just a known issue

      std::vector<Point2f> corners = nodeInOther->GetBoundingBox().GetVertices();
      for (Point2f& p : corners) {
        p = transform2d.GetTransform() * p;
      }
      
      // grab CH to sort verticies into CW order
      const ConvexPolygon poly = ConvexPolygon::ConvexHull( std::move(corners) );
      changed |= Insert(FastPolygon(poly), [&nodeInOther] (auto) { return nodeInOther->GetData(); });
  }
  return changed;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool QuadTree::ExpandToFit(const AxisAlignedQuad& region)
{  
  // allow expanding several times until the poly fits in the tree, as long as we can expand, we keep trying,
  // relying on the root to tell us if we reached a limit
  bool expanded = false;
  bool fitsInMap = false;
  
  do
  {
    // find in which direction we are expanding, upgrade root level in that direction (center moves)
    const Vec2f& direction = region.GetCentroid() - Point2f{GetCenter().x(), GetCenter().y()};
    expanded = UpgradeRootLevel(direction, kQuadTreeMaxRootDepth);

    // check if the region now fits in the expanded root
    fitsInMap = _boundingBox.ContainsAll( {region.GetMinVertex(), region.GetMaxVertex()} );
    
  } while( !fitsInMap && expanded );

  // if the poly still doesn't fit, see if we can shift once
  if ( !fitsInMap )
  {
    // shift the root to try to cover the poly, by removing opposite nodes in the map
    ShiftRoot(region);

    // check if the poly now fits in the expanded root
    fitsInMap = _boundingBox.ContainsAll( {region.GetMinVertex(), region.GetMaxVertex()} );
  }
  
  // the poly should be contained, if it's not, we have reached the limit of expansions and shifts, and the poly does not
  // fit, which will cause information loss
  if ( !fitsInMap ) {
    PRINT_NAMED_WARNING("QuadTree.Expand.InsufficientExpansion",
      "Quad caused expansion, but expansion was not enough PolyCenter(%.2f, %.2f), Root(%.2f,%.2f) with sideLen(%.2f).",
      region.GetCentroid().x(), region.GetCentroid().y(),
      GetCenter().x(), GetCenter().y(),
      GetSideLen() );
  }
  
  // always flag as dirty since we have modified the root (potentially)
  return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool QuadTree::ShiftRoot(const AxisAlignedQuad& region)
{
  const float rootHalfLen = _sideLen * 0.5f;

  const bool xPlusAxisReq  = FLT_GE(region.GetMaxVertex().x(), _center.x()+rootHalfLen);
  const bool xMinusAxisReq = FLT_LE(region.GetMinVertex().x(), _center.x()-rootHalfLen);
  const bool yPlusAxisReq  = FLT_GE(region.GetMaxVertex().y(), _center.y()+rootHalfLen);
  const bool yMinusAxisReq = FLT_LE(region.GetMinVertex().y(), _center.y()-rootHalfLen);
  
  // can't shift +x and -x at the same time
  if ( xPlusAxisReq && xMinusAxisReq ) {
    PRINT_NAMED_WARNING("QuadTreeNode.ShiftRoot.CantShiftPMx", "Current root size can't accomodate given points");
    return false;
  }

  // can't shift +y and -y at the same time
  if ( yPlusAxisReq && yMinusAxisReq ) {
    PRINT_NAMED_WARNING("QuadTreeNode.ShiftRoot.CantShiftPMy", "Current root size can't accomodate given points");
    return false;
  }

  // cache which axes we shift in
  const bool xShift = xPlusAxisReq || xMinusAxisReq;
  const bool yShift = yPlusAxisReq || yMinusAxisReq;
  if ( !xShift && !yShift ) {
    // this means all points are contained in this node, we shouldn't be here
    PRINT_NAMED_ERROR("QuadTreeNode.ShiftRoot.AllPointsIn", "We don't need to shift");
    return false;
  }

  // the new center will be shifted in one or both axes, depending on xyIncrease
  // for example, if we left the root through the right, only the right side will expand, and the left will collapse,
  // but top and bottom will remain the same
  _center.x() = _center.x() + (xShift ? (xPlusAxisReq ? rootHalfLen : -rootHalfLen) : 0.0f);
  _center.y() = _center.y() + (yShift ? (yPlusAxisReq ? rootHalfLen : -rootHalfLen) : 0.0f);
  _boundingBox = AxisAlignedQuad(_center - Point2f(_sideLen * .5f), _center + Point2f(_sideLen * .5f) );
  
  // if the root has children, update them, otherwise no further changes are necessary
  if ( IsSubdivided() )
  {
    // typedef to cast quadrant enum to the underlaying type (that can be assigned to size_t)
    using Q2N = std::underlying_type<EQuadrant>::type; // Q2N stands for "Quadrant To Number", it makes code below easier to read
    static_assert( sizeof(Q2N) < sizeof(size_t), "UnderlyingTypeIsBiggerThanSizeType");
  
    if ( xShift )
    {
      std::vector< std::unique_ptr<QuadTreeNode> > oldChildren;
      std::swap(oldChildren, _childrenPtr);
      Subdivide();

      // make two pairs, (a1->a2) and (b1->b2) that will be swapped depending on the direction of the shift.
      const size_t a1 = (Q2N) ( xPlusAxisReq ? EQuadrant::MinusXPlusY  : EQuadrant::PlusXPlusY);
      const size_t a2 = (Q2N) (!xPlusAxisReq ? EQuadrant::MinusXPlusY  : EQuadrant::PlusXPlusY);
      const size_t b1 = (Q2N) ( xPlusAxisReq ? EQuadrant::MinusXMinusY : EQuadrant::PlusXMinusY);
      const size_t b2 = (Q2N) (!xPlusAxisReq ? EQuadrant::MinusXMinusY : EQuadrant::PlusXMinusY);

      _childrenPtr[a1]->SwapChildrenAndContent(oldChildren[a2].get() );
      _childrenPtr[b1]->SwapChildrenAndContent(oldChildren[b2].get() );
    }

    if ( yShift )
    {
      std::vector< std::unique_ptr<QuadTreeNode> > oldChildren;
      std::swap(oldChildren, _childrenPtr);
      Subdivide();
      
      // make two pairs, (a1->a2) and (b1->b2) that will be swapped depending on the direction of the shift.
      const size_t a1 = (Q2N) ( yPlusAxisReq ? EQuadrant::PlusXMinusY  : EQuadrant::PlusXPlusY);
      const size_t a2 = (Q2N) (!yPlusAxisReq ? EQuadrant::PlusXMinusY  : EQuadrant::PlusXPlusY);
      const size_t b1 = (Q2N) ( yPlusAxisReq ? EQuadrant::MinusXMinusY : EQuadrant::MinusXPlusY);
      const size_t b2 = (Q2N) (!yPlusAxisReq ? EQuadrant::MinusXMinusY : EQuadrant::MinusXPlusY);

      // delete everything in oldChildren since we put the nodes we are keeping back into their new position
      _childrenPtr[a1]->SwapChildrenAndContent(oldChildren[a2].get() );
      _childrenPtr[b1]->SwapChildrenAndContent(oldChildren[b2].get() );
    }
  }

  // log
  PRINT_CH_INFO("QuadTree", "QuadTree.ShiftRoot", "Root level is still %u, root shifted. Allowing %.2fm", _maxHeight, MM_TO_M(_sideLen));
  
  // successful shift
  return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool QuadTree::UpgradeRootLevel(const Point2f& direction, uint8_t maxRootLevel)
{ 
  // reached expansion limit
  if ( _maxHeight == std::numeric_limits<uint8_t>::max() || _maxHeight >= maxRootLevel) {
    return false;
  }

/*
    A = old center
    B = new center (in direction we want to grow)
 
          + - - - - - - - + - - - - - - - +
          -               -               -                   +x
          -               -               -                   ↑
          -               -               -                   |    direction     
          -               -               -                   |   ↗
          -               -               -                   | ⟋
          +-------+-------B - - - - - - - +         +y ←------+
          |       |       |               -
          |       |       |               -
          +-------A-------+               -
          |       |       |               -
          |       |       |               -
          +-------+-------+ - - - - - - - +
*/

  // reset this nodes parameters
  _center += Quadrant2Vec( Vec2Quadrant(direction) ) * _sideLen * 0.5f;
  _boundingBox = AxisAlignedQuad(_center - Point2f(_sideLen), _center + Point2f(_sideLen) );
  _sideLen *= 2.0f;
  ++_maxHeight;
  
  // temporary take its children, then subdivide this node again
  std::vector< std::unique_ptr<QuadTreeNode> > oldChildren;
  std::swap(oldChildren, _childrenPtr);
  Subdivide();

  // calculate the child that takes my place by using the opposite direction to expansion
  QuadTreeNode* childTakingMyPlace = GetChild( Vec2Quadrant(-direction) );
  
  // set the new parent in my old children
  for ( auto& childPtr : oldChildren ) {
    childPtr->ChangeParent( childTakingMyPlace );
  }
  
  // swap children with the temp
  std::swap(childTakingMyPlace->_childrenPtr, oldChildren);

  // set the content type I had in the child that takes my place, then reset my content
  childTakingMyPlace->ForceSetContent( NodeContent(_content) );
  ForceSetContent(MemoryMapDataPtr());

  // log
  PRINT_CH_INFO("QuadTree", "QuadTree.UpdgradeRootLevel", "Root expanded to level %u. Allowing %.2fm", _maxHeight, MM_TO_M(_sideLen));
  
  return true;
}


} // namespace Vector
} // namespace Anki
