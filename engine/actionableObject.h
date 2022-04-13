/**
 * File: actionableObject.h
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
 *
 * Copyright: Anki, Inc. 2014
 **/

#ifndef ANKI_COZMO_BASESTATION_ACTIONABLE_OBJECT_H
#define ANKI_COZMO_BASESTATION_ACTIONABLE_OBJECT_H

#include "engine/cozmoObservableObject.h"

#include "engine/preActionPose.h"
#include "engine/viz/vizManager.h"

#include "clad/types/objectTypes.h"

namespace Anki {
  namespace Vector {
    
    class ActionableObject : public Vector::ObservableObject
    {
    public:
      ActionableObject(const ObjectType& type);
      
      // Return only those pre-action poses that are "valid" (See protected
      // IsPreActionPoseValid() method below.)
      // Optionally, you may filter based on ActionType and Marker Code as well.
      // Returns true if we had to generate preActionPoses, false if cached poses were used
      // return value is currently only used for unit tests
      bool GetCurrentPreActionPoses(std::vector<PreActionPose>& preActionPoses,
                                    const Pose3d& robotPose,
                                    const std::set<PreActionPose::ActionType>& withAction = std::set<PreActionPose::ActionType>(),
                                    const std::set<Vision::Marker::Code>& withCode = std::set<Vision::Marker::Code>(),
                                    const std::vector<std::pair<Quad2f,ObjectID> >& obstacles = std::vector<std::pair<Quad2f,ObjectID> >(),
                                    const f32 offset_mm = 0,
                                    bool visualize = false) const;
      
      // Draws just the pre-action poses given robotPose
      void VisualizePreActionPoses(const std::vector<std::pair<Quad2f,ObjectID> >& obstacles,
                                   const Pose3d& robotPose) const;
      
      // Just erases pre-action poses (if any were drawn). Subclasses should
      // call this from their virtual EraseVisualization() implementations.
      virtual void EraseVisualization() const override;
      
    protected:
 
      // Only "valid" poses are returned by GetCurrenPreActionPoses
      // By default, allows any rotation around Z, but none around X/Y, meaning
      // the pose must be vertically-oriented to be "valid".
      virtual bool IsPreActionPoseValid(const PreActionPose& preActionPose,
                                        const std::vector<std::pair<Quad2f,ObjectID> >& obstacles) const;
      
      // Generates all possible preAction poses of the given type
      virtual void GeneratePreActionPoses(const PreActionPose::ActionType type,
                                          std::vector<PreActionPose>& preActionPoses) const = 0;
      
      // Set the object's pose. newPose should be with respect to world origin.
      virtual void SetPose(const Pose3d& newPose, f32 fromDistance, PoseState newPoseState) override;
      
    private:      
      mutable std::set<VizManager::Handle_t> _vizPreActionPoseHandles;
      
      // Set of pathIDs for visualizing the preActionLines
      mutable std::set<u32> _vizPreActionLineIDs;
      
      mutable std::array<std::vector<PreActionPose>, PreActionPose::ActionType::NONE> _cachedPreActionPoses;
      
    }; // class ActionableObject
    
  } // namespace Vector
} // namespace Anki

#endif // ANKI_COZMO_BASESTATION_ACTIONABLE_OBJECT_H
