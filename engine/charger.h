/**
 * File: charger.h
 *
 * Author: Kevin Yoon
 * Date:   6/5/2015
 *
 * Description: Defines a Charger object, which is a type of ActionableObject
 *
 *
 * Copyright: Anki, Inc. 2015
 **/

#ifndef ANKI_COZMO_BASESTATION_CHARGER_H
#define ANKI_COZMO_BASESTATION_CHARGER_H

#include "coretech/common/engine/math/pose.h"
#include "coretech/common/engine/math/quad_fwd.h"
#include "coretech/common/engine/robotTimeStamp.h"

#include "coretech/vision/engine/observableObject.h"

#include "coretech/vision/shared/MarkerCodeDefinitions.h"

#include "engine/actionableObject.h"
#include "engine/viz/vizManager.h"

namespace Anki {
  
  namespace Util {
    class RandomGenerator;
  }
  
  namespace Vector {
  
    class Robot;
    
    // Note that a ramp's origin (o) is the bottom right vertex of this diagram:
    //
    //   +------------+
    //   |              .
    //   |                .
    //   |     o            .
    //   |                    .
    //   |                      .
    //   *------------------------+
    //   <= Platform =><= Slope ==>
    //
    
    class Charger : public ActionableObject
    {
    public:
      
      Charger();
      
      virtual const Point3f& GetSize() const override { return _size; }
      
      const Vision::KnownMarker* GetMarker() const { return _marker; }
      
      // Return pose of the robot when it's in the charger
      Pose3d GetRobotDockedPose()  const;
      
      // Return the pose of the robot when it has just rolled off the
      // charger. It is roughly the first point at which the robot's
      // bounding quad no longer intersects the charger's.
      Pose3d GetRobotPostRollOffPose() const;
      
      // Return pose of charger wrt robot when the robot is on the charger
      static Pose3d GetDockPoseRelativeToRobot(const Robot& robot);
      
      // Returns a quad describing the area in front of the charger
      // that must be clear before the robot can dock with the charger.
      Quad2f GetDockingAreaQuad() const;
      
      // Randomly generate some poses from which to observe the charger 
      // for the purpose of verifying its position
      // (e.g. before attempting to dock with it)
      // NOTE: the poses are randomly sampled in a annulus around the charger
      std::vector<Pose3d> GenerateObservationPoses( Util::RandomGenerator& rng, 
                                                    const size_t nPoses,
                                                    const float& span_rad) const;
      
      //
      // Inherited Virtual Methods
      //
      virtual ~Charger();
      
      virtual Charger*   CloneType() const override;
      virtual void    Visualize(const ColorRGBA& color) const override;
      virtual void    EraseVisualization() const override;
      virtual bool CanIntersectWithRobot() const override { return true; }
      
      // Assume there is exactly one of these objects at a given time
      virtual bool IsUnique() const override  { return true; }

      virtual Point3f GetSameDistanceTolerance()  const override;
      
      
      // Charger has no accelerometer so it should never be considered moving
      virtual bool IsMoving(RobotTimeStamp_t* t = nullptr) const override { return false; }
      virtual void SetIsMoving(bool isMoving, RobotTimeStamp_t t) override { }
      
      virtual f32 GetMaxObservationDistance_mm() const override;
      
      constexpr static f32 GetLength() { return kLength; }
      
      virtual const std::vector<Point3f>& GetCanonicalCorners() const override;
      
      // Model dimensions in mm (perhaps these should come from a configuration
      // file instead?)
      constexpr static const f32 kWallWidth      = 12.f;
      constexpr static const f32 kPlatformWidth  = 64.f;
      constexpr static const f32 kWidth          = 2*kWallWidth + kPlatformWidth;
      constexpr static const f32 kHeight         = 80.f;
      constexpr static const f32 kSlopeLength    = 94.f;
      constexpr static const f32 kPlatformLength = 0.f;
      constexpr static const f32 kLength         = kSlopeLength + kPlatformLength + kWallWidth;
      constexpr static const f32 kMarkerHeight   = 46.f;
      constexpr static const f32 kMarkerWidth    = 46.f;
      constexpr static const f32 kMarkerZPosition   = 48.5f;  // Middle of marker above ground
      constexpr static const f32 kPreAscentDistance  = 100.f; // for ascending from bottom
      constexpr static const f32 kRobotToChargerDistWhenDocked = 30.f;  // Distance from front of charger to robot origin when docked
      constexpr static const f32 kRobotToChargerDistPostRollOff = 80.f;  // Distance from front of charger to robot origin after just having rolled off the charger
      
      protected:
      
      virtual void GeneratePreActionPoses(const PreActionPose::ActionType type,
                                          std::vector<PreActionPose>& preActionPoses) const override;
      
      Point3f _size;
      
      const Vision::KnownMarker* _marker;
      
      mutable VizManager::Handle_t _vizHandle;

    }; // class Charger
    
  } // namespace Vector
} // namespace Anki

#endif // ANKI_COZMO_BASESTATION_CHARGER_H
