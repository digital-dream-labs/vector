//
//  block.h
//  Products_Cozmo
//
//  Created by Andrew Stein on 8/23/13.
//  Copyright (c) 2013 Anki, Inc. All rights reserved.
//

#ifndef __Products_Cozmo__block__
#define __Products_Cozmo__block__

#include "clad/types/ledTypes.h"

#include "coretech/common/engine/math/pose.h"
#include "coretech/common/engine/math/quad_fwd.h"
#include "coretech/common/engine/robotTimeStamp.h"
#include "coretech/vision/shared/MarkerCodeDefinitions.h"

#include "engine/actionableObject.h"
#include "engine/viz/vizManager.h"

namespace Anki {
  namespace Vector {
    
    //
    // Block Class
    //
    //   Representation of a physical Block in the world.
    //
    class Block : public ActionableObject 
    {
    public:
      
      Block(const ObjectType& type,
            const ActiveID& activeID = -1,
            const FactoryID& factoryID = "");
      
      virtual ~Block();
      
      virtual Block* CloneType() const override {
        return new Block(this->_type);
      }
      
#include "engine/BlockDefinitions.h"
      
      // Enumerated block types
      using Type = Vector::ObjectType;
      
      // NOTE: if the ordering of these is modified, you must also update
      //       the static OppositeFaceLUT.
      enum FaceName {
        FIRST_FACE  = 0,
        FRONT_FACE  = 0,
        LEFT_FACE   = 1,
        BACK_FACE   = 2,
        RIGHT_FACE  = 3,
        TOP_FACE    = 4,
        BOTTOM_FACE = 5,
        NUM_FACES
      };
      
      enum Corners {
        LEFT_FRONT_TOP =     0,
        RIGHT_FRONT_TOP =    1,
        LEFT_FRONT_BOTTOM =  2,
        RIGHT_FRONT_BOTTOM = 3,
        LEFT_BACK_TOP =      4,
        RIGHT_BACK_TOP =     5,
        LEFT_BACK_BOTTOM =   6,
        RIGHT_BACK_BOTTOM =  7,
        NUM_CORNERS       =  8
      };
      
      enum PreActionOrientation {
        NONE  = 0x0,
        UP    = 0x01,
        LEFT  = 0x02,
        DOWN  = 0x04,
        RIGHT = 0x08,
        ALL   = UP | LEFT | DOWN | RIGHT
      };
      
      static const s32 NUM_LEDS = 4;
      
      static const int kInvalidTapCnt = -1;
      
      // Accessors:
      virtual const Point3f& GetSize() const override { return _size; }
      
      void AddFace(const FaceName whichFace,
                   const Vision::MarkerType& code,
                   const float markerSize_mm,
                   const u8 dockOrientations = PreActionOrientation::ALL,
                   const u8 rollOrientations = PreActionOrientation::ALL);
            
      // Return a reference to the marker on a particular face of the block.
      // Symmetry convention: if no marker was set for the requested face, the
      // one on the opposite face is returned.  If none is defined for the
      // opposite face either, the front marker is returned.  Not having
      // a marker defined for at least the front the block is an error, (which
      // should be caught in the constructor).
      Vision::KnownMarker const& GetMarker(FaceName onFace) const;
      
      const Vision::KnownMarker& GetTopMarker(Pose3d& markerPoseWrtOrigin) const;
      
      // Get the orientation of the top marker around the Z axis. An angle of 0
      // means the top marker is in the canonical orienation, such that the corners
      // are as shown in activeBlockTypes.h
      Radians GetTopMarkerOrientation() const;
      
      // Get the block's corners at a specified pose
      virtual void GetCorners(const Pose3d& atPose, std::vector<Point3f>& corners) const override;
      
      // Projects the box in its current 3D pose (or a given 3D pose) onto the
      // XY plane and returns the corresponding 2D quadrilateral. Pads the
      // quadrilateral (around its center) by the optional padding if desired.
      virtual Quad2f GetBoundingQuadXY(const Pose3d& atPose, const f32 padding_mm = 0.f) const override;
      
      // Visualize using VizManager.
      virtual void Visualize(const ColorRGBA& color) const override;
      virtual void EraseVisualization() const override;
      
      virtual bool IsActive() const override  { return true; }
      
      // NOTE: This prevents us from having multiple active objects in the world at the same time: this means we
      //  match to existing objects based solely on type. If we ever do anything like COZMO-23 to get around that, then
      //  this would need to be changed.
      virtual bool IsUnique() const override { return true; }
      
      // Set the same color and flashing frequency of one or more LEDs on the block
      // If turnOffUnspecifiedLEDs is true, any LEDs that were not indicated by
      // whichLEDs will be turned off. Otherwise, they will be left in their current
      // state.
      // NOTE: Alpha is ignored.
      void SetLEDs(const WhichCubeLEDs whichLEDs,
                   const ColorRGBA& onColor,        const ColorRGBA& offColor,
                   const u32 onPeriod_ms,           const u32 offPeriod_ms,
                   const u32 transitionOnPeriod_ms, const u32 transitionOffPeriod_ms,
                   const s32 offset,
                   const bool turnOffUnspecifiedLEDs);
      
      // Specify individual colors and flash frequencies for all the LEDS of the block
      // The index of the arrays matches the diagram above.
      // NOTE: Alpha is ignored
      void SetLEDs(const std::array<u32,NUM_LEDS>& onColors,
                   const std::array<u32,NUM_LEDS>& offColors,
                   const std::array<u32,NUM_LEDS>& onPeriods_ms,
                   const std::array<u32,NUM_LEDS>& offPeriods_ms,
                   const std::array<u32,NUM_LEDS>& transitionOnPeriods_ms,
                   const std::array<u32,NUM_LEDS>& transitionOffPeriods_ms,
                   const std::array<s32,NUM_LEDS>& offsets);
      
      // Make whatever state has been set on the block relative to a given (x,y)
      //  location.
      // When byUpperLeftCorner=true, "relative" means that the pattern is rotated
      //  so that whatever is currently specified for LED 0 is applied to the LED
      //  currently closest to the given position
      // When byUpperLeftCorner=false, "relative" means that the pattern is rotated
      //  so that whatever is specified for the side with LEDs 0 and 4 is applied
      //  to the face currently closest to the given position
      void MakeStateRelativeToXY(const Point2f& xyPosition, MakeRelativeMode mode);
      
      // Similar to above, but returns rotated WhichCubeLEDs rather than changing
      // the block's current state.
      WhichCubeLEDs MakeWhichLEDsRelativeToXY(const WhichCubeLEDs whichLEDs,
                                              const Point2f& xyPosition,
                                              MakeRelativeMode mode) const;
      
      // Get the LED specification for the top (and bottom) LEDs on the corner closest
      // to the specified (x,y) position, using the ActiveCube's current pose.
      WhichCubeLEDs GetCornerClosestToXY(const Point2f& xyPosition) const;
      
      // Get the LED specification for the four LEDs on the face closest
      // to the specified (x,y) position, using the ActiveCube's current pose.
      WhichCubeLEDs GetFaceClosestToXY(const Point2f& xyPosition) const;
      
      // Rotate the currently specified pattern of colors/flashing once slot in
      // the specified direction (assuming you are looking down at the top face)
      void RotatePatternAroundTopFace(bool clockwise);
      
      // Helper for figuring out which LEDs will be selected after rotating
      // a given pattern of LEDs one slot in the specified direction
      static WhichCubeLEDs RotateWhichLEDsAroundTopFace(WhichCubeLEDs whichLEDs, bool clockwise);
      
      
      // If object is moving, returns true and the time that it started moving in t.
      // If not moving, returns false and the time that it stopped moving in t.
      
      virtual bool IsMoving(RobotTimeStamp_t* t = nullptr) const override { if (t) *t=_movingTime; return _isMoving; }
      
      // Set the moving state of the object and when it either started or stopped moving.
      virtual void SetIsMoving(bool isMoving, RobotTimeStamp_t t) override { _isMoving = isMoving; _movingTime = t;}
      
      struct LEDstate {
        ColorRGBA onColor;
        ColorRGBA offColor;
        u32       onPeriod_ms;
        u32       offPeriod_ms;
        u32       transitionOnPeriod_ms;
        u32       transitionOffPeriod_ms;
        s32       offset;
        
        LEDstate()
        : onColor(0), offColor(0), onPeriod_ms(0), offPeriod_ms(0)
        , transitionOnPeriod_ms(0), transitionOffPeriod_ms(0)
        , offset(0)
        {
          
        }
      };
      const LEDstate& GetLEDState(s32 whichLED) const;
      
      // Current tapCount, which is just an incrementing counter in the raw cube messaging
      int GetTapCount() const { return _tapCount; }
      void SetTapCount(const int cnt) { _tapCount = cnt; }
      
    protected:
      
      virtual void GeneratePreActionPoses(const PreActionPose::ActionType type,
                                          std::vector<PreActionPose>& preActionPoses) const override;
      
      // Make this protected so we have to use public AddFace() method
      using ActionableObject::AddMarker;
      
      // LUT of the marker on each face, NULL if none specified.

      std::array<const Vision::KnownMarker*, NUM_FACES> markersByFace_;
      
      // Static const lookup table for all block specs, by block ID, auto-
      // generated from the BlockDefinitions.h file using macros
      typedef struct {
        FaceName             whichFace;
        Vision::MarkerType   code;
        f32                  size;
        u8                   dockOrientations; // See PreActionOrientation
        u8                   rollOrientations; // See PreActionOrientation
      } BlockFaceDef_t;
      
      typedef struct {
        std::string          name;
        ColorRGBA            color;
        Point3f              size;
        bool                 isActive;
        std::vector<BlockFaceDef_t> faces;
      } BlockInfoTableEntry_t;
      
      virtual const std::vector<Point3f>& GetCanonicalCorners() const override;
      
      static const BlockInfoTableEntry_t& LookupBlockInfo(const ObjectType type);
      
      constexpr static const f32 PreDockDistance = 100.f;

      Point3f     _size;
      
      mutable VizManager::Handle_t _vizHandle;
      
      bool        _isMoving = false;
      RobotTimeStamp_t _movingTime = 0;
      
      // Keep track of flash rate and color of each LED
      std::array<LEDstate,NUM_LEDS> _ledState;
      
      // Keep track of the current tapCount, which is just an
      // incrementing counter in the raw cube messaging
      int _tapCount = kInvalidTapCnt;
      
    }; // class Block
    
    
    // prefix operator (++fname)
    Block::FaceName& operator++(Block::FaceName& fname);
    
    // postfix operator (fname++)
    Block::FaceName operator++(Block::FaceName& fname, int);
    

  } // namespace Vector
} // namespace Anki

#endif // __Products_Cozmo__block__
