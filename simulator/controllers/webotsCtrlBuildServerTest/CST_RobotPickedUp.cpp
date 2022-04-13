#include "simulator/game/cozmoSimTestController.h"

namespace Anki {
namespace Vector {
enum class TestState {
  Init,
  PickupRobot,
  VerifyRobotPickedUp,
  Exit
};

class CST_RobotPickedUp : public CozmoSimTestController
{
public:
  CST_RobotPickedUp();

private:
  s32 UpdateSimInternal() override;

  TestState _testState = TestState::Init;
  s32 _result = 0;

  bool _robotWasPickedUp = false;

  void HandleRobotOffTreadsStateChanged(ExternalInterface::RobotOffTreadsStateChanged const& msg) override;
};

REGISTER_COZMO_SIM_TEST_CLASS(CST_RobotPickedUp);

CST_RobotPickedUp::CST_RobotPickedUp() {}

s32 CST_RobotPickedUp::UpdateSimInternal()
{
  switch (_testState) {
    case TestState::Init:
    {
      SET_TEST_STATE(PickupRobot);
      break;
    }

    case TestState::PickupRobot:
    {
      // Apply a arbitrary z force of to trigger the RobotPickedUp event.
      SendApplyForce("cozmo", 0, 0, 100);
      SET_TEST_STATE(VerifyRobotPickedUp);
      break;
    }

    case TestState::VerifyRobotPickedUp:
    {
      IF_CONDITION_WITH_TIMEOUT_ASSERT(_robotWasPickedUp, 5)
      {
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

void CST_RobotPickedUp::HandleRobotOffTreadsStateChanged(ExternalInterface::RobotOffTreadsStateChanged const& msg)
{
  if(msg.treadsState != OffTreadsState::OnTreads){
    _robotWasPickedUp = true;
  }
}

}  // namespace Vector
}  // namespace Anki
