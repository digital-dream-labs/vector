/**
 * File: actionableObject.cpp
 *
 * Author: Andrew Stein
 * Date:   7/9/2014
 *
 * Description: Defines an "Actionable" Object, which is a subclass of an
 *              ObservableObject that can also be interacted with or acted upon.
 *              It extends the (coretech) ObservableObject to have a notion of
 *              docking and entry points, for example, useful for Cozmo's.
 *              These are represented by different types of "pre-action" poses.
 *
 * Copyright: Anki, Inc. 2014
 **/

#include "engine/actionableObject.h"

#include "coretech/common/engine/math/quad.h"
 
#include "util/math/math.h"

#include "anki/cozmo/shared/cozmoEngineConfig.h"

namespace Anki {
  namespace Vector {
        
    ActionableObject::ActionableObject(const ObjectType& type)
    : Vector::ObservableObject(type)
    {
    }
    
    bool ActionableObject::IsPreActionPoseValid(const PreActionPose& preActionPose,
                                                const std::vector<std::pair<Quad2f,ObjectID> >& obstacles) const
    {
      const Pose3d checkPose = preActionPose.GetPose().GetWithRespectToRoot();
      
      // Allow any rotation around Z, but none around X/Y, in order to keep
      // vertically-oriented poses
      const f32 vertAlignThresh = 1.f - std::cos(PreActionPose::ANGLE_TOLERANCE); // TODO: tighten?
      bool isValid = NEAR(checkPose.GetRotationMatrix()(2,2), 1.f, vertAlignThresh);
    
      if(isValid && !obstacles.empty()) {
        // Cheap hack for now (until we use the planner to do this check for us):
        //   Walk a straight line from this preActionPose to the parent object
        //   and check for intersections with the obstacle list. Actually, we will
        //   walk three lines (center, left, and right) so the caller doesn't
        //   have to do some kind of oriented padding of the obstacles to deal
        //   with objects close to the action-object unnecessarily blocking
        //   paths on other sides of the object unnecessarily.
        
        //   (Assumes obstacles are w.r.t. origin...)
        Point2f xyStart(preActionPose.GetPose().GetWithRespectToRoot().GetTranslation());
        const Point2f xyEnd(preActionPose.GetMarker()->GetPose().GetWithRespectToRoot().GetTranslation());
        
        const f32 stepSize = 10.f; // 1cm
        Vec2f   stepVec(xyEnd);
        stepVec -= xyStart;
        f32 lineLength = stepVec.MakeUnitLength();
        Vec2f offsetVec(stepVec.y(), -stepVec.x());
        offsetVec *= 0.5f*ROBOT_BOUNDING_Y;
        
        // Pull back xyStart to the rear of the robot's bounding box when the robot is at the preaction pose.
        xyStart -= (stepVec * (ROBOT_BOUNDING_X - ROBOT_BOUNDING_X_FRONT));
        lineLength += (ROBOT_BOUNDING_X - ROBOT_BOUNDING_X_FRONT);
        
        const s32 numSteps = Util::numeric_cast<s32>(std::floor(lineLength / stepSize));
        
        stepVec *= stepSize;
        
        
        bool pathClear = true;
        Point2f currentPoint(xyStart);
        Point2f currentPointL(xyStart + offsetVec);
        Point2f currentPointR(xyStart - offsetVec);
        for(s32 i=0; i<numSteps && pathClear; ++i) {
          // Check whether the current point along the line is inside any obstacles
          // (excluding this ActionableObject as an obstacle)
          
          // DEBUG VIZ
          //          _vizManager->DrawGenericQuad(i+1000+(0xffff & (long)preActionPose.GetMarker()),
          //                                                     Quad2f(currentPoint+Point2f(-1.f,-1.f),
          //                                                            currentPoint+Point2f(-1.f, 1.f),
          //                                                            currentPoint+Point2f( 1.f,-1.f),
          //                                                            currentPoint+Point2f( 1.f, 1.f)),
          //                                                     1.f, NamedColors::BLUE);
          //          _vizManager->DrawGenericQuad(i+2000+(0xffff & (long)preActionPose.GetMarker()),
          //                                                     Quad2f(currentPointL+Point2f(-1.f,-1.f),
          //                                                            currentPointL+Point2f(-1.f, 1.f),
          //                                                            currentPointL+Point2f( 1.f,-1.f),
          //                                                            currentPointL+Point2f( 1.f, 1.f)),
          //                                                     1.f, NamedColors::RED);
          //          _vizManager->DrawGenericQuad(i+3000+(0xffff & (long)preActionPose.GetMarker()),
          //                                                     Quad2f(currentPointR+Point2f(-1.f,-1.f),
          //                                                            currentPointR+Point2f(-1.f, 1.f),
          //                                                            currentPointR+Point2f( 1.f,-1.f),
          //                                                            currentPointR+Point2f( 1.f, 1.f)),
          //                                                     1.f, NamedColors::GREEN);
          
          // Technically, this quad is already in the list of obstacles, so we could
          // find it rather than recomputing it...
          const Quad2f& boundingQuad = GetBoundingQuadXY();
          
          for(auto & obstacle : obstacles) {
            
            // DEBUG VIZ
            //            _vizManager->DrawGenericQuad(obstacle.second.GetValue(), obstacle.first, 1.f, NamedColors::ORANGE);
            
            // Make sure this obstacle is not from this object (the one we are trying to interact with).
            if(obstacle.second != this->GetID()) {
              // Also make sure the obstacle is not part of a stack this one belongs
              // to, by seeing if its centroid is contained within this object's
              // bounding quad
              if(boundingQuad.Contains(obstacle.first.ComputeCentroid()) == false) {
                if(obstacle.first.Contains(currentPoint)  ||
                   obstacle.first.Contains(currentPointR) ||
                   obstacle.first.Contains(currentPointL)) {
                  pathClear = false;
                  break;
                }
              }
            }
          }
          // Take a step along the line
          assert( ((currentPoint +stepVec)-xyEnd).Length() < (currentPoint -xyEnd).Length());
          assert( ((currentPointL+stepVec)-xyEnd).Length() < (currentPointL-xyEnd).Length());
          assert( ((currentPointR+stepVec)-xyEnd).Length() < (currentPointR-xyEnd).Length());
          currentPoint  += stepVec;
          currentPointR += stepVec;
          currentPointL += stepVec;
        }

        if(pathClear == false) {
          isValid = false;
        }
      
      }
      
      return isValid;
      
    } // IsPreActionPoseValid()

    
    bool ActionableObject::GetCurrentPreActionPoses(std::vector<PreActionPose>& preActionPoses,
                                                    const Pose3d& robotPose,
                                                    const std::set<PreActionPose::ActionType>& withAction,
                                                    const std::set<Vision::Marker::Code>& withCode,
                                                    const std::vector<std::pair<Quad2f,ObjectID> >& obstacles,
                                                    const f32 offset_mm,
                                                    bool visualize) const
    {
      bool res = false;
      const Pose3d& relToObjectPose = GetPose();
      
      u8 count = 0;
      
      std::vector<PreActionPose> genPreActionPoses;
      for(const PreActionPose::ActionType type : withAction)
      {
        std::vector<PreActionPose>& cachedPoses = _cachedPreActionPoses[type];
        
        // If we don't have any cached preAction poses, generate them
        if(cachedPoses.empty())
        {
          GeneratePreActionPoses(type, cachedPoses);
          res = true;
        }
        
        genPreActionPoses.insert(genPreActionPoses.end(), cachedPoses.begin(), cachedPoses.end());
      }
      
      for(const auto & preActionPose : genPreActionPoses)
      {
        if((withCode.empty()   || withCode.count(preActionPose.GetMarker()->GetCode()) > 0) &&
           (withAction.empty() || withAction.count(preActionPose.GetActionType()) > 0))
        {
          // offset_mm is scaled by some amount because otherwise it might too far to see the marker
          // it's docking to.
          PreActionPose currentPose(preActionPose, relToObjectPose, preActionPose.GetLineLength(), PREACTION_POSE_OFFSET_SCALAR * offset_mm);
          
          // If our preActionPoses aren't using an offset then use the point on the preActionLine closest to the robot
          if(offset_mm == 0)
          {
            // Find the end point of the preActionLine
            const f32 angle = currentPose.GetPose().GetRotation().GetAngleAroundZaxis().ToFloat();
            const Point3f endPoint = {currentPose.GetPose().GetTranslation().x() + cosf(angle) * preActionPose.GetLineLength(),
                                      currentPose.GetPose().GetTranslation().y() + sinf(angle) * preActionPose.GetLineLength(),
                                      currentPose.GetPose().GetTranslation().z()};
            
            Pose3d p = currentPose.GetPose().GetWithRespectToRoot();
            Pose3d robot = robotPose.GetWithRespectToRoot();
            
            // x and y difference between the intersection point and the start of the preActionLine
            f32 x = 0;
            f32 y = 0;
            
            const f32 xDiff = endPoint.x() - p.GetTranslation().x();
            const f32 yDiff = endPoint.y() - p.GetTranslation().y();
            
            // Vertical preActionLine
            if(NEAR_ZERO(xDiff))
            {
              y = robot.GetTranslation().y() - p.GetTranslation().y();
            }
            // Horizontal preActionLine
            else if(NEAR_ZERO(yDiff))
            {
              x = robot.GetTranslation().x() - p.GetTranslation().x();
            }
            else
            {
              // Find the point on the preActionLine that is closest to the robot's pose
              const f32 m = (yDiff) / (xDiff);
              const f32 b = p.GetTranslation().y() - m * p.GetTranslation().x();
            
              const f32 b_inv = robot.GetTranslation().y() + robot.GetTranslation().x()/m;
              
              const f32 x_intersect = (b_inv - b) / (m + (1.f/m));
              const f32 y_intersect = - (x_intersect / m) + b_inv;
              
              // Find the offset the closest point on the line to the robot is from the start of the preActionLine (pose of the
              // preActionPose)
              x = x_intersect - p.GetTranslation().x();
              y = y_intersect - p.GetTranslation().y();
            }
            
            // Clip the offset so it will stay on the preActionLine
            // Offset will always be positive which is what causes the slightly odd (but desirable) behavior of the
            // preDock pose moving away from the robot when we are infront of the end of the preActionLine closest to the object
            const f32 offset = CLIP(sqrtf(x*x + y*y), 0.f, preActionPose.GetLineLength());
          
            PreActionPose newPose(preActionPose, relToObjectPose, preActionPose.GetLineLength(), offset);
            currentPose = newPose;
          }
          
          if(IsPreActionPoseValid(currentPose, obstacles)) {
            preActionPoses.emplace_back(currentPose);
            
            // Draw the preActionLines in viz
            if(visualize)
            {
              PreActionPose basePose(preActionPose, relToObjectPose, preActionPose.GetLineLength(), 0);
              Pose3d end = basePose.GetPose();
              const f32 endAngle = end.GetRotation().GetAngleAroundZaxis().ToFloat();
              end.SetTranslation({end.GetTranslation().x() - cosf(endAngle)*preActionPose.GetLineLength(),
                                  end.GetTranslation().y() - sinf(endAngle)*preActionPose.GetLineLength(),
                                  end.GetTranslation().z()});
              
              // Arbitrarily add 100 to the pathID so it doesn't conflict with other pathIDs like path planner paths
              u32 id = GetID() + 100 + (count++);
              _vizPreActionLineIDs.insert(id);
              _vizManager->ErasePath(id);
              _vizManager->AppendPathSegmentLine(id,
                                                 basePose.GetPose().GetTranslation().x(),
                                                 basePose.GetPose().GetTranslation().y(),
                                                 end.GetTranslation().x(),
                                                 end.GetTranslation().y());
              _vizManager->SetPathColor(id, NamedColors::CYAN);
            }
          }
        } // if preActionPose has correct code/action
      } // for each preActionPose
      return res;
    }
    
    void ActionableObject::VisualizePreActionPoses(const std::vector<std::pair<Quad2f,ObjectID> >& obstacles,
                                                   const Pose3d& robotPose) const
    {
      // Draw the pre-action poses, using a different color for each type of action
      u32 poseID = 0;
      std::vector<PreActionPose> poses;
      
      for(PreActionPose::ActionType actionType : {PreActionPose::DOCKING, PreActionPose::ENTRY})
      {
        GetCurrentPreActionPoses(poses, robotPose, {actionType}, std::set<Vision::Marker::Code>(), obstacles, 0, true);
        for(auto & pose : poses) {
          // TODO: In computing poseID to pass to DrawPreDockPose, multiply object ID by the max number of
          //       preaction poses we expect to visualize per object. Currently, hardcoded to 48 (4 dock and
          //       4 roll per side). We probably won't have more than this.
          _vizPreActionPoseHandles.emplace(_vizManager->DrawPreDockPose(poseID + GetID().GetValue()*48,
                                                                        pose.GetPose().GetWithRespectToRoot(),
                                                                        PreActionPose::GetVisualizeColor(actionType)));

          ++poseID;
        }
        
        poses.clear();
      } // for each actionType
      
    } // VisualizeWithPreActionPoses()
    
    
    void ActionableObject::EraseVisualization() const
    {
      // Erase preActionPoses
      for(auto & preActionPoseHandle : _vizPreActionPoseHandles) {
        if(preActionPoseHandle != VizManager::INVALID_HANDLE) {
          _vizManager->EraseVizObject(preActionPoseHandle);
        }
      }
      _vizPreActionPoseHandles.clear();
      
      // Erase preActionLines
      for(u32 id : _vizPreActionLineIDs) {
        _vizManager->ErasePath(id);
      }
      _vizPreActionLineIDs.clear();
    }
    
    void ActionableObject::SetPose(const Pose3d& newPose, f32 fromDistance, PoseState newPoseState)
    {
      // Clear all of the cached preActionPoses
      for(auto& cachedPoses : _cachedPreActionPoses)
      {
        cachedPoses.clear();
      }
      
      ObservableObject::SetPose(newPose, fromDistance, newPoseState);
    }
    
  } // namespace Vector
} // namespace Anki
