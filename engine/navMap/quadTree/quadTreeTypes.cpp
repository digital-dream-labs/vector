/**
 * File: quadTreeTypes.cpp
 *
 * Author: Raul
 * Date:   01/13/2016
 *
 * Description: Type definitions for QuadTree.
 *
 * Copyright: Anki, Inc. 2016
 **/
#include "quadTreeTypes.h"

#include "coretech/common/engine/exceptions.h"

namespace Anki {
namespace Vector {
namespace QuadTreeTypes {

static_assert(Vec2Quadrant( Vec2f( 1.f,  1.f) ) == EQuadrant::PlusXPlusY,   "Incorrect Quadrant 1");
static_assert(Vec2Quadrant( Vec2f( 1.f, -1.f) ) == EQuadrant::PlusXMinusY,  "Incorrect Quadrant 2");
static_assert(Vec2Quadrant( Vec2f(-1.f,  1.f) ) == EQuadrant::MinusXPlusY,  "Incorrect Quadrant 3");
static_assert(Vec2Quadrant( Vec2f(-1.f, -1.f) ) == EQuadrant::MinusXMinusY, "Incorrect Quadrant 4");

static_assert(Vec2Quadrant( Vec2f( 1.f,  0.f) ) == EQuadrant::PlusXPlusY,   "Incorrect +x 0y Axis quadrant");
static_assert(Vec2Quadrant( Vec2f(-1.f,  0.f) ) == EQuadrant::MinusXMinusY, "Incorrect -x 0y Axis quadrant");
static_assert(Vec2Quadrant( Vec2f( 0.f,  1.f) ) == EQuadrant::MinusXPlusY,  "Incorrect 0x +y Axis quadrant");
static_assert(Vec2Quadrant( Vec2f( 0.f, -1.f) ) == EQuadrant::PlusXMinusY,  "Incorrect 0x -y Axis quadrant");


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Vec2f Quadrant2Vec(EQuadrant dir) 
{
  switch (dir) {
    case EQuadrant::PlusXPlusY:   { return { 1, 1}; };
    case EQuadrant::PlusXMinusY:  { return { 1,-1}; };
    case EQuadrant::MinusXPlusY:  { return {-1, 1}; };
    case EQuadrant::MinusXMinusY: { return {-1,-1}; };
    default:                      { return {0, 0}; };
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
NodeAddress GetAddressForNodeCenter(const Point2i& nodeCenter, const uint8_t& depth)
{
  if(nodeCenter.x() < 0 || nodeCenter.y() < 0) {
    return NodeAddress();
  }
  // (0,0) is the furthest possible leaf node in the MinusXMinusY direction
  // thus the binary mask of the cell is directly used to compute the address
  NodeAddress addr(depth);
  addr[0] = EQuadrant::Root;
  uint32_t dirX = ~nodeCenter.x();
  uint32_t dirY = ~nodeCenter.y();
  uint32_t mask = 1;
  bool minusX, minusY;
  for(int i=depth-1; i>=0; --i) {
    minusX = dirX & mask;
    minusY = dirY & mask;
    addr[i] = (EQuadrant) (((minusX) << 1) + minusY);
    mask = mask << 1;
  }
  return addr;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::string ToString(const NodeAddress& addr)
{
  std::stringstream ss;
  
  const auto quadrantString = [](const EQuadrant& q) -> std::string {
    switch(q) {
      case EQuadrant::PlusXPlusY:   return "++";
      case EQuadrant::PlusXMinusY:  return "+-";
      case EQuadrant::MinusXPlusY:  return "-+";
      case EQuadrant::MinusXMinusY: return "--";
      case EQuadrant::Root:         return "root";
    }
    return "";
  };

  ss << "[";
  for(int i=0; i<addr.size(); ++i) {
    ss << quadrantString(addr[i]);
    ss << (i+1 == addr.size() ? "" : ",");
  }
  ss << "]";
  return ss.str();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
static_assert(GetQuadrantInDirection(EQuadrant::PlusXPlusY,   EDirection::PlusX)  == EQuadrant::MinusXPlusY,  "bad quadrant calculation");
static_assert(GetQuadrantInDirection(EQuadrant::PlusXPlusY,   EDirection::MinusX) == EQuadrant::MinusXPlusY,  "bad quadrant calculation");
static_assert(GetQuadrantInDirection(EQuadrant::PlusXPlusY,   EDirection::PlusY)  == EQuadrant::PlusXMinusY,  "bad quadrant calculation");
static_assert(GetQuadrantInDirection(EQuadrant::PlusXPlusY,   EDirection::MinusY) == EQuadrant::PlusXMinusY,  "bad quadrant calculation");

static_assert(GetQuadrantInDirection(EQuadrant::MinusXPlusY,  EDirection::PlusX)  == EQuadrant::PlusXPlusY,  "bad quadrant calculation");
static_assert(GetQuadrantInDirection(EQuadrant::MinusXPlusY,  EDirection::MinusX) == EQuadrant::PlusXPlusY,  "bad quadrant calculation");
static_assert(GetQuadrantInDirection(EQuadrant::MinusXPlusY,  EDirection::PlusY)  == EQuadrant::MinusXMinusY, "bad quadrant calculation");
static_assert(GetQuadrantInDirection(EQuadrant::MinusXPlusY,  EDirection::MinusY) == EQuadrant::MinusXMinusY, "bad quadrant calculation");

static_assert(GetQuadrantInDirection(EQuadrant::PlusXMinusY,  EDirection::PlusX)  == EQuadrant::MinusXMinusY, "bad quadrant calculation");
static_assert(GetQuadrantInDirection(EQuadrant::PlusXMinusY,  EDirection::MinusX) == EQuadrant::MinusXMinusY, "bad quadrant calculation");
static_assert(GetQuadrantInDirection(EQuadrant::PlusXMinusY,  EDirection::PlusY)  == EQuadrant::PlusXPlusY,   "bad quadrant calculation");
static_assert(GetQuadrantInDirection(EQuadrant::PlusXMinusY,  EDirection::MinusY) == EQuadrant::PlusXPlusY,   "bad quadrant calculation");

static_assert(GetQuadrantInDirection(EQuadrant::MinusXMinusY, EDirection::PlusX)  == EQuadrant::PlusXMinusY,  "bad quadrant calculation");
static_assert(GetQuadrantInDirection(EQuadrant::MinusXMinusY, EDirection::MinusX) == EQuadrant::PlusXMinusY,  "bad quadrant calculation");
static_assert(GetQuadrantInDirection(EQuadrant::MinusXMinusY, EDirection::PlusY)  == EQuadrant::MinusXPlusY,  "bad quadrant calculation");
static_assert(GetQuadrantInDirection(EQuadrant::MinusXMinusY, EDirection::MinusY) == EQuadrant::MinusXPlusY,  "bad quadrant calculation");

static_assert(GetOppositeDirection(EDirection::PlusX)  == EDirection::MinusX, "bad direction calculation");
static_assert(GetOppositeDirection(EDirection::MinusX) == EDirection::PlusX,  "bad direction calculation");
static_assert(GetOppositeDirection(EDirection::PlusY)  == EDirection::MinusY, "bad direction calculation");
static_assert(GetOppositeDirection(EDirection::MinusY) == EDirection::PlusY,  "bad direction calculation");

static_assert(!IsSibling(EQuadrant::PlusXPlusY, EDirection::PlusX),  "bad sibling calculation");
static_assert( IsSibling(EQuadrant::PlusXPlusY, EDirection::MinusX), "bad sibling calculation");
static_assert(!IsSibling(EQuadrant::PlusXPlusY, EDirection::PlusY),  "bad sibling calculation");
static_assert( IsSibling(EQuadrant::PlusXPlusY, EDirection::MinusY), "bad sibling calculation");

static_assert( IsSibling(EQuadrant::MinusXPlusY, EDirection::PlusX),  "bad sibling calculation");
static_assert(!IsSibling(EQuadrant::MinusXPlusY, EDirection::MinusX), "bad sibling calculation");
static_assert(!IsSibling(EQuadrant::MinusXPlusY, EDirection::PlusY),  "bad sibling calculation");
static_assert( IsSibling(EQuadrant::MinusXPlusY, EDirection::MinusY), "bad sibling calculation");

static_assert(!IsSibling(EQuadrant::PlusXMinusY, EDirection::PlusX),  "bad sibling calculation");
static_assert( IsSibling(EQuadrant::PlusXMinusY, EDirection::MinusX), "bad sibling calculation");
static_assert( IsSibling(EQuadrant::PlusXMinusY, EDirection::PlusY),  "bad sibling calculation");
static_assert(!IsSibling(EQuadrant::PlusXMinusY, EDirection::MinusY), "bad sibling calculation");

static_assert( IsSibling(EQuadrant::MinusXMinusY, EDirection::PlusX),  "bad sibling calculation");
static_assert(!IsSibling(EQuadrant::MinusXMinusY, EDirection::MinusX), "bad sibling calculation");
static_assert( IsSibling(EQuadrant::MinusXMinusY, EDirection::PlusY),  "bad sibling calculation");
static_assert(!IsSibling(EQuadrant::MinusXMinusY, EDirection::MinusY), "bad sibling calculation");

} // namespace
} // namespace
} // namespace
