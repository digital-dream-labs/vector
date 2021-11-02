/**
 * File: quadTreeTypes.h
 *
 * Author: Raul
 * Date:   01/13/2016
 *
 * Description: Type definitions for QuadTree.
 *
 * Copyright: Anki, Inc. 2016
 **/

#ifndef ANKI_COZMO_NAV_MESH_QUAD_TREE_TYPES_H
#define ANKI_COZMO_NAV_MESH_QUAD_TREE_TYPES_H

#include "engine/navMap/memoryMap/data/memoryMapDataWrapper.h"
#include "coretech/common/engine/math/pointSetUnion.h"

#include <cstdint>
#include <type_traits>

namespace Anki {
namespace Vector {

class QuadTreeNode;

namespace QuadTreeTypes {

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Types
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


// wrapper for a tuple that allows implicit casting to any of its contained types
template <typename... Ts>
class SmartTuple : public std::tuple<Ts...> {
private:
  // general case - assume false since Types can be empty
  template <typename... Types>
  struct has_type : std::false_type {};

  // Test type is different from Head, so recurse on Tail
  template <typename Test, typename Head, typename... Tail>
  struct has_type<Test, Head, Tail...> : has_type<Test, Tail...> {};

  // Head and Test are the same type, so we have Test type
  template <typename Test, typename... Tail>
  struct has_type<Test, Test, Tail...> : std::true_type {};

  // the empty list of types is Unique
  template <typename... Types>
  struct is_unique_set : std::true_type {};

  // to be unique, Tail must not contain Head, and Tail must be unique
  template <typename Head, typename... Tail>
  struct is_unique_set<Head, Tail...> : std::integral_constant<bool, !has_type<Head, Tail...>::value && is_unique_set<Tail...>::value> {};

public:
  // make sure SmartTuple only contains unique types
  static_assert(is_unique_set<Ts...>::value, "SmartTuple cannot have duplicate types");

  SmartTuple() : std::tuple<Ts...>() {}
  SmartTuple(const SmartTuple<Ts...>& n) : std::tuple<Ts...>(n) {}

  // NOTE: use `enable_if_t` here, otherwise we will try to cast to find `SmartTuple<Ts...>` inside `std::tuple<Ts...>`
  //       and get `tuple:1017:5: error: static_assert failed "type not found in type list"` 
  template <typename U, typename = std::enable_if_t<has_type<U, Ts...>::value>>
  SmartTuple(const U& u) : std::tuple<Ts...>() { std::get<U>(*this) = u; }

  template <typename U, typename = std::enable_if_t<has_type<U, Ts...>::value>>
  operator U() const { return std::get<U>(*this); }
};

// wrapper class for specifying the interface between QT actions and geometry methods
class FoldableRegion {
public:
  FoldableRegion(const BoundedConvexSet2f& set) 
  : _aabb(set.GetAxisAlignedBoundingBox())
  {
    using std::placeholders::_1;

    // NOTE: lambda incurs some ~5% perforamnce overhead from the std::function interface, 
    //       so use std::bind where possible to reduce function deref overhead. lambda implementations
    //       are included in comments for clarity on what the bind method should evaluate to

    Contains       = std::bind( &BoundedConvexSet2f::Contains, &set, _1 );
                // ~ [&set](const Point2f& p) { return set.Contains(p); };
    ContainsQuad   = std::bind( &BoundedConvexSet2f::ContainsAll, &set, std::bind(&AxisAlignedQuad::GetVertices, _1) );
                // ~ [&set](const AxisAlignedQuad& q) { return set.ContainsAll(q.GetVertices()); };
    IntersectsQuad = std::bind( &BoundedConvexSet2f::Intersects, &set, _1 );
                // ~ [&set](const AxisAlignedQuad& q) { return set.Intersects(q); };
  }

  // allow Union types
  template <typename T, typename U>
  FoldableRegion(const PointSetUnion2f<T,U>& set) 
  : _aabb(set.GetAxisAlignedBoundingBox())
  {
    using std::placeholders::_1;

    Contains       = std::bind( &PointSetUnion2f<T,U>::Contains, &set, _1 );
                // ~ [&set](const Point2f& p) { return set.Contains(p); };
    ContainsQuad   = std::bind( &PointSetUnion2f<T,U>::ContainsHyperCube, &set, _1 );
                // ~ [&set](const AxisAlignedQuad& q) { return set.ContainsHyperCube(q); };
    IntersectsQuad = std::bind( &PointSetUnion2f<T,U>::Intersects, &set, _1 );
                // ~ [&set](const AxisAlignedQuad& q) { return set.Intersects(q); };
  }

  std::function<bool(const Point2f&)>         Contains;
  std::function<bool(const AxisAlignedQuad&)> ContainsQuad;
  std::function<bool(const AxisAlignedQuad&)> IntersectsQuad;

  // cache AABB since the set is const at this point
  const AxisAlignedQuad& GetBoundingBox() const { return _aabb; };

private:
  const AxisAlignedQuad _aabb;
};

enum class FoldDirection { DepthFirst, BreadthFirst };

// position with respect to the parent
enum class EQuadrant : uint8_t {
  PlusXPlusY   = 0,
  PlusXMinusY  = 1,
  MinusXPlusY  = 2,
  MinusXMinusY = 3,
  Root         = 4 // needed for the root node, who has no parent
};

// movement direction
enum class EDirection { PlusX, PlusY, MinusX, MinusY };

using MemoryMapDataPtr      = MemoryMapDataWrapper<MemoryMapData>;
using NodeContent           = SmartTuple<MemoryMapDataPtr>;
using NodeTransformFunction = std::function<NodeContent (const NodeContent&)>;
using NodeAddress           = std::vector<EQuadrant>;
using FoldFunctor           = std::function<void (QuadTreeNode& node)>;
using FoldFunctorConst      = std::function<void (const QuadTreeNode& node)>;

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Helper functions
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

// EQuadrant to Vec2f
Vec2f Quadrant2Vec(EQuadrant dir) ;

// Vec2f to EQuadrant
constexpr QuadTreeTypes::EQuadrant Vec2Quadrant(const Vec2f& dir) 
{
  if      ( (dir.x() < 0.f) && (dir.y() < 0.f) ) { return EQuadrant::MinusXMinusY; }
  else if ( (dir.x() > 0.f) && (dir.y() < 0.f) ) { return EQuadrant::PlusXMinusY; }
  else if ( (dir.x() < 0.f) && (dir.y() > 0.f) ) { return EQuadrant::MinusXPlusY; }
  else if ( (dir.x() > 0.f) && (dir.y() > 0.f) ) { return EQuadrant::PlusXPlusY; }

  // NOTE: when we have exact matches for 0.f, discriminate the difference between -0.f and 0.f.
  //       This preserves the property that checking a vector reflected through the origin
  //       results in a quadrant reflected through the origin (this property is not true for
  //       vertical and horizontal lines when using float comparison operations, since 
  //       -0.f == 0.f by definition)  
  else if ( dir.x() == 0.f ) { return (dir.y() >= 0.f) ? EQuadrant::MinusXPlusY : EQuadrant::PlusXMinusY; }
  else if ( dir.y() == 0.f ) { return (dir.x() >= 0.f) ? EQuadrant::PlusXPlusY  : EQuadrant::MinusXMinusY; }

  return EQuadrant::PlusXPlusY;
}

// step from a quadrant in direction
inline constexpr QuadTreeTypes::EQuadrant GetQuadrantInDirection(EQuadrant from, EDirection dir)
{
  // bit position 0 is the Y coordinate, bit position 1 is x, so toggle the appropriate bit with an XOR
  switch (dir) {
    case EDirection::PlusX:
    case EDirection::MinusX: { return (EQuadrant) ((std::underlying_type<EQuadrant>::type) from ^ 2); };
    case EDirection::PlusY:    
    case EDirection::MinusY: { return (EQuadrant) ((std::underlying_type<EQuadrant>::type) from ^ 1); };
  }
}

inline constexpr QuadTreeTypes::EDirection GetOppositeDirection(EDirection dir)
{
  // directions are defined in CW order, so just move two positions CW and grab the last two bits
  const EDirection ret = (EDirection)(((std::underlying_type<EDirection>::type)dir + 2) & 0b11);
  return ret;
}

inline constexpr bool IsSibling(EQuadrant from, EDirection dir)
{
  // bit position 0 is the Y coordinate, bit position 1 is x, so just compare state of these bits
  // for each direction
  switch (dir) {
    case EDirection::PlusX:  { return  ((std::underlying_type<EQuadrant>::type)from & 0b10); }
    case EDirection::MinusX: { return !((std::underlying_type<EQuadrant>::type)from & 0b10); }
    case EDirection::PlusY:  { return  ((std::underlying_type<EQuadrant>::type)from & 0b01); }
    case EDirection::MinusY: { return !((std::underlying_type<EQuadrant>::type)from & 0b01); }
  }
}

// computes the node address relative to the root of a tree whose origin is (0,0)
// assumes maximum reachable depth is desired
NodeAddress GetAddressForNodeCenter(const Point2i& nodeCenter, const uint8_t& depth);

std::string ToString(const NodeAddress& addr);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
inline Point2i GetIntegralCoordinateOfNode(const Point2f& point, const Point2f& center, const f32 precision, const uint8_t height)
{
  // first step:
  // transform the cartesian input coordinates so that the tree origin  
  // is at (-0.5, -0.5) and snap to grid coordinates
  // IMPORTANT: the behavior of std::round causes values to round away from zero
  // as a result, the integral coordinates line up better when we first transform
  // s.t. one of the four nearest nodes to the tree center becomes (0,0)
  const f32 precision2 = (precision/2);
  const int x = std::round( (point.x() - center.x() - precision2) / precision );
  const int y = std::round( (point.y() - center.y() - precision2) / precision );
  // second step:
  // determine the pose of the FPP (furthest plus-plus node from tree origin) in
  // the integral coordinates and transform again to make this be the new (0,0)
  const u32 offset = (1 << (height-1));
  return Point2i(x + offset, y + offset);  
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
inline Point2f GetCartesianCoordinateOfNode(const Point2i& point, const Point2f& center, const f32 precision, const uint8_t height)
{
  // transform the coordinate s.t. (0,0) is now the tree origin
  const f32 offset = (1<<(height-1)) - 0.5f;
  return Point2f((point.x() - offset) * precision + center.x(),
                 (point.y() - offset) * precision + center.y());
}


} // namespace
} // namespace
} // namespace

#endif //
