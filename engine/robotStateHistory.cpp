//
//  robotPoseHistory.cpp
//  Products_Cozmo
//
//  Created by Kevin Yoon 2014-05-13
//  Copyright (c) 2014 Anki, Inc. All rights reserved.
//

#include "robotStateHistory.h"
#include "anki/cozmo/shared/cozmoConfig.h"
#include "engine/robot.h"

#include "coretech/common/robot/utilities.h"

#include "util/logging/logging.h"
#include "util/math/math.h"

#define LOG_CHANNEL "RobotStateHistory"

#define DEBUG_ROBOT_POSE_HISTORY 0

namespace Anki {
  namespace Vector {

    //////////////////////// HistRobotState /////////////////////////
    
    HistRobotState::HistRobotState()
    : _state(Robot::GetDefaultRobotState())
    {

    }
    

    HistRobotState::HistRobotState(const Pose3d& pose,
                                   const RobotState& state,
                                   const ProxSensorData& proxData)
    {
      _pose  = pose;
      _state = state;
      _proxData = proxData;
      _cliffDetectedFlags.SetFlags(state.cliffDetectedFlags);
    }
    
    void HistRobotState::SetPose(const PoseFrameID_t frameID, const Pose3d& pose,
                                 const f32 headAngle_rad, const f32 liftAngle_rad)
    {
      _pose = pose;
      _state.pose_frame_id = frameID;
      _state.headAngle = headAngle_rad;
      _state.liftAngle = liftAngle_rad;
    }
    
    void HistRobotState::SetPoseParent(const Pose3d& newParent)
    {
      _pose.SetParent(newParent);
    }
    
    void HistRobotState::ClearPoseParent()
    {
      _pose.ClearParent();
    }
    
    const f32 HistRobotState::GetLiftHeight_mm() const
    {
      return ConvertLiftAngleToLiftHeightMM(_state.liftAngle);
    }
    
    const u16 HistRobotState::GetCliffData(CliffSensor sensor) const
    {
      DEV_ASSERT(sensor < CliffSensor::CLIFF_COUNT, "HistRobotState.GetCliffData.InvalidIndex");
      return _state.cliffDataRaw[Util::EnumToUnderlying(sensor)];
    }
    
    void HistRobotState::Print() const
    {
      printf("Frame %d, headAng %f, cliff %d %d %d %d, carrying %s, moving %s, whichMoving [%s%s%s]",
             GetFrameId(), GetHeadAngle_rad(),
             GetCliffData(CliffSensor::CLIFF_FL),
             GetCliffData(CliffSensor::CLIFF_FR),
             GetCliffData(CliffSensor::CLIFF_BL),
             GetCliffData(CliffSensor::CLIFF_BR),
             WasCarryingObject() ? "Y" : "N",
             WasMoving()         ? "Y" : "N",
             WasHeadMoving()     ? "H" : "",
             WasLiftMoving()     ? "L" : "",            
             WereWheelsMoving()  ? "B" : "");
      _pose.Print();
    }
    
    bool HistRobotState::WasCliffDetected(CliffSensor sensor) const
    {
      DEV_ASSERT(sensor < CliffSensor::CLIFF_COUNT, "HistRobotState.WasCliffDetected.InvalidIndex");
      return _cliffDetectedFlags.IsBitFlagSet(sensor);
    }
    
    HistRobotState HistRobotState::Interpolate(const HistRobotState& histState1, const HistRobotState& histState2,
                                               const Pose3d& pose2wrtPose1, f32 fraction)
    {
      DEV_ASSERT(Util::IsFltGE(fraction, 0.f) && Util::IsFltLE(fraction, 1.f),
                 "HistRobotState.Interpolate.FractionOOR");
      
      const bool isCloserToFirst = Util::IsFltLT(fraction, 0.5f);
      const HistRobotState* closestHistRobotState = isCloserToFirst ? &histState1 : &histState2;
      
      //
      // Interpolate RobotState data
      //
      
      // For now, just take most state info from whichever entry is closer in time.
      RobotState interpState = closestHistRobotState->_state;
      
      DEV_ASSERT(histState1._state.pose_frame_id == histState2._state.pose_frame_id,
                 "HistRobotState.Interpolate.MisMatchedPoseFrameIDs");
      interpState.pose_frame_id = histState1.GetFrameId();
      
      // Interp head angle
      interpState.headAngle = histState1.GetHeadAngle_rad() + fraction * (histState2.GetHeadAngle_rad() - histState1.GetHeadAngle_rad());
      
      // Interp lift angle
      interpState.liftAngle = histState1.GetLiftAngle_rad() + fraction * (histState2.GetLiftAngle_rad() - histState1.GetLiftAngle_rad());

      // Interp pitch
      interpState.pose.pitch_angle = histState1.GetPitch_rad() + fraction * (histState2.GetPitch_rad() - histState1.GetPitch_rad());
      
      // Interp cliff data
      for (int i=0 ; i<interpState.cliffDataRaw.size() ; i++) {
        const auto s = static_cast<CliffSensor>(i);
        interpState.cliffDataRaw[i] = std::round(f32(histState1.GetCliffData(s)) + fraction * f32(histState2.GetCliffData(s) - histState1.GetCliffData(s)));
      }
      
      // Interp wheel speeds
      interpState.lwheel_speed_mmps = histState1.GetLeftWheelSpeed_mmps() + fraction * (histState2.GetLeftWheelSpeed_mmps() - histState1.GetLeftWheelSpeed_mmps());
      interpState.rwheel_speed_mmps = histState1.GetRightWheelSpeed_mmps() + fraction * (histState2.GetRightWheelSpeed_mmps() - histState1.GetRightWheelSpeed_mmps());
      
      // Interp prox data
      // Only interpolating the ProxSensorData struct instead of the ProxSensorDataRaw struct
      // ProxSensorDataRaw struct in RobotState. If there's a use case for exposing the raw data
      // we should interpolate here. Yes, there's some data duplication for convenience of having
      // all the useful stuff in ProxSensorData.
      ProxSensorData interpProxData = closestHistRobotState->GetProxSensorData();   // Full copy to take care of the bools
      interpProxData.distance_mm = std::round(f32(histState1.GetProxSensorData().distance_mm) + fraction * f32(histState2.GetProxSensorData().distance_mm - histState1.GetProxSensorData().distance_mm));
      interpProxData.signalQuality = histState1.GetProxSensorData().signalQuality + fraction * (histState2.GetProxSensorData().signalQuality - histState1.GetProxSensorData().signalQuality); 

      // TODO: Interpolate other things as needed
      
      //
      // Interpolate Pose3d data
      //
      
      // Compute scaled transform to get interpolated pose
      // NOTE: Assuming there is only z-axis rotation!
      // TODO: Make generic?
      Vec3f interpTrans(histState1.GetPose().GetTranslation());
      interpTrans += pose2wrtPose1.GetTranslation() * fraction;
      const Radians interpRotation = histState1.GetPose().GetRotationAngle<'Z'>() + Radians(pose2wrtPose1.GetRotationAngle<'Z'>() * fraction);
      const Pose3d interpPose(interpRotation, Z_AXIS_3D(), interpTrans, histState1.GetPose().GetParent());
      
      //
      // Interpolate booleans
      //
      interpState.cliffDetectedFlags = closestHistRobotState->_cliffDetectedFlags.GetFlags();
      
      // Construct interpolated HistRobotState to return
      HistRobotState interpHistState(interpPose,
                                     interpState,
                                     interpProxData);
      
      return interpHistState;
    }
    
    /////////////////////// RobotStateHistory /////////////////////////////
    
    HistStateKey RobotStateHistory::currHistStateKey_ = 0;
    
    RobotStateHistory::RobotStateHistory()
    : IDependencyManagedComponent(this, RobotComponentID::StateHistory)
    , _windowSize_ms(3000)
    {

    }

    void RobotStateHistory::Clear()
    {
      LOG_INFO("RobotStateHistory.Clear", "Clearing history");
    
      _states.clear();
      _visStates.clear();
      _computedStates.clear();
      
      _tsByKeyMap.clear();
      _keyByTsMap.clear();
    }
    
    void RobotStateHistory::SetTimeWindow(const u32 _windowSize_msms)
    {
      _windowSize_ms = _windowSize_msms;
      CullToWindowSize();
    }
    
    
    Result RobotStateHistory::AddRawOdomState(const RobotTimeStamp_t t,
                                              const HistRobotState& state)
    {
      if (!_states.empty())
      {
        RobotTimeStamp_t newestTime = _states.rbegin()->first;
        if (newestTime > _windowSize_ms && t < newestTime - _windowSize_ms) {
          LOG_WARNING("RobotStateHistory.AddRawOdomState.TimeTooOld", "newestTime %u, oldestAllowedTime %u, t %u",
                      (TimeStamp_t)newestTime, (TimeStamp_t)(newestTime - _windowSize_ms), (TimeStamp_t)t);
          return RESULT_FAIL;
        }
      }
    
      if (state.GetPose().HasParent() && !state.GetPose().GetParent().IsRoot()) {
        LOG_ERROR("RobotStateHistory.AddRawOdomState.NonFlattenedPose",
                  "Pose object inside pose stamp should be flattened (%s)",
                  state.GetPose().GetNamedPathToRoot(false).c_str());
        return RESULT_FAIL;
      }
      
      std::pair<StateMapIter_t, bool> res;
      res = _states.emplace(t, state);

      if (!res.second) {
        LOG_WARNING("RobotStateHistory.AddRawOdomState.AddFailed", "Time: %u", (TimeStamp_t)t);
        return RESULT_FAIL;
      }

      CullToWindowSize();
      
      return RESULT_OK;
    }

    Result RobotStateHistory::AddVisionOnlyState(const RobotTimeStamp_t t,
                                                const HistRobotState& state)
    {
      if (state.GetPose().HasParent() && !state.GetPose().GetParent().IsRoot()) {
        LOG_ERROR("RobotStateHistory.AddVisionOnlyState.NonFlattenedPose",
                  "Pose object inside pose stamp should be flattened (%s)",
                  state.GetPose().GetNamedPathToRoot(false).c_str());
        return RESULT_FAIL;
      }

      // Check if the pose's timestamp is too old.
      if (!_states.empty()) {
        RobotTimeStamp_t newestTime = _states.rbegin()->first;
        if (newestTime > _windowSize_ms && t < newestTime - _windowSize_ms) {
          LOG_ERROR("RobotStateHistory.AddVisionOnlyState.TooOld",
                    "Pose at t=%d too old to add. Newest time=%d, windowSize=%d",
                    (TimeStamp_t)t, (TimeStamp_t)newestTime, _windowSize_ms);
          return RESULT_FAIL;
        }
      }
      
      // If visPose entry exist at t, then overwrite it
      StateMapIter_t it = _visStates.find(t);
      if (it != _visStates.end()) {
        const u32 oldFrameId = it->second.GetFrameId();
        
        it->second = state;
        
        if (ANKI_DEV_CHEATS)
        {
          const u32 curId = state.GetFrameId();
          u32 prevId = 0;
          u32 nextId = std::numeric_limits<u32>::max();
        
          std::stringstream ss;
          ss << "Old id:" << oldFrameId;
          ss << " t:" << t;
          ss << " New id:" << curId;
          ss << " t:" << it->first;
          
          if (it != _visStates.begin())
          {
            --it;
            prevId = it->second.GetFrameId();
            ss << " Previous entry id:" << prevId;
            ss << " t:" << it->first;
            ++it;
          }
          
          ++it;
          if (it != _visStates.end())
          {
            nextId = it->second.GetFrameId();
            ss << " Next entry id:" << nextId;
            ss << " t:" << it->first;
          }
          --it;
          
          LOG_INFO("RobotStateHistory.AddVisionOnlyState.Overwriting", "%s", ss.str().c_str());
          
          DEV_ASSERT((prevId <= curId) && (curId <= nextId),
                     "RobotStateHistory.AddVisionOnlyState.FrameIDsOutOfOrder");
        }
      } else {
      
        std::pair<StateMapIter_t, bool> res;
        res = _visStates.emplace(t, state);
      
        if (!res.second) {
          LOG_ERROR("RobotStateHistory.AddVisionOnlyState.EmplaceFailed",
                    "Emplace of pose with t=%d, frameID=%d failed",
                    (TimeStamp_t)t, state.GetFrameId());
          return RESULT_FAIL;
        }
        
        CullToWindowSize();
      }
      
      return RESULT_OK;
    }

    
    Result RobotStateHistory::GetRawStateBeforeAndAfter(const RobotTimeStamp_t t,
                                                        RobotTimeStamp_t&    t_before,
                                                        HistRobotState& state_before,
                                                        RobotTimeStamp_t&    t_after,
                                                        HistRobotState& state_after)
    {
      // Get the iterator for time t
      const_StateMapIter_t it = _states.lower_bound(t);
      
      if (it == _states.begin() || it == _states.end() || t < _states.begin()->first) {
        return RESULT_FAIL;
      }

      // Get iterator to pose just before t
      const_StateMapIter_t prev_it = it;
      --prev_it;
      
      t_before = prev_it->first;
      state_before = prev_it->second;
      
      // Get iterator to pose just after t
      const_StateMapIter_t next_it = it;
      ++next_it;
      
      if (next_it == _states.end()) {
        return RESULT_FAIL;
      }
      
      t_after = next_it->first;
      state_after = next_it->second;
      
      return RESULT_OK;
    }
    
    // Sets p to the pose nearest the given timestamp t.
    // Interpolates pose if withInterpolation == true.
    // Returns OK if t is between the oldest and most recent timestamps stored.
    Result RobotStateHistory::GetRawStateAt(const RobotTimeStamp_t t_request,
                                            RobotTimeStamp_t& t, 
                                            HistRobotState& state,
                                            bool withInterpolation) const
    {
      // This pose occurs at or immediately after t_request
      const_StateMapIter_t it = _states.lower_bound(t_request);
      
      // Check if in range
      if (it == _states.end() || t_request < _states.begin()->first) {
        return RESULT_FAIL;
      }
      
      if (t_request == it->first) {
        // If the exact timestamp was found, return the corresponding pose.
        t = it->first;
        state = it->second;
      } else {

        // Get iterator to the pose just before t_request
        const_StateMapIter_t prev_it = it;
        --prev_it;

        // Check for same frameId
        // (Shouldn't interpolate between poses from different frameIDs)
        if (it->second.GetFrameId() != prev_it->second.GetFrameId())
        {
          LOG_INFO("RobotStateHistory.GetRawStateAt.MisMatchedFrameIds",
                   "Cannot interpolate at t=%u as requested because the two frame IDs don't match: prev=%d vs next=%d",
                    (TimeStamp_t)t_request,
                    prev_it->second.GetFrameId(),
                   it->second.GetFrameId());
          
          // they asked us for a t_request that is between two frame IDs, which for all intents and purposes is just
          // as bad as trying to choose between two poses with mismatched origins (like above).
          return RESULT_FAIL_ORIGIN_MISMATCH;
        }

        bool inSameOrigin;
        if (withInterpolation)
        {
          // Get the pose transform between the two poses.
          // We don't need to check return value (bool) here because we've effectively
          // checked it already in the call to HasSameRootAs above
          Pose3d pTransform;
          inSameOrigin = it->second.GetPose().GetWithRespectTo(prev_it->second.GetPose(), pTransform);

          if (inSameOrigin)
          {
            // Compute scale factor between time to previous pose and time between previous pose and next pose.
            const f32 timeScale = (f32)(t_request - prev_it->first) / TimeStamp_t(it->first - prev_it->first);

            state = HistRobotState::Interpolate(prev_it->second, it->second, pTransform, timeScale);

            t = t_request;
          }
        }
        else
        {
          inSameOrigin = it->second.GetPose().HasSameRootAs(prev_it->second.GetPose());
          
          if (inSameOrigin)
          {
            // Return the pose closest to the requested time
            if (it->first - t_request < t_request - prev_it->first) {
              t = it->first;
              state = it->second;
            } else {
              t = prev_it->first;
              state = prev_it->second;
            }
          }
        }

        if (!inSameOrigin)
        {
          LOG_INFO("RobotStateHistory.GetRawStateAt.MisMatchedOrigins",
                   "Cannot interpolate at t=%u as requested because the two poses don't share the same origin: prev=%s vs next=%s",
                   (TimeStamp_t)t_request,
                   prev_it->second.GetPose().FindRoot().GetName().c_str(),
                   it->second.GetPose().FindRoot().GetName().c_str());

          // they asked us for a t_request that is between two origins. We can't interpolate or decide which origin is
          // "right" for you, so, we are going to fail
          return RESULT_FAIL_ORIGIN_MISMATCH;
        }
      }
      
      return RESULT_OK;
    }

    Result RobotStateHistory::UpdateProxSensorData(const RobotTimeStamp_t t, const ProxSensorData& data)
    {
      StateMapIter_t it = _states.find(t); 
      if (it == _states.end()) {
        return RESULT_FAIL;
      }
      
      HistRobotState& state = it->second;
      state.SetProxSensorData(data);

      return RESULT_OK;
    }

    Result RobotStateHistory::GetVisionOnlyStateAt(const RobotTimeStamp_t t_request, HistRobotState** state)
    {
      StateMapIter_t it = _visStates.find(t_request);
      if (it != _visStates.end()) {
        *state = &(it->second);
        return RESULT_OK;
      }
      return RESULT_FAIL;
    }
    
    Result RobotStateHistory::ComputeStateAt(const RobotTimeStamp_t t_request,
                                             RobotTimeStamp_t& t, 
                                             HistRobotState& state,
                                             bool withInterpolation) const
    {
      // If the vision-based version of the pose exists, return it.
      const_StateMapIter_t it = _visStates.find(t_request);
      if (it != _visStates.end()) {
        t = t_request;
        state = it->second;
        return RESULT_OK;
      }
      
      // Get the raw pose at the requested timestamp
      HistRobotState state1;
      const Result getRawResult = GetRawStateAt(t_request, t, state1, withInterpolation);
      if (RESULT_OK != getRawResult) {
        return getRawResult;
      }
      
      // Now get the previous vision-based pose
      const_StateMapIter_t git = _visStates.lower_bound(t);
      
      // If there are no vision-based poses then return the raw pose that we just got
      if (git == _visStates.end()) {
        if (_visStates.empty()) {
          state = state1;
          return RESULT_OK;
        } else {
          --git;
        }
      } else if (git->first != t) {
        // If this is the first vision-based pose then return the raw pose that we got
        if (git == _visStates.begin()) {
          state = state1;
          return RESULT_OK;
        } else {
          // As long as the vision-based pose is not from time t,
          // decrement the pointer to get the previous vision-based
          --git;
        }
      }
      
      // Check frame ID
      // If the vision pose frame id <= requested frame id
      // then just return the raw pose of the requested frame id since it
      // is already based on the vision-based pose.
      if (git->second.GetFrameId() <= state1.GetFrameId()) {
        //printf("FRAME %d <= %d\n", git->second.GetFrameId(), p1.GetFrameId());
        state = state1;
        return RESULT_OK;
      }
      
      #if (DEBUG_ROBOT_POSE_HISTORY)
      static bool printDbg = false;
      if(printDbg) {
        printf("gt: %d\n", git->first);
        git->second.GetPose().Print();
      }
      #endif
      
      // git now points to the latest vision-based pose that exists before time t.
      // Now get the pose in _states that immediately follows the vision-based pose's time.
      const_StateMapIter_t p0_it = _states.lower_bound(git->first);

      #if (DEBUG_ROBOT_POSE_HISTORY)
      if (printDbg) {
        printf("p0_it: t: %d  frame: %d\n", p0_it->first, p0_it->second.GetFrameId());
        p0_it->second.GetPose().Print();
      
        printf("p1: t: %d  frame: %d\n", t, p1.GetFrameId());
        p1.GetPose().Print();
      }
      #endif
     
      // Compute the total transformation taking us from the vision-only pose at time
      // corresponding to p0, forward to p1. We will be applying this transformation
      // to whatever is stored in the vision-only pose below.
      Pose3d pTransform;
      if (p0_it->second.GetFrameId() == state1.GetFrameId())
      {
        // Special case: no intermediate frames to chain through. The total transformation
        // is just going from p0 to p1.
        Pose3d p1_wrt_p0_parent;
        const bool inSameOrigin = state1.GetPose().GetWithRespectTo(p0_it->second.GetPose().GetParent(), pTransform);
        DEV_ASSERT(inSameOrigin, "RobotStateHistory.ComputeStateAt.FailedGetWRT1");
        pTransform *= p0_it->second.GetPose().GetInverse();
      }
      else
      {
        const_StateMapIter_t pMid0 = p0_it;
        const_StateMapIter_t pMid1 = p0_it;
        for (pMid1 = p0_it; pMid1 != _states.end(); ++pMid1)
        {
          // Bump pMid1 forward until it hits the next frame ID
          if (pMid1->second.GetFrameId() > pMid0->second.GetFrameId())
          {
            // pMid1 is now pointing to the first pose in the next frame after pMid0.
            // Point pMid1 to the last pose of the same frame as pMid0. Compute
            // the transform for this frame (from pMid0 to pMid1) and
            // fold it into the running total stored in pTransform.
            
            // We expect the beginning (pMid0) and end (pMid1) of this part of history
            // to have the same frame ID and origin.
            --pMid1; // (temporarily) move back to last pose in same frame as pMid0
            DEV_ASSERT(pMid0->second.GetFrameId() == pMid1->second.GetFrameId(),
                       "RobotStateHistory.ComputeStateAt.MismatchedIntermediateFrameIDs");
            DEV_ASSERT(pMid0->second.GetPose().HasSameRootAs(pMid1->second.GetPose()),
                       "RobotStateHistory.ComputeStateAt.MismatchedIntermediateOrigins");

            // Get pMid0 and pMid1 w.r.t. the same parent and store in the intermediate
            // transformation pMidTransform, which is going to hold the transformation
            // from pMid0 to pMid1
            Pose3d pMidTransform;
            const bool inSameOrigin = pMid1->second.GetPose().GetWithRespectTo(pMid0->second.GetPose().GetParent(), pMidTransform);
            DEV_ASSERT(inSameOrigin, "RobotStateHistory.ComputeStateAt.FailedGetWRT2");
            
            // pMidTransform = pMid1 * pMid0^(-1)
            pMidTransform *= pMid0->second.GetPose().GetInverse();
            
            // Fold the transformation from pMid0 to pMid1 into the total transformation thus far
            //  pTranform = pMidTransform * pTransform
            pTransform.PreComposeWith(pMidTransform);
            
            // Move both pointers to start of next pose frame to begin process again
            ++pMid1;
            pMid0 = pMid1;
          }
       
          if (pMid1->second.GetFrameId() == state1.GetFrameId())
          {
            // Reached p1, so we're done
            break;
          }
        }
      }

      #if (DEBUG_ROBOT_POSE_HISTORY)
      if (printDbg) {
        printf("pTrans: %d\n", t);
        pTransform.Print();
      }
      #endif
      
      // NOTE: We are about to return p, which is a transformed version of the vision-only
      // pose in "git", so it should still be relative to whatever "git" was relative to.
      pTransform *= git->second.GetPose(); // Apply pTransform to git and store in pTransform
      pTransform.SetParent(git->second.GetPose().GetParent()); // Keep git's parent
      state.SetPose(state1.GetFrameId(), pTransform, state1.GetHeadAngle_rad(), state1.GetLiftAngle_rad());
      
      return RESULT_OK;
    }
    
    Result RobotStateHistory::ComputeAndInsertStateAt(const RobotTimeStamp_t t_request,
                                                      RobotTimeStamp_t& t, HistRobotState** state,
                                                      HistStateKey* key,
                                                      bool withInterpolation)
    {
      HistRobotState state_computed;
      //printf("COMPUTE+INSERT\n");
      const Result computeResult = ComputeStateAt(t_request, t, state_computed, withInterpolation);
      if (RESULT_OK != computeResult) {
        *state = nullptr;
        return computeResult;
      }
      
      // If computedPose entry exist at t, then overwrite it
      StateMapIter_t it = _computedStates.find(t);
      if (it != _computedStates.end()) {
        it->second = state_computed;
        *state = &(it->second);
        
        if (key) {
          *key = _keyByTsMap[t];
        }
      } else {
        
        std::pair<StateMapIter_t, bool> res;
        res = _computedStates.emplace(t, state_computed);
        
        if (!res.second) {
          return RESULT_FAIL;
        }
        
        *state = &(res.first->second);
        
        
        // Create key associated with computed pose
        ++currHistStateKey_;
        _tsByKeyMap.emplace(std::piecewise_construct,
                            std::forward_as_tuple(currHistStateKey_),
                            std::forward_as_tuple(t));
        _keyByTsMap.emplace(std::piecewise_construct,
                                  std::forward_as_tuple(t),
                                  std::forward_as_tuple(currHistStateKey_));
        
        if (key) {
          *key = currHistStateKey_;
        }

      }
      
      return RESULT_OK;
    }

    Result RobotStateHistory::GetComputedStateAt(const RobotTimeStamp_t t_request,
                                                 const HistRobotState ** state,
                                                 HistStateKey* key) const
    {
      const_StateMapIter_t it = _computedStates.find(t_request);
      if (it != _computedStates.end()) {
        *state = &(it->second);
        
        // Get key for the computed pose
        if (key){
          const_KeyByTimestampMapIter_t kIt = _keyByTsMap.find(it->first);
          if (kIt == _keyByTsMap.end()) {
            LOG_WARNING("RobotStateHistory.GetComputedStateAt.KeyNotFound","");
            return RESULT_FAIL;
          }
          *key = kIt->second;
        }
        
        return RESULT_OK;
      }
      
      // TODO: Compute the pose if it doesn't exist already?
      // ...
      
      return RESULT_FAIL;
    }


    // TODO:(bn) is there a way to avoid this duplicated code here? Not eager to use templates...
    Result RobotStateHistory::GetComputedStateAt(const RobotTimeStamp_t t_request,
                                                 HistRobotState ** state,
                                                 HistStateKey* key)
    {
      return GetComputedStateAt(t_request, const_cast<const HistRobotState **>(state), key);
    }
    
    Result RobotStateHistory::GetLatestVisionOnlyState(RobotTimeStamp_t& t, HistRobotState& state) const
    {
      if (!_visStates.empty()) {
        t = _visStates.rbegin()->first;
        state = _visStates.rbegin()->second;
        return RESULT_OK;
      }
      
      return RESULT_FAIL;
    }
    
    Result RobotStateHistory::GetLastStateWithFrameID(const PoseFrameID_t frameID, HistRobotState& state) const
    {
      // Start from end and work backward until we find a pose stamp with the
      // specified ID. Fail if we get back to the beginning without finding it.
      if (_states.empty()) {
        LOG_INFO("RobotStateHistory.GetLastStateWithFrameID.EmptyHistory",
                 "Looking for last pose with frame ID=%d, but pose history is empty.", frameID);
        return RESULT_FAIL;
      }
      
      // First look through "raw" poses for the frame ID. We don't need to look
      // any further once the frameID drops below the one we are looking for,
      // because they are ordered
      bool found = false;
      auto poseIter = _states.rend();
      for (poseIter = _states.rbegin();
          poseIter != _states.rend() && poseIter->second.GetFrameId() >= frameID; ++poseIter )
      {
        if (poseIter->second.GetFrameId() == frameID) {
          found = true; // break out of the loop without incrementing poseIter
          break;
        }
      }
      
      // NOTE: this second loop over vision poses will only occur if found is still false,
      // meaning we didn't find a pose already in the first loop.
      if (!found) {
        for (poseIter = _visStates.rbegin();
            poseIter != _visStates.rend() && poseIter->second.GetFrameId() >= frameID; ++poseIter)
        {
          if (poseIter->second.GetFrameId() == frameID) {
            found = true;
            break;
          }
        }
      }
      
      if (found) {
        // Success!
        DEV_ASSERT(poseIter != _states.rend(),
                   "RobotStateHistory.GetLastStateWithFrameID.InvalidIter");
        state = poseIter->second;
        return RESULT_OK;
        
      } else {
        LOG_INFO("RobotStateHistory.GetLastStateWithFrameID.FrameIdNotFound",
                 "Could not find frame ID=%d in pose history. "
                 "(First frameID in pose history is %d (t:%u), last is %d (t:%u). "
                 "First frameID in vis pose history is %d (t:%u), last is %d (t:%u).)",
                 frameID,
                 _states.begin()->second.GetFrameId(),
                 (TimeStamp_t)_states.begin()->first,
                 _states.rbegin()->second.GetFrameId(),
                 (TimeStamp_t)_states.rbegin()->first,
                 (_visStates.empty() ? -1 : _visStates.begin()->second.GetFrameId()),
                 (TimeStamp_t)(_visStates.empty() ? 0 : _visStates.begin()->first),
                 (_visStates.empty() ? -1 : _visStates.rbegin()->second.GetFrameId()),
                 (TimeStamp_t)(_visStates.empty() ? 0 : _visStates.rbegin()->first));
        return RESULT_FAIL;

      }
      
    } // GetLastStateWithFrameID()


    u32 RobotStateHistory::GetNumRawStatesWithFrameID(const PoseFrameID_t frameID) const
    {
      // First look through "raw" poses for the frame ID. We don't need to look
      // any further once the frameID drops below the one we are looking for,
      // because they are ordered
      u32 cnt = 0;
      for (auto poseIter = _states.rbegin(); poseIter != _states.rend(); ++poseIter )
      {
        auto currFrameId = poseIter->second.GetFrameId();
        if (currFrameId == frameID) {
          ++cnt;
        } else if (currFrameId < frameID) {
          break;
        }
      }
      return cnt;
    }
    
    void RobotStateHistory::CullToWindowSize()
    {
      if (_states.size() > 1) {
        
        // Get the most recent timestamp
        RobotTimeStamp_t mostRecentTime = _states.rbegin()->first;
        
        // If most recent time is less than window size, we're done.
        if (mostRecentTime < _windowSize_ms) {
          return;
        }
        
        // Get pointer to the oldest timestamp that may remain in the map
        RobotTimeStamp_t oldestAllowedTime = mostRecentTime - _windowSize_ms;
        const auto it = _states.lower_bound(oldestAllowedTime);
        const auto git = _visStates.lower_bound(oldestAllowedTime);
        const auto cit = _computedStates.lower_bound(oldestAllowedTime);
        const auto keyByTs_it = _keyByTsMap.lower_bound(oldestAllowedTime);
        
        // Delete everything before the oldest allowed timestamp
        if (!_states.empty() && it != _states.begin()) {
          _states.erase(_states.begin(), it);
          
          if (_states.empty())
          {
            LOG_DEBUG("RobotStateHistory.CullToWindowSize.StatesEmpty",
                      "_states is empty after culling to window size %u",
                      _windowSize_ms);
          }
        }
        if (!_visStates.empty() &&  git != _visStates.begin()) {
          _visStates.erase(_visStates.begin(), git);
          
          if (_visStates.empty())
          {
            LOG_DEBUG("RobotStateHistory.CullToWindowSize.VisStatesEmpty",
                      "_visStates is empty after culling to window size %u",
                      _windowSize_ms);
          }
        }
        if (!_computedStates.empty() && cit != _computedStates.begin()) {
          _computedStates.erase(_computedStates.begin(), cit);
        }

        if (!_keyByTsMap.empty() && keyByTs_it != _keyByTsMap.begin()) {
          if (keyByTs_it != _keyByTsMap.end()) {
            const auto tsByKey_it = _tsByKeyMap.find(keyByTs_it->second);
            if (tsByKey_it != _tsByKeyMap.end()) {
              _tsByKeyMap.erase(_tsByKeyMap.begin(), tsByKey_it);
            } else {
              LOG_ERROR("RobotStateHistory.CullToWindowSize.MapsOutOfSync",
                        "keyByTsMap size: %zu, tsByKeyMap size: %zu",
                        _keyByTsMap.size(), _tsByKeyMap.size());
            }
          }
          _keyByTsMap.erase(_keyByTsMap.begin(), keyByTs_it);
        }

      }
    }
    
    bool RobotStateHistory::IsValidKey(const HistStateKey key) const
    {
      return (_tsByKeyMap.find(key) != _tsByKeyMap.end());
    }
    
    RobotTimeStamp_t RobotStateHistory::GetOldestTimeStamp() const
    {
      return (_states.empty() ? 0 : _states.begin()->first);
    }
    
    RobotTimeStamp_t RobotStateHistory::GetNewestTimeStamp() const
    {
      return (_states.empty() ? 0 : _states.rbegin()->first);
    }

    RobotTimeStamp_t RobotStateHistory::GetOldestVisionOnlyTimeStamp() const
    {
      return (_visStates.empty() ? 0 : _visStates.begin()->first);
    }

    RobotTimeStamp_t RobotStateHistory::GetNewestVisionOnlyTimeStamp() const
    {
      return (_visStates.empty() ? 0 : _visStates.rbegin()->first);
    }

    void RobotStateHistory::Print() const
    {
      // Create merged map of all poses
      std::multimap<TimeStamp_t, std::pair<std::string, const_StateMapIter_t> > mergedPoses;
      std::multimap<TimeStamp_t, std::pair<std::string, const_StateMapIter_t> >::iterator mergedIt;
      const_StateMapIter_t pit;
      
      for (pit = _states.begin(); pit != _states.end(); ++pit) {
        mergedPoses.emplace(std::piecewise_construct,
                            std::forward_as_tuple(pit->first),
                            std::forward_as_tuple("  ", pit));
      }

      for (pit = _visStates.begin(); pit != _visStates.end(); ++pit) {
        mergedPoses.emplace(std::piecewise_construct,
                            std::forward_as_tuple(pit->first),
                            std::forward_as_tuple("v ", pit));
      }

      for (pit = _computedStates.begin(); pit != _computedStates.end(); ++pit) {
        mergedPoses.emplace(std::piecewise_construct,
                            std::forward_as_tuple(pit->first),
                            std::forward_as_tuple("c ", pit));
      }
      
      
      printf("\nRobotStateHistory\n");
      printf("================\n");
      for (mergedIt = mergedPoses.begin(); mergedIt != mergedPoses.end(); ++mergedIt) {
        printf("%s%d: ", mergedIt->second.first.c_str(), mergedIt->first);
        mergedIt->second.second->second.Print();
      }
    }
    
  } // namespace Vector
} // namespace Anki
