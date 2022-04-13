/**
 * File: charger.cpp
 *
 * Author: Kevin Yoon
 * Date:   6/5/2015
 *
 * Description: Defines a charger base object, which is a type of ActionableObject
 *
 *
 * Copyright: Anki, Inc. 2015
 **/
#include "engine/charger.h"

#include "engine/robot.h"
#include "engine/utils/robotPointSamplerHelper.h"

#include "anki/cozmo/shared/cozmoConfig.h"
#include "anki/cozmo/shared/cozmoEngineConfig.h"

#include "coretech/common/engine/math/quad.h"

#include "util/console/consoleInterface.h"
#include "util/logging/logging.h"
#include "util/random/randomGenerator.h"

namespace Anki {
  
  namespace Vector {

    namespace 
    {
      // Valid range of radii from which the Robot may observe the charger
      //  with good visibility. Candidate poses are sampled within this range.
      const float kInnerAnnulusRadiusForObservation_mm = 100.f;
      const float kOuterAnnulusRadiusForObservation_mm = 200.f;
    }

    CONSOLE_VAR(f32, kChargerMaxObservationDistance_mm, "Charger", 500.f);
    
    // === Charger predock pose params ===
    // {angle, x, y}
    // angle: angle about z-axis (which runs vertically along marker)
    //     x: distance along marker normal
    //     y: distance along marker horizontal
    const Pose2d kChargerPreDockPoseOffset = {0, 0, 130.f};
    
    const std::vector<Point3f>& Charger::GetCanonicalCorners() const {
    
      static const std::vector<Point3f> CanonicalCorners = {{
        // Bottom corners
        Point3f(kLength, -0.5f*kWidth,  0.f),
        Point3f(0,      -0.5f*kWidth,  0.f),
        Point3f(0,       0.5f*kWidth,  0.f),
        Point3f(kLength,  0.5f*kWidth,  0.f),
        // Top corners:
        Point3f(kLength, -0.5f*kWidth,  kHeight),
        Point3f(0,       -0.5f*kWidth,  kHeight),
        Point3f(0,        0.5f*kWidth,  kHeight),
        Point3f(kLength,  0.5f*kWidth,  kHeight),
      }};
      
      return CanonicalCorners;
      
    } // GetCanonicalCorners()
    
    
    Charger::Charger()
    : ActionableObject(ObjectType::Charger_Basic)
    , _size(kLength, kWidth, kHeight)
    , _vizHandle(VizManager::INVALID_HANDLE)
    {
      Pose3d frontPose(-M_PI_2_F, Z_AXIS_3D(),
                       Point3f{kSlopeLength+kPlatformLength, 0, kMarkerZPosition});
      
      _marker = &AddMarker(Vision::MARKER_CHARGER_HOME, frontPose, Point2f(kMarkerWidth, kMarkerHeight));
      
    } // Charger() Constructor

    
    Charger::~Charger()
    {
      EraseVisualization();
    }

    void Charger::GeneratePreActionPoses(const PreActionPose::ActionType type,
                                         std::vector<PreActionPose>& preActionPoses) const
    {
      preActionPoses.clear();
      
      switch(type)
      {
        case PreActionPose::ActionType::DOCKING:
        case PreActionPose::ActionType::PLACE_RELATIVE:
        {
          const float halfHeight = 0.5f * kHeight;
          
          Pose3d poseWrtMarker(M_PI_2_F + kChargerPreDockPoseOffset.GetAngle().ToFloat(),
                               Z_AXIS_3D(),
                               {kChargerPreDockPoseOffset.GetX() , -kChargerPreDockPoseOffset.GetY(), -halfHeight},
                               _marker->GetPose());
          
          poseWrtMarker.SetName("Charger" + std::to_string(GetID().GetValue()) + "PreActionPose");
          
          preActionPoses.emplace_back(type,
                                      _marker,
                                      poseWrtMarker,
                                      0);
          break;
        }
        case PreActionPose::ActionType::ENTRY:
        case PreActionPose::ActionType::FLIPPING:
        case PreActionPose::ActionType::PLACE_ON_GROUND:
        case PreActionPose::ActionType::ROLLING:
        case PreActionPose::ActionType::NONE:
        {
          break;
        }
      }
    }
    
    Pose3d Charger::GetRobotDockedPose() const
    {
      Pose3d pose(M_PI, Z_AXIS_3D(),
                  Point3f{kRobotToChargerDistWhenDocked, 0, 0},
                  GetPose());
      
      pose.SetName("Charger" + std::to_string(GetID().GetValue()) + "DockedPose");
      
      return pose;
    }
    
    Pose3d Charger::GetRobotPostRollOffPose() const
    {
      Pose3d pose(M_PI_F, Z_AXIS_3D(),
                  Point3f{-kRobotToChargerDistPostRollOff, 0, 0},
                  GetPose());
      
      pose.SetName("Charger" + std::to_string(GetID().GetValue()) + "PostRollOffPose");
      
      return pose;
    }
    
    Pose3d Charger::GetDockPoseRelativeToRobot(const Robot& robot)
    {
      return Pose3d(M_PI_F, Z_AXIS_3D(),
                    Point3f{kRobotToChargerDistWhenDocked, 0, 0},
                    robot.GetPose(),
                    "ChargerDockPose");
    }
    
    Quad2f Charger::GetDockingAreaQuad() const
    {
      // Define the docking area w.r.t. charger. This defines the area in
      // front of the charger that must be clear of obstacles if the robot
      // is to successfully dock with the charger.
      const float xExtent_mm = 120.f;
      const float yExtent_mm = kWidth;
      std::vector<Point3f> dockingAreaPts = {{
        {0.f,         -yExtent_mm/2.f, 0.f},
        {-xExtent_mm, -yExtent_mm/2.f, 0.f},
        {0.f,         +yExtent_mm/2.f, 0.f},
        {-xExtent_mm, +yExtent_mm/2.f, 0.f}
      }};
      
      const auto& chargerPose = GetPose();
      const RotationMatrix3d& R = chargerPose.GetRotationMatrix();
      std::vector<Point2f> points;
      for (auto& pt : dockingAreaPts) {
        // Rotate to charger pose
        pt = R*pt;
        
        // Project onto XY plane, i.e. just drop the Z coordinate
        points.emplace_back(pt.x(), pt.y());
      }
      
      Quad2f boundingQuad = GetBoundingQuad(points);
      
      // Re-center
      Point2f center(chargerPose.GetTranslation().x(), chargerPose.GetTranslation().y());
      boundingQuad += center;
      
      return boundingQuad;
    }
    
    std::vector<Pose3d> Charger::GenerateObservationPoses(Util::RandomGenerator& rng, 
                                                          const size_t nPoses,
                                                          const float& span_rad) const
    {
      // Generate a uniformly distributed set of random poses in a semi-circle (really a semi-annulus) in front of the
      // charger. The poses should point at the charger, and they should not be too far off from the marker normal, so
      // that the robot can see the marker from a reasonable angle.
      const f32 minTheta = M_PI_F - span_rad;
      const f32 maxTheta = M_PI_F + span_rad;
      
      // The charger's origin is at the front of the lip of the charger, and its x axis points inward toward the marker.
      // Therefore we want poses centered around the angle pi (w.r.t. the charger), and pointing toward the charger origin.
      const auto& chargerPose = GetPose();
      std::vector<Pose3d> outPoses;
      outPoses.reserve(nPoses);
      for (int i=0 ; i < nPoses ; i++) {
        const auto pt = RobotPointSamplerHelper::SamplePointInAnnulus(rng, kInnerAnnulusRadiusForObservation_mm, kOuterAnnulusRadiusForObservation_mm, minTheta, maxTheta);
        const f32 th = std::atan2(pt.y(), pt.x());
        outPoses.emplace_back(th + M_PI_F, Z_AXIS_3D(),
                              Vec3f{pt.x(), pt.y(), chargerPose.GetTranslation().z()},
                              chargerPose);
      }
      
      return outPoses;
    }
    
#pragma mark --- Virtual Method Implementations ---
    
    Charger* Charger::CloneType() const
    {
      return new Charger();
    }
    
    void Charger::Visualize(const ColorRGBA& color) const
    {
      Pose3d vizPose = GetPose().GetWithRespectToRoot();
      _vizHandle = _vizManager->DrawCharger(GetID().GetValue(),
                                            Charger::kPlatformLength + Charger::kWallWidth,
                                            Charger::kSlopeLength,
                                            Charger::kWidth,
                                            Charger::kHeight,
                                            vizPose,
                                            color);
    } // Visualize()
    
    
    void Charger::EraseVisualization() const
    {
      // Erase the Charger
      if(_vizHandle != VizManager::INVALID_HANDLE) {
        _vizManager->EraseVizObject(_vizHandle);
        _vizHandle = VizManager::INVALID_HANDLE;
      }
      
      // Erase the pre-action poses
      ActionableObject::EraseVisualization();
    }

    
    // TODO: Make these dependent on Charger type/size?
    Point3f Charger::GetSameDistanceTolerance() const {
      Point3f distTol(kLength*.5f, kWidth*.5f, kHeight*.5f);
      return distTol;
    }
    
    f32 Charger::GetMaxObservationDistance_mm() const
    {
      return kChargerMaxObservationDistance_mm;
    }
    
  } // namespace Vector
} // namespace Anki


