//
//  block.cpp
//  Products_Cozmo
//
//  Created by Andrew Stein on 8/23/13.
//  Copyright (c) 2013 Anki, Inc. All rights reserved.
//

#include "engine/block.h"

#include "anki/cozmo/shared/cozmoConfig.h"
#include "anki/cozmo/shared/cozmoEngineConfig.h"

#include "coretech/vision/engine/camera.h"
#include "coretech/common/engine/math/linearAlgebra.h"
#include "coretech/common/engine/math/quad.h"

#include <iomanip>

#define LOG_CHANNEL "Block"

namespace Anki {
namespace Vector {

  // === Block predock pose params ===
  // {angle, x, y}
  // angle: angle about z-axis (which runs vertically along marker)
  //     x: distance along marker horizontal
  //     y: distance along marker normal
  const Pose2d kBlockPreDockPoseOffset = {0, 0, DEFAULT_MIN_PREDOCK_POSE_DISTANCE_MM};
  
  // Static helper for looking up block properties by type
  const Block::BlockInfoTableEntry_t& Block::LookupBlockInfo(const ObjectType type)
  {
    static const std::map<ObjectType, Block::BlockInfoTableEntry_t> BlockInfoLUT = {
#     define BLOCK_DEFINITION_MODE BLOCK_LUT_MODE
#     include "engine/BlockDefinitions.h"
    };
    
    // If this assertion fails, somebody is trying to construct an invalid
    // block type
    auto entry = BlockInfoLUT.find(type);
    DEV_ASSERT(entry != BlockInfoLUT.end(), "Block.LookupBlockInfo.InvalidBlockType");
    return entry->second;
  }

  
#pragma mark --- Generic Block Implementation ---
  
  void Block::AddFace(const FaceName whichFace,
                      const Vision::MarkerType &code,
                      const float markerSize_mm,
                      const u8 dockOrientations,
                      const u8 rollOrientations)
  {
    Pose3d facePose;
    
    const float halfWidth  = 0.5f * GetSize().y();  
    const float halfHeight = 0.5f * GetSize().z();  
    const float halfDepth  = 0.5f * GetSize().x();
    
    // SetSize() should have been called already
    DEV_ASSERT(halfDepth > 0.f && halfHeight > 0.f && halfWidth > 0.f, "Block.AddFace.InvalidHalfSize");
    
    // The poses here are based on the Marker's canonical pose being in the
    // X-Z plane
    // NOTE: these poses intentionally have no parent. That is handled by AddMarker below.
    switch(whichFace)
    {
      case FRONT_FACE:
        facePose = Pose3d(-M_PI_2_F, Z_AXIS_3D(), {-halfDepth, 0.f, 0.f});
        break;
        
      case LEFT_FACE:
        facePose = Pose3d(M_PI,    Z_AXIS_3D(), {0.f, halfWidth, 0.f});
        break;
        
      case BACK_FACE:
        facePose = Pose3d(M_PI_2,  Z_AXIS_3D(), {halfDepth, 0.f, 0.f});
        break;
        
      case RIGHT_FACE:
        facePose = Pose3d(0,       Z_AXIS_3D(), {0.f, -halfWidth, 0.f});
        break;
        
      case TOP_FACE:
        // Rotate -90deg around X, then -90 around Z
        facePose = Pose3d(2.09439510f, {-0.57735027f, 0.57735027f, -0.57735027f}, {0.f, 0.f, halfHeight});
        break;
        
      case BOTTOM_FACE:
        // Rotate +90deg around X, then -90 around Z
        facePose = Pose3d(2.09439510f, {0.57735027f, -0.57735027f, -0.57735027f}, {0.f, 0.f, -halfHeight});
        break;
        
      default:
        CORETECH_THROW("Unknown block face.\n");
    }
    
    // Store a pointer to the marker on each face:
    markersByFace_[whichFace] = &AddMarker(code, facePose, markerSize_mm);
    
  } // AddFace()
  
  
  void Block::GeneratePreActionPoses(const PreActionPose::ActionType type,
                                     std::vector<PreActionPose>& preActionPoses) const
  {
    preActionPoses.clear();
    
    const float halfWidth  = 0.5f * GetSize().y();
    const float halfHeight = 0.5f * GetSize().z();
    
    // The four rotation vectors for the pre-action poses created below
    static const std::array<RotationVector3d,4> kPreActionPoseRotations = {{
      {0.f, Y_AXIS_3D()},  {M_PI_2_F, Y_AXIS_3D()},  {M_PI_F, Y_AXIS_3D()},  {-M_PI_2_F, Y_AXIS_3D()}
    }};
  
    for(const auto& face : LookupBlockInfo(_type).faces)
    {
      const auto& marker = GetMarker(face.whichFace);
      
      // Add a pre-dock pose to each face, at fixed distance normal to the face,
      // and one for each orientation of the block
      for (u8 rot = 0; rot < 4; ++rot)
      {
        auto const& Rvec = kPreActionPoseRotations[rot];
        
        switch(type)
        {
          case PreActionPose::ActionType::DOCKING:
          {
            if (face.dockOrientations & (1 << rot))
            {
              Pose3d preDockPose(M_PI_2 + kBlockPreDockPoseOffset.GetAngle().ToFloat(),
                                 Z_AXIS_3D(),
                                 {kBlockPreDockPoseOffset.GetX() , -kBlockPreDockPoseOffset.GetY(), -halfHeight},
                                 marker.GetPose());
              
              preDockPose.RotateBy(Rvec);
              
              preActionPoses.emplace_back(PreActionPose::DOCKING,
                                          &marker,
                                          preDockPose,
                                          DEFAULT_PREDOCK_POSE_LINE_LENGTH_MM);
            }
            break;
          }
          case PreActionPose::ActionType::FLIPPING:
          {
            // Flip preActionPoses are at the corners of the block so need to divided by root2 to get x and y dist
            const float flipPreActionPoseDist = FLIP_PREDOCK_POSE_DISTAMCE_MM / 1.414f;
            
            if (face.dockOrientations & (1 << rot))
            {
              Pose3d preDockPose(M_PI_2 + M_PI_4 + kBlockPreDockPoseOffset.GetAngle().ToFloat(),
                                 Z_AXIS_3D(),
                                 {flipPreActionPoseDist + halfWidth, -flipPreActionPoseDist, -halfHeight},
                                 marker.GetPose());
              
              preDockPose.RotateBy(Rvec);
              
              preActionPoses.emplace_back(PreActionPose::FLIPPING,
                                          &marker,
                                          preDockPose,
                                          0);
            }
            break;
          }
          case PreActionPose::ActionType::PLACE_ON_GROUND:
          {
            const f32 DefaultPrePlaceOnGroundDistance = ORIGIN_TO_LIFT_FRONT_FACE_DIST_MM - DRIVE_CENTER_OFFSET;
            
            // Add a pre-placeOnGround pose to each face, where the robot will be sitting
            // relative to the face when we put down the block -- one for each
            // orientation of the block.
            Pose3d prePlaceOnGroundPose(M_PI_2,
                                        Z_AXIS_3D(),
                                        Point3f{0.f, -DefaultPrePlaceOnGroundDistance, -halfHeight},
                                        marker.GetPose());
            
            prePlaceOnGroundPose.RotateBy(Rvec);
            
            preActionPoses.emplace_back(PreActionPose::PLACE_ON_GROUND,
                                        &marker,
                                        prePlaceOnGroundPose,
                                        0);
            break;
          }
          case PreActionPose::ActionType::PLACE_RELATIVE:
          {
            // Add a pre-placeRelative pose to each face, where the robot should be before
            // it approaches the block in order to place a carried object on top of or in front of it.
            Pose3d prePlaceRelativePose(M_PI_2,
                                        Z_AXIS_3D(),
                                        Point3f{0.f, -PLACE_RELATIVE_MIN_PREDOCK_POSE_DISTANCE_MM, -halfHeight},
                                        marker.GetPose());
            
            prePlaceRelativePose.RotateBy(Rvec);
            
            preActionPoses.emplace_back(PreActionPose::PLACE_RELATIVE,
                                        &marker,
                                        prePlaceRelativePose,
                                        PLACE_RELATIVE_PREDOCK_POSE_LINE_LENGTH_MM);
            break;
          }
          case PreActionPose::ActionType::ROLLING:
          {
            if (face.rollOrientations & (1 << rot))
            {
              Pose3d preDockPose(M_PI_2 + kBlockPreDockPoseOffset.GetAngle().ToFloat(),
                                 Z_AXIS_3D(),
                                 {kBlockPreDockPoseOffset.GetX() , -kBlockPreDockPoseOffset.GetY(), -halfHeight},
                                 marker.GetPose());
              
              preDockPose.RotateBy(Rvec);
              
              preActionPoses.emplace_back(PreActionPose::ROLLING,
                                          &marker,
                                          preDockPose,
                                          DEFAULT_PREDOCK_POSE_LINE_LENGTH_MM);
              
            }
            break;
          }
          case PreActionPose::ActionType::NONE:
          case PreActionPose::ActionType::ENTRY:
          {
            break;
          }
        }
      }
    }
  }
  
  
  Block::Block(const ObjectType& type,
               const ActiveID& activeID,
               const FactoryID& factoryID)
  : ActionableObject(type)
  , _size(LookupBlockInfo(_type).size)
  , _vizHandle(VizManager::INVALID_HANDLE)
  {
    _activeID = activeID;
    _factoryID = factoryID;
    
    DEV_ASSERT(IsBlockType(type, false),
               "Block.InvalidType");
    
    SetColor(LookupBlockInfo(_type).color);
             
    markersByFace_.fill(NULL);
    
    for(const auto& face : LookupBlockInfo(_type).faces) {
      AddFace(face.whichFace, face.code, face.size, face.dockOrientations, face.rollOrientations);
    }
    
    // Every block should at least have a front face defined in the BlockDefinitions file
    DEV_ASSERT(markersByFace_[FRONT_FACE] != NULL, "Block.Constructor.InvalidFrontFace");
    
    // Skip the check for ghost objects, which are a special case (they have 6 unknown markers)
    if(ANKI_DEVELOPER_CODE && (type != ObjectType::Block_LIGHTCUBE_GHOST))
    {
      // For now, assume 6 different markers, so we can avoid rotation ambiguities
      // Verify that here by making sure a set of markers has as many elements
      // as the original list:
      std::list<Vision::KnownMarker> const& markerList = GetMarkers();
      std::set<Vision::Marker::Code> uniqueCodes;
      for(auto & marker : markerList) {
        uniqueCodes.insert(marker.GetCode());
      }
      DEV_ASSERT(uniqueCodes.size() == markerList.size(), "Block.Constructor.InvalidMarkerList");
    }
    
  } // Constructor: Block(type)
  
  
  const std::vector<Point3f>& Block::GetCanonicalCorners() const
  {
    static const std::vector<Point3f> CanonicalCorners = {{
      Point3f(-0.5f, -0.5f,  0.5f),
      Point3f( 0.5f, -0.5f,  0.5f),
      Point3f(-0.5f, -0.5f, -0.5f),
      Point3f( 0.5f, -0.5f, -0.5f),
      Point3f(-0.5f,  0.5f,  0.5f),
      Point3f( 0.5f,  0.5f,  0.5f),
      Point3f(-0.5f,  0.5f, -0.5f),
      Point3f( 0.5f,  0.5f, -0.5f)
    }};
    
    return CanonicalCorners;
  }
  
  // Override of base class method that also scales the canonical corners
  void Block::GetCorners(const Pose3d& atPose, std::vector<Point3f>& corners) const
  {
    // Start with (zero-centered) canonical corners *at unit size*
    corners = GetCanonicalCorners();
    for(auto & corner : corners) {
      // Scale to the right size
      corner *= _size;
      
      // Move to block's current pose
      corner = atPose * corner;
    }
  }
  
  // Override of base class method which scales the canonical corners
  // to the block's size
  Quad2f Block::GetBoundingQuadXY(const Pose3d& atPose, const f32 padding_mm) const
  {
    const std::vector<Point3f>& canonicalCorners = GetCanonicalCorners();
    
    const Pose3d atPoseWrtOrigin = atPose.GetWithRespectToRoot();
    const Rotation3d& R = atPoseWrtOrigin.GetRotation();

    Point3f paddedSize(_size);
    paddedSize += 2.f*padding_mm;
    
    std::vector<Point2f> points;
    points.reserve(canonicalCorners.size());
    for(auto corner : canonicalCorners) {
      // Scale canonical point to correct (padded) size
      corner *= paddedSize;
      
      // Rotate to given pose
      corner = R*corner;
      
      // Project onto XY plane, i.e. just drop the Z coordinate
      points.emplace_back(corner.x(), corner.y());
    }
    
    Quad2f boundingQuad = GetBoundingQuad(points);
    
    // Re-center
    Point2f center(atPoseWrtOrigin.GetTranslation().x(), atPoseWrtOrigin.GetTranslation().y());
    boundingQuad += center;
    
    return boundingQuad;
    
  } // GetBoundingBoxXY()
  
  
  Block::~Block(void)
  {
    //--Block::numBlocks;
    EraseVisualization();
  }

  // prefix operator (++fname)
  Block::FaceName& operator++(Block::FaceName& fname) {
    fname = (fname < Block::NUM_FACES) ? static_cast<Block::FaceName>( static_cast<int>(fname) + 1 ) : Block::NUM_FACES;
    return fname;
  }
  
  // postfix operator (fname++)
  Block::FaceName operator++(Block::FaceName& fname, int) {
    Block::FaceName newFname = fname;
    ++newFname;
    return newFname;
  }

  Vision::KnownMarker const& Block::GetMarker(FaceName onFace) const
  {
    static const Block::FaceName OppositeFaceLUT[Block::NUM_FACES] = {
      Block::BACK_FACE,
      Block::RIGHT_FACE,
      Block::FRONT_FACE,
      Block::LEFT_FACE,
      Block::BOTTOM_FACE,
      Block::TOP_FACE
    };
    
    const Vision::KnownMarker* markerPtr = markersByFace_[onFace];
    
    if(markerPtr == NULL) {
      if(onFace == FRONT_FACE) {
        CORETECH_THROW("A front face marker should be defined for every block.");
      }
      else if( (markerPtr = markersByFace_[OppositeFaceLUT[onFace] /*GetOppositeFace(onFace)*/]) == NULL) {
          return GetMarker(FRONT_FACE);
      }
    }
    
    return *markerPtr;
    
  } // Block::GetMarker()
  
  const Vision::KnownMarker& Block::GetTopMarker(Pose3d& topMarkerPoseWrtOrigin) const
  {
    // Compare each face's normal's dot product with the Z axis and return the
    // one that is most closely aligned.
    // TODO: Better, cheaper algorithm for finding top face?
    //const Vision::KnownMarker* topMarker = _markers.front();
    auto topMarker = _markers.begin();
    f32 maxDotProd = std::numeric_limits<f32>::lowest();
    //for(FaceName whichFace = FIRST_FACE; whichFace < NUM_FACES; ++whichFace) {
    for(auto marker = _markers.begin(); marker != _markers.end(); ++marker) {
      //const Vision::KnownMarker& marker = _markers[whichFace];
      Pose3d poseWrtOrigin = marker->GetPose().GetWithRespectToRoot();
      const f32 currentDotProd = DotProduct(marker->ComputeNormal(poseWrtOrigin), Z_AXIS_3D());
      if(currentDotProd > maxDotProd) {
        //topFace = whichFace;
        topMarker = marker;
        topMarkerPoseWrtOrigin = poseWrtOrigin;
        maxDotProd = currentDotProd;
      }
    }
    
    return *topMarker;
  }
  
  Radians Block::GetTopMarkerOrientation() const
  {
    Pose3d topMarkerPose;
    GetTopMarker(topMarkerPose);
    const Radians angle( topMarkerPose.GetRotation().GetAngleAroundZaxis() );
    return angle;
  }
  
  void Block::Visualize(const ColorRGBA& color) const
  {
    Pose3d vizPose = GetPose().GetWithRespectToRoot();
    _vizHandle = _vizManager->DrawCuboid(GetID().GetValue(), _size, vizPose, color);
  }
  
  void Block::EraseVisualization() const
  {
    // Erase the main object
    if(_vizHandle != VizManager::INVALID_HANDLE) {
      _vizManager->EraseVizObject(_vizHandle);
      _vizHandle = VizManager::INVALID_HANDLE;
    }
    
    // Erase the pre-dock poses
    ActionableObject::EraseVisualization();
  }
  
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  void Block::SetLEDs(const WhichCubeLEDs whichLEDs,
                      const ColorRGBA& onColor,
                      const ColorRGBA& offColor,
                      const u32 onPeriod_ms,
                      const u32 offPeriod_ms,
                      const u32 transitionOnPeriod_ms,
                      const u32 transitionOffPeriod_ms,
                      const s32 offset,
                      const bool turnOffUnspecifiedLEDs)
  {
    static const u8 FIRST_BIT = 0x01;
    u8 shiftedLEDs = static_cast<u8>(whichLEDs);
    for(u8 iLED=0; iLED<NUM_LEDS; ++iLED) {
      // If this LED is specified in whichLEDs (its bit is set), then
      // update
      if(shiftedLEDs & FIRST_BIT) {
        _ledState[iLED].onColor      = onColor;
        _ledState[iLED].offColor     = offColor;
        _ledState[iLED].onPeriod_ms  = onPeriod_ms;
        _ledState[iLED].offPeriod_ms = offPeriod_ms;
        _ledState[iLED].transitionOnPeriod_ms = transitionOnPeriod_ms;
        _ledState[iLED].transitionOffPeriod_ms = transitionOffPeriod_ms;
        _ledState[iLED].offset = offset;
      } else if(turnOffUnspecifiedLEDs) {
        _ledState[iLED].onColor      = ::Anki::NamedColors::BLACK;
        _ledState[iLED].offColor     = ::Anki::NamedColors::BLACK;
        _ledState[iLED].onPeriod_ms  = 1000;
        _ledState[iLED].offPeriod_ms = 1000;
        _ledState[iLED].transitionOnPeriod_ms = 0;
        _ledState[iLED].transitionOffPeriod_ms = 0;
        _ledState[iLED].offset = 0;
      }
      shiftedLEDs = shiftedLEDs >> 1;
    }
  }
  
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  void Block::SetLEDs(const std::array<u32,NUM_LEDS>& onColors,
                      const std::array<u32,NUM_LEDS>& offColors,
                      const std::array<u32,NUM_LEDS>& onPeriods_ms,
                      const std::array<u32,NUM_LEDS>& offPeriods_ms,
                      const std::array<u32,NUM_LEDS>& transitionOnPeriods_ms,
                      const std::array<u32,NUM_LEDS>& transitionOffPeriods_ms,
                      const std::array<s32,NUM_LEDS>& offsets)
  {
    for(u8 iLED=0; iLED<NUM_LEDS; ++iLED) {
      _ledState[iLED].onColor      = onColors[iLED];
      _ledState[iLED].offColor     = offColors[iLED];
      _ledState[iLED].onPeriod_ms  = onPeriods_ms[iLED];
      _ledState[iLED].offPeriod_ms = offPeriods_ms[iLED];
      _ledState[iLED].transitionOnPeriod_ms = transitionOnPeriods_ms[iLED];
      _ledState[iLED].transitionOffPeriod_ms = transitionOffPeriods_ms[iLED];
      _ledState[iLED].offset = offsets[iLED];
    }
  }
  
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  void Block::MakeStateRelativeToXY(const Point2f& xyPosition, MakeRelativeMode mode)
  {
    WhichCubeLEDs referenceLED = WhichCubeLEDs::NONE;
    switch(mode)
    {
      case MakeRelativeMode::RELATIVE_LED_MODE_OFF:
        // Nothing to do
        return;
        
      case MakeRelativeMode::RELATIVE_LED_MODE_BY_CORNER:
        referenceLED = GetCornerClosestToXY(xyPosition);
        break;
        
      case MakeRelativeMode::RELATIVE_LED_MODE_BY_SIDE:
        referenceLED = GetFaceClosestToXY(xyPosition);
        break;
        
      default:
        LOG_ERROR("Block.MakeStateRelativeToXY", "Unrecognized relative LED mode %s.", MakeRelativeModeToString(mode));
        return;
    }
    
    switch(referenceLED)
    {
        //
        // When using upper left corner (of current top face) as reference corner:
        //
        //   OR
        //
        // When using upper side (of current top face) as reference side:
        // (Note this is the current "Left" face of the block.)
        //
        
      case WhichCubeLEDs::FRONT_RIGHT:
      case WhichCubeLEDs::FRONT:
        // Nothing to do
        return;
        
      case WhichCubeLEDs::FRONT_LEFT:
      case WhichCubeLEDs::LEFT:
        // Rotate clockwise one slot
        RotatePatternAroundTopFace(true);
        return;
        
      case WhichCubeLEDs::BACK_RIGHT:
      case WhichCubeLEDs::RIGHT:
        // Rotate counterclockwise one slot
        RotatePatternAroundTopFace(false);
        return;
        
      case WhichCubeLEDs::BACK_LEFT:
      case WhichCubeLEDs::BACK:
        // Rotate two slots (either direction)
        // TODO: Do this in one shot
        RotatePatternAroundTopFace(true);
        RotatePatternAroundTopFace(true);
        return;
        
      default:
        LOG_ERROR("Block.MakeStateRelativeToXY", "Unexpected reference LED %d.", static_cast<int>(referenceLED));
        return;
    }
  } // MakeStateRelativeToXY()
  
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  WhichCubeLEDs Block::MakeWhichLEDsRelativeToXY(const WhichCubeLEDs whichLEDs,
                                                 const Point2f& xyPosition,
                                                 MakeRelativeMode mode) const
  {
    WhichCubeLEDs referenceLED = WhichCubeLEDs::NONE;
    switch(mode)
    {
      case MakeRelativeMode::RELATIVE_LED_MODE_OFF:
        // Nothing to do
        return whichLEDs;
        
      case MakeRelativeMode::RELATIVE_LED_MODE_BY_CORNER:
        referenceLED = GetCornerClosestToXY(xyPosition);
        break;
        
      case MakeRelativeMode::RELATIVE_LED_MODE_BY_SIDE:
        referenceLED = GetFaceClosestToXY(xyPosition);
        break;
        
      default:
        LOG_ERROR("Block.MakeStateRelativeToXY", "Unrecognized relateive LED mode %s.", MakeRelativeModeToString(mode));
        return whichLEDs;
    }
    
    switch(referenceLED)
    {
        //
        // When using upper left corner (of current top face) as reference corner:
        //
        //  OR
        //
        // When using upper side (of current top face) as reference side:
        // (Note this is the current "Left" face of the block.)
        //
        
      case WhichCubeLEDs::FRONT_RIGHT:
      case WhichCubeLEDs::FRONT:
        // Nothing to do
        return whichLEDs;
        
      case WhichCubeLEDs::FRONT_LEFT:
      case WhichCubeLEDs::LEFT:
        // Rotate clockwise one slot
        return RotateWhichLEDsAroundTopFace(whichLEDs, true);
        
      case WhichCubeLEDs::BACK_RIGHT:
      case WhichCubeLEDs::RIGHT:
        // Rotate counterclockwise one slot
        return RotateWhichLEDsAroundTopFace(whichLEDs, false);
        
      case WhichCubeLEDs::BACK_LEFT:
      case WhichCubeLEDs::BACK:
        // Rotate two slots (either direction)
        // TODO: Do this in one shot
        return RotateWhichLEDsAroundTopFace(RotateWhichLEDsAroundTopFace(whichLEDs, true), true);
        
      default:
        LOG_ERROR("Block.MakeStateRelativeToXY", "Unexpected reference LED %d.", static_cast<int>(referenceLED));
        return whichLEDs;
    }
  } // MakeWhichLEDsRelativeToXY()
  
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  WhichCubeLEDs Block::GetCornerClosestToXY(const Point2f& xyPosition) const
  {
    // Get a vector from center of marker in its current pose to given xyPosition
    Pose3d topMarkerPose;
    const Vision::KnownMarker& topMarker = GetTopMarker(topMarkerPose);
    const Vec2f topMarkerCenter(topMarkerPose.GetTranslation());
    Vec2f v(xyPosition);
    v -= topMarkerCenter;
    
    if (topMarker.GetCode() != GetMarker(FaceName::TOP_FACE).GetCode()) {
      LOG_WARNING("Block.GetCornerClosestToXY.IgnoringBecauseBlockOnSide", "");
      return WhichCubeLEDs::FRONT_LEFT;
    }
    
    LOG_INFO("Block.GetCornerClosestToXY",
             "Block %d's TopMarker is = %s, angle = %.3f deg",
             GetID().GetValue(),
             topMarker.GetCodeName(),
             topMarkerPose.GetRotation().GetAngleAroundZaxis().getDegrees());
    
    Radians angle = std::atan2(v.y(), v.x());
    angle -= topMarkerPose.GetRotationAngle<'Z'>();
    //assert(angle >= -M_PI && angle <= M_PI); // No longer needed: Radians class handles this
    
    WhichCubeLEDs whichLEDs = WhichCubeLEDs::NONE;
    if(angle > 0.f) {
      if(angle < M_PI_2) {
        // Between 0 and 90 degrees: Upper Right Corner
        whichLEDs = WhichCubeLEDs::BACK_LEFT;
      } else {
        // Between 90 and 180: Upper Left Corner
        //assert(angle<=M_PI);
        whichLEDs = WhichCubeLEDs::FRONT_LEFT;
      }
    } else {
      //assert(angle >= -M_PI);
      if(angle > -M_PI_2_F) {
        // Between -90 and 0: Lower Right Corner
        whichLEDs = WhichCubeLEDs::BACK_RIGHT;
      } else {
        // Between -90 and -180: Lower Left Corner
        //assert(angle >= -M_PI);
        whichLEDs = WhichCubeLEDs::FRONT_RIGHT;
      }
    }
    
    if (whichLEDs != WhichCubeLEDs::NONE) {
      LOG_INFO("Block.GetCornerClosestToXY",
               "Angle = %.3f deg, Closest corner to (%.2f, %.2f): %s",
               angle.getDegrees(),
               xyPosition.x(),
               xyPosition.y(),
               EnumToString(whichLEDs));
    }
    
    return whichLEDs;
  } // GetCornerClosestToXY()
  
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  WhichCubeLEDs Block::GetFaceClosestToXY(const Point2f& xyPosition) const
  {
    // Get a vector from center of marker in its current pose to given xyPosition
    Pose3d topMarkerPose;
    const Vision::KnownMarker& topMarker = GetTopMarker(topMarkerPose);
    const Vec3f topMarkerCenter(topMarkerPose.GetTranslation());
    
    
    const Vec2f v(xyPosition.x()-topMarkerCenter.x(), xyPosition.y()-topMarkerCenter.y());
    
    if (topMarker.GetCode() != GetMarker(FaceName::TOP_FACE).GetCode()) {
      LOG_WARNING("Block.GetFaceClosestToXY.IgnoringBecauseBlockOnSide", "");
      return WhichCubeLEDs::FRONT;
    }
    
    LOG_INFO("Block.GetFaceClosestToXY",
             "Block %d's TopMarker is = %s, angle = %.3f deg",
             GetID().GetValue(),
             Vision::MarkerTypeStrings[topMarker.GetCode()],
             topMarkerPose.GetRotation().GetAngleAroundZaxis().getDegrees());
    
    Radians angle = std::atan2(v.y(), v.x());
    angle = -(topMarkerPose.GetRotationAngle<'Z'>() - angle);
    
    
    WhichCubeLEDs whichLEDs = WhichCubeLEDs::NONE;
    if(angle < M_PI_4_F && angle >= -M_PI_4_F) {
      // Between -45 and 45 degrees: Back Face
      whichLEDs = WhichCubeLEDs::BACK;
    }
    else if(angle < 3*M_PI_4_F && angle >= M_PI_4_F) {
      // Between 45 and 135 degrees: Left Face
      whichLEDs = WhichCubeLEDs::LEFT;
    }
    else if(angle < -M_PI_4_F && angle >= -3*M_PI_4_F) {
      // Between -45 and -135: Right Face
      whichLEDs = WhichCubeLEDs::RIGHT;
    }
    else {
      // Between -135 && +135: Front Face
      assert(angle < -3*M_PI_4_F || angle > 3*M_PI_4_F);
      whichLEDs = WhichCubeLEDs::FRONT;
    }
    
    if (whichLEDs != WhichCubeLEDs::NONE) {
      LOG_INFO("Block.GetFaceClosestToXY",
               "Angle = %.3f deg, Closest face to (%.2f, %.2f): %s",
               angle.getDegrees(),
               xyPosition.x(),
               xyPosition.y(),
               EnumToString(whichLEDs));
    }
    
    return whichLEDs;
  } // GetFaceClosestToXY()
  
  
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  inline const u8* GetRotationLUT(bool clockwise)
  {
    static const u8 cwRotatedPosition[Block::NUM_LEDS] = {
      3, 0, 1, 2
    };
    static const u8 ccwRotatedPosition[Block::NUM_LEDS] = {
      1, 2, 3, 0
    };
    
    // Choose the appropriate LUT
    const u8* rotatedPosition = (clockwise ? cwRotatedPosition : ccwRotatedPosition);
    
    return rotatedPosition;
  }
  
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  void Block::RotatePatternAroundTopFace(bool clockwise)
  {
    const u8* rotatedPosition = GetRotationLUT(clockwise);
    
    // Create the new state array
    std::array<LEDstate,NUM_LEDS> newState;
    for(u8 iLED=0; iLED<NUM_LEDS; ++iLED) {
      newState[rotatedPosition[iLED]] = _ledState[iLED];
    }
    
    // Swap new state into place
    std::swap(newState, _ledState);
    
  } // RotatePatternAroundTopFace()
  
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  WhichCubeLEDs Block::RotateWhichLEDsAroundTopFace(WhichCubeLEDs whichLEDs, bool clockwise)
  {
    const u8* rotatedPosition = GetRotationLUT(clockwise);
    
    u8 rotatedWhichLEDs = 0;
    u8 currentBit = 1;
    for(u8 iLED=0; iLED<NUM_LEDS; ++iLED) {
      // Set the corresponding rotated bit if the current bit is set
      rotatedWhichLEDs |= ((currentBit & (u8)whichLEDs)>0) << rotatedPosition[iLED];
      currentBit = (u8)(currentBit << 1);
    }
    
    return (WhichCubeLEDs)rotatedWhichLEDs;
  }
  
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  const Block::LEDstate& Block::GetLEDState(s32 whichLED) const
  {
    if(whichLED >= NUM_LEDS) {
      LOG_WARNING("Block.GetLEDState.IndexTooLarge",
                  "Requested LED index is too large (%d > %d). Returning %d.",
                  whichLED, NUM_LEDS-1, NUM_LEDS-1);
      whichLED = NUM_LEDS-1;
    } else if(whichLED < 0) {
      LOG_WARNING("Block.GetLEDState.NegativeIndex",
                  "LED index should be >= 0, not %d. Using 0.", whichLED);
      whichLED = 0;
    }
    return _ledState[whichLED];
  }


} // namespace Vector
} // namespace Anki
