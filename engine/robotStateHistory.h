//
//  robotPoseHistory.h
//  Products_Cozmo
//
//  Created by Kevin Yoon on 2014-05-13.
//  Copyright (c) 2014 Anki, Inc. All rights reserved.
//

#ifndef __Anki_Cozmo_RobotStateHistory_H__
#define __Anki_Cozmo_RobotStateHistory_H__

#include "coretech/common/shared/types.h"
#include "coretech/common/engine/math/pose.h"
#include "coretech/common/engine/robotTimeStamp.h"
#include "coretech/vision/engine/camera.h"
#include "clad/types/robotStatusAndActions.h"

#include "util/bitFlags/bitFlags.h"
#include "util/entityComponent/iDependencyManagedComponent.h"
#include "engine/components/sensors/proxSensorComponent.h"
#include "engine/robotComponents_fwd.h"
#include "util/helpers/templateHelpers.h"

namespace Anki {
  namespace Vector {
    
    /*
     * HistRobotState
     *
     * Snapshot of robot pose/state, stored in RobotStateHistory
     */
    class HistRobotState
    {
    public:
      HistRobotState();
      
      HistRobotState(const Pose3d& pose,
                     const RobotState& state,
                     const ProxSensorData& proxData);
      
      HistRobotState(const HistRobotState& other) = default;
      
      HistRobotState& operator=(const HistRobotState& other) = default;

      // Only update pose-related information: includes pose frame ID, body pose in the world, and head/lift angles
      void SetPose(const PoseFrameID_t frameID, const Pose3d& pose, const f32 headAngle_rad, const f32 liftAngle_rad);
      
      void SetPoseParent(const Pose3d& newParent);
      void ClearPoseParent();
      
      const Pose3d&       GetPose()                        const {return _pose;               }
      const f32           GetHeadAngle_rad()               const {return _state.headAngle;    }
      const f32           GetLiftAngle_rad()               const {return _state.liftAngle;    }
      const f32           GetLiftHeight_mm()               const;
      const u16           GetCliffData(CliffSensor sensor) const;
      const PoseFrameID_t GetFrameId()                     const {return _state.pose_frame_id;}
      
      const f32           GetLeftWheelSpeed_mmps()         const {return _state.lwheel_speed_mmps;}
      const f32           GetRightWheelSpeed_mmps()        const {return _state.rwheel_speed_mmps;}

      // TODO: remove this once `_pose` actually contains full 3d orientation (currently it only includes yaw)
      const f32           GetPitch_rad()                   const {return _state.pose.pitch_angle;}

      const ProxSensorData& GetProxSensorData()            const {return _proxData;}

      // Only meant to be used by RobotStateHistory::UpdateProxSensorData()
      // VIC-13035: The better thing to do would be to pull out ProxSensorData into 
      //            its own history buffer and keep HistRobotState as a container for
      //            raw unprocessed states (i.e. RobotState) only.
      void SetProxSensorData(const ProxSensorData& data) {_proxData = data;}
                                                          
      bool WasCarryingObject() const { return  (_state.status & Util::EnumToUnderlying(RobotStatusFlag::IS_CARRYING_BLOCK)); }
      bool WasMoving()         const { return  (_state.status & Util::EnumToUnderlying(RobotStatusFlag::IS_MOVING)); }
      bool WasHeadMoving()     const { return !(_state.status & Util::EnumToUnderlying(RobotStatusFlag::HEAD_IN_POS)); }
      bool WasLiftMoving()     const { return !(_state.status & Util::EnumToUnderlying(RobotStatusFlag::LIFT_IN_POS)); }
      bool WereWheelsMoving()  const { return  (_state.status & Util::EnumToUnderlying(RobotStatusFlag::ARE_WHEELS_MOVING)); }
      bool WasPickedUp()       const { return  (_state.status & Util::EnumToUnderlying(RobotStatusFlag::IS_PICKED_UP)); }
      bool WasCameraMoving()   const { return  (WasHeadMoving() || WereWheelsMoving()); }
      bool WasCliffDetected(CliffSensor sensor) const;

      
      // Returns a new HistRobotState the given fraction between 1 and 2, where fraction is [0,1].
      // Note: always uses histState1's PoseFrameID
      static HistRobotState Interpolate(const HistRobotState& histState1, const HistRobotState& histState2,
                                        const Pose3d& pose2wrtPose1, f32 fraction);
      
      void Print() const;
      
    private:
      
      // Note that the _state.pose is not guaranteed to match what's in _pose (COZMO-10225)
      Pose3d                       _pose;  // robot pose
      RobotState                   _state;
      
      ProxSensorData               _proxData;
      Util::BitFlags8<CliffSensor> _cliffDetectedFlags;
    };
    
    // A key associated with each computed pose retrieved from history
    // to be used to check its validity at a later time.
    using HistStateKey = uint32_t;
    
    /*
     * RobotStateHistory
     *
     * A collection of timestamped HistRobotState for a specified time range.
     * Can be used to compute better pose estimated based on a combination of 
     * raw odometry based poses from the robot and vision-based poses computed by Blockworld.
     * Can also be used to check robot state (e.g. whether carrying an object) at a historical time.  
     *
     */
    class RobotStateHistory : public IDependencyManagedComponent<RobotComponentID>
    {
    public:
      
      RobotStateHistory();

      //////
      // IDependencyManagedComponent functions
      //////
      virtual void InitDependent(Vector::Robot* robot, const RobotCompMap& dependentComps) override {};
      virtual void GetInitDependencies(RobotCompIDSet& dependencies) const override {};
      virtual void GetUpdateDependencies(RobotCompIDSet& dependencies) const override {};
      //////
      // end IDependencyManagedComponent functions
      //////
        
      // Clear all history
      void Clear();
      
      // Returns the number of states that were added via AddRawOdomState() that still remain in history
      size_t GetNumRawStates() const {return _states.size();}
      
      // Returns the number of states that were added via AddVisionOnlyState() that still remain in history
      size_t GetNumVisionStates() const {return _visStates.size();}
      
      // Specify the maximum time span of states that can be held.
      // States that are older than the newest/largest timestamp stored
      // minus windowSize_ms are automatically removed.
      void SetTimeWindow(const u32 windowSize_ms);
      
      // Adds a timestamped state received from the robot to the history.
      // Returns RESULT_FAIL if an entry for that timestamp already exists
      // or the state is too old to be added.
      Result AddRawOdomState(const RobotTimeStamp_t t, const HistRobotState& state);

      // Adds a timestamped state based off of a vision marker to the history.
      // These are used in conjunction with raw odometry states to compute
      // better estimates of the state at any point t in the history.
      Result AddVisionOnlyState(const RobotTimeStamp_t t, const HistRobotState& state);
      
      // Sets p to the raw odometry state nearest the given timestamp t in the history.
      // Interpolates state if withInterpolation == true.
      // Returns OK if t is between the oldest and most recent timestamps stored.
      Result GetRawStateAt(const RobotTimeStamp_t t_request,
                           RobotTimeStamp_t& t, HistRobotState& state,
                           bool withInterpolation = false) const;
      
      // Get raw odometry states (and their times) immediately before and after
      // the state nearest to the requested time. Returns failure if either cannot
      // be found (e.g. when the requested time corresponds to the first or last
      // state in  history).
      Result GetRawStateBeforeAndAfter(const RobotTimeStamp_t t,
                                       RobotTimeStamp_t&    t_before,
                                       HistRobotState& state_before,
                                       RobotTimeStamp_t&    t_after,
                                       HistRobotState& state_after);

      // If a raw state with the given timestamp is found, its proxSensorData
      // is updated. Otherwise, returns RESULT_FAIL.
      //
      // NOTE: Only meant to be used in Robot::UpdateFullRobotState() to update
      //       the robot state history with processed prox data
      // VIC-13035: The better thing to do would be to pull out ProxSensorData into 
      //            its own history buffer and keep HistRobotState as a container for
      //            raw unprocessed states (i.e. RobotState) only.
      Result UpdateProxSensorData(const RobotTimeStamp_t t, const ProxSensorData& data);
      
      // Returns OK and points statePtr to a vision-based state at the specified time if such a state exists.
      // Note: The state that statePtr points to may be invalidated by subsequent calls to
      //       the history like Clear() or Add...(). Use carefully!
      Result GetVisionOnlyStateAt(const RobotTimeStamp_t t_request, HistRobotState** statePtr);
      
      // Same as above except that it uses the last vision-based
      // state that exists at or before t_request to compute a
      // better estimate of the state at time t.
      Result ComputeStateAt(const RobotTimeStamp_t t_request,
                            RobotTimeStamp_t& t, HistRobotState& state,
                            bool withInterpolation = false) const;

      // Same as above except that it also inserts the resulting state
      // as a vision-based state back into history.
      Result ComputeAndInsertStateAt(const RobotTimeStamp_t t_request,
                                     RobotTimeStamp_t& t, HistRobotState** statePtr,
                                     HistStateKey* key = nullptr,
                                     bool withInterpolation = false);

      // Points *statePtr to a computed state in the history that was insert via ComputeAndInsertStateAt
      Result GetComputedStateAt(const RobotTimeStamp_t t_request,
                                HistRobotState ** statePtr,
                                HistStateKey* key = nullptr);

      // NOTE: p is not const, but *statePtr is. So you can assign to *statePtr, or statePtr, but not **statePtr
      Result GetComputedStateAt(const RobotTimeStamp_t t_request,
                                const HistRobotState ** statePtr,
                                HistStateKey* key = nullptr) const;

      // If at least one vision only state exists, the most recent one is returned in p
      // and the time it occured at in t.
      Result GetLatestVisionOnlyState(RobotTimeStamp_t& t, HistRobotState& state) const;
      
      // Get the last state in history with the given pose frame ID.
      Result GetLastStateWithFrameID(const PoseFrameID_t frameID, HistRobotState& state) const;

      // Returns the number of raw states with the given pose frame ID.
      u32 GetNumRawStatesWithFrameID(const PoseFrameID_t frameID) const;
      
      // Checks whether or not the given key is associated with a valid computed state
      bool IsValidKey(const HistStateKey key) const;     
      
      RobotTimeStamp_t GetOldestTimeStamp() const;
      RobotTimeStamp_t GetNewestTimeStamp() const;

      RobotTimeStamp_t GetOldestVisionOnlyTimeStamp() const;
      RobotTimeStamp_t GetNewestVisionOnlyTimeStamp() const;

      // Prints the entire history
      void Print() const;
      
      typedef std::map<RobotTimeStamp_t, HistRobotState> StateMap_t;
      
      const StateMap_t& GetRawStates() const { return _states; }
      
    private:
      
      void CullToWindowSize();
      
      // Pose history as reported by robot
      using StateMapIter_t = StateMap_t::iterator;
      using const_StateMapIter_t = StateMap_t::const_iterator;
      StateMap_t _states;

      // Map of timestamps of vision-based poses as computed from mat markers
      StateMap_t _visStates;

      // Map of poses that were computed with ComputeAt
      StateMap_t _computedStates;

      // The last pose key assigned to a computed pose
      static HistStateKey currHistStateKey_;
      
      // Map of HistStateKeys to timestamps and vice versa
      using TimestampByKeyMap_t           = std::map<HistStateKey, RobotTimeStamp_t> ;
      using TimestampByKeyMapIter_t       = TimestampByKeyMap_t::iterator;
      using KeyByTimestampMap_t           = std::map<RobotTimeStamp_t, HistStateKey>;
      using KeyByTimestampMapIter_t       = KeyByTimestampMap_t::iterator;
      using const_KeyByTimestampMapIter_t = KeyByTimestampMap_t::const_iterator;
      
      TimestampByKeyMap_t _tsByKeyMap;
      KeyByTimestampMap_t _keyByTsMap;
      
      // Size of history time window (ms)
      u32 _windowSize_ms;
      
    }; // class RobotStateHistory

    
  } // namespace Vector
} // namespace Anki

#endif // __Anki_Cozmo_RobotStateHistory_H__
