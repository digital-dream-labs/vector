#include "simulator/game/cozmoSimTestController.h"
#include <string>

namespace Anki {
namespace Vector {

enum class TestState {
  Init,
  TapCube,
  CheckForTappedMessage,
  Wait1Sec,
  MoveCube,
  CheckForMovedMessage,
  CheckForStoppedMessage,
  CheckForUpAxisChangedMessage,
  Exit
};

class CST_MotionMessagesFromBlocks : public CozmoSimTestController
{
public:
  CST_MotionMessagesFromBlocks();

private:
  s32 UpdateSimInternal() override;

  void HandleActiveObjectTapped(const ExternalInterface::ObjectTapped& msg) override;
  void HandleActiveObjectStoppedMoving(const ExternalInterface::ObjectStoppedMoving& msg) override;
  void HandleActiveObjectMoved(const ExternalInterface::ObjectMoved& msg) override;
  void HandleActiveObjectUpAxisChanged(const ExternalInterface::ObjectUpAxisChanged& msg) override;
  void HandleActiveObjectConnectionState(const ExternalInterface::ObjectConnectionState& msg) override;

  TestState _testState = TestState::Init;
  const Pose3d _cubePose1 = {0, Vec3f(0.f, 0.f, 1.f), Vec3f(200.f, 50.f, 22.1f)};
  bool _wasTapped = false;
  bool _wasStopped = false;
  bool _wasMoved = false;
  UpAxis _lastReportedUpAxis = UpAxis::UnknownAxis;
  
  u32 _numObjectsConnected = 0;
  u32 _objId = 0;
};

REGISTER_COZMO_SIM_TEST_CLASS(CST_MotionMessagesFromBlocks);

CST_MotionMessagesFromBlocks::CST_MotionMessagesFromBlocks() {}

s32 CST_MotionMessagesFromBlocks::UpdateSimInternal()
{
  switch(_testState) {
    case TestState::Init:
    {
      // Request a cube connection
      SendConnectToCube();

      SET_TEST_STATE(TapCube);
      break;
    }

    case TestState::TapCube:
    {
      IF_CONDITION_WITH_TIMEOUT_ASSERT(_numObjectsConnected == 1, 15) {
        _wasTapped = false;
        _wasStopped = false;
        _wasMoved = false;
        UiGameController::SendApplyForce("cube", 0, 0, 6);
        SET_TEST_STATE(CheckForTappedMessage);
      }
      break;
    }

    case TestState::CheckForTappedMessage:
    {
      IF_CONDITION_WITH_TIMEOUT_ASSERT(_wasTapped, 5) {
        SET_TEST_STATE(Wait1Sec);
      }
      break;
    }

    case TestState::Wait1Sec:
    {
      // To prevent double tap detect (and therefore move suppression) with the next lifting of the cube
      if (HasXSecondsPassedYet(1.0)) {
        SET_TEST_STATE(MoveCube);
      }
      break;
    }

    case TestState::MoveCube:
    {
      _wasTapped = false;
      _wasStopped = false;
      _wasMoved = false;
      UiGameController::SendApplyForce("cube", 10, 0, 20);
      SET_TEST_STATE(CheckForMovedMessage);
      break;
    }

    case TestState::CheckForMovedMessage:
    {
      IF_CONDITION_WITH_TIMEOUT_ASSERT(_wasMoved, 5) {
        SET_TEST_STATE(CheckForStoppedMessage);
      }
      break;
    }

    case TestState::CheckForStoppedMessage:
    {
      IF_CONDITION_WITH_TIMEOUT_ASSERT(_wasStopped, 5) {
        // Rotate the block onto another side
        //  (without causing the 'moved' message)
        _lastReportedUpAxis = UpAxis::UnknownAxis;
        Pose3d p = GetLightCubePoseActual(ObjectType::Block_LIGHTCUBE1);
        p.SetRotation(Radians(DEG_TO_RAD(90.f)), Y_AXIS_3D());
        SetLightCubePose(ObjectType::Block_LIGHTCUBE1, p);
        SET_TEST_STATE(CheckForUpAxisChangedMessage);
      }
      break;
    }

    case TestState::CheckForUpAxisChangedMessage:
    {
      IF_CONDITION_WITH_TIMEOUT_ASSERT(_lastReportedUpAxis == UpAxis::XNegative, 5) {
        SET_TEST_STATE(Exit);
      }
      break;
    }
      
    case TestState::Exit:
    {
      CST_EXIT();
      break;
    }
  }

  return _result;
}

void CST_MotionMessagesFromBlocks::HandleActiveObjectTapped(const ExternalInterface::ObjectTapped& msg)
{
  _wasTapped = true;
}

void CST_MotionMessagesFromBlocks::HandleActiveObjectStoppedMoving(const ExternalInterface::ObjectStoppedMoving& msg)
{
  _wasStopped = true;
}

void CST_MotionMessagesFromBlocks::HandleActiveObjectMoved(const ExternalInterface::ObjectMoved& msg)
{
  _wasMoved = true;
}
  
void CST_MotionMessagesFromBlocks::HandleActiveObjectUpAxisChanged(const ExternalInterface::ObjectUpAxisChanged& msg)
{
  _lastReportedUpAxis = msg.upAxis;
}
  
void CST_MotionMessagesFromBlocks::HandleActiveObjectConnectionState(const ExternalInterface::ObjectConnectionState& msg)
{
  if (msg.connected) {
    ++_numObjectsConnected;
    _objId = msg.objectID;
  } else if (_numObjectsConnected > 0) {
    --_numObjectsConnected;
  }
}


}  // namespace Vector
}  // namespace Anki
