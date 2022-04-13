#include <webots/Node.hpp>
#include <webots/Supervisor.hpp>
#include "simulator/game/cozmoSimTestController.h"
#include "coretech/messaging/shared/UdpServer.h"
#include "clad/types/vizTypes.h"
#include "anki/cozmo/shared/cozmoEngineConfig.h"

namespace Anki {
namespace Vector {

enum class TestState {
  RequestCubeConnection,          // Request to be connected to a cube
  WaitingForObjectAvailable,      // We should receive ObjectAvailable messages from all three cubes
  WaitingForConnectToCubeA,       // We should connect to the closest cube, which is cube A
  WaitingForDisconnectFromCubeA,
  WaitingForConnectToCubeA_again, // We should connect to cube A again, since we have not reset the preferred cube
  WaitingForDisconnectFromCubeA_again,
  WaitingForConnectToCubeB,       // We should connect to the closest cube, which is now cube B
  WaitingForDisconnectFromCubeB,
  WaitingForConnectToCubeC,       // We should connect to the preferred cube, which we have set as cube C
  WaitingForUnexpectedDisconnect, // After zapping cube C from the world, we should get a disconnection message
  Exit
};

const std::string kCubeA = "aa:aa:aa:aa:aa:aa";
const std::string kCubeB = "bb:bb:bb:bb:bb:bb";
const std::string kCubeC = "cc:cc:cc:cc:cc:cc";
  
class CST_CubeConnection : public CozmoSimTestController
{
public:
  CST_CubeConnection();

private:

  s32 UpdateSimInternal() override;
  
  void HandleActiveObjectAvailable(const ExternalInterface::ObjectAvailable& msg) override;
  
  void HandleActiveObjectConnectionState(const ExternalInterface::ObjectConnectionState& msg) override;

  TestState _testState = TestState::RequestCubeConnection;
  
  struct CubeInfo {
    ObjectID objectId;
    bool connected = false;
    int objectAvailableCnt = 0;
    webots::Node* node;
  };
  
  // map of factory ID to CubeInfo
  std::map<std::string, CubeInfo> _cubes;

  // Returns the factory ID of the connected cube or
  // empty string if there is none.
  std::string GetConnectedCube();
  
  bool IsAnyCubeConnected();
};

REGISTER_COZMO_SIM_TEST_CLASS(CST_CubeConnection);

CST_CubeConnection::CST_CubeConnection()
{
  _cubes[kCubeA].node = GetNodeByDefName("CubeA");
  _cubes[kCubeB].node = GetNodeByDefName("CubeB");
  _cubes[kCubeC].node = GetNodeByDefName("CubeC");
}

s32 CST_CubeConnection::UpdateSimInternal()
{
  switch (_testState) {
    case TestState::RequestCubeConnection:
    {
      CST_ASSERT(!IsAnyCubeConnected(), "Should not be connected to any cubes initially");
      
      // Tell engine to forward ObjectAvailable messages to us
      SendBroadcastObjectAvailable(true);
      
      // Tell engine to forget its preferred cube and request a cube connection
      SendForgetPreferredCube();
      SendConnectToCube();
      SET_TEST_STATE(WaitingForObjectAvailable);
      break;
    }
    
    case TestState::WaitingForObjectAvailable:
    {
      IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(5,
                                            _cubes[kCubeA].objectAvailableCnt > 0,
                                            _cubes[kCubeB].objectAvailableCnt > 0,
                                            _cubes[kCubeC].objectAvailableCnt > 0) {
        SET_TEST_STATE(WaitingForConnectToCubeA);
      }
      break;
    }
      
    case TestState::WaitingForConnectToCubeA:
    {
      // We should be connected to the closest cube, which is cube A.
      IF_ALL_CONDITIONS_WITH_TIMEOUT_ASSERT(5,
                                            IsAnyCubeConnected(),
                                            GetConnectedCube() == kCubeA) {
        // Move cube A far from the robot so that cube B is now the closest.
        auto cubePose = GetPose3dOfNode(_cubes[kCubeA].node);
        auto translation = cubePose.GetTranslation();
        translation.x() += 1000.f;
        cubePose.SetTranslation(translation);
        SetNodePose(_cubes[kCubeA].node, cubePose);
        
        // Disconnect from cube. Next connection attempt should
        // connect to cube A again (since it is the preferred cube).
        SendDisconnectFromCube(0.f);
        
        SET_TEST_STATE(WaitingForDisconnectFromCubeA);
      }
      break;
    }
      
    case TestState::WaitingForDisconnectFromCubeA:
    {
      IF_CONDITION_WITH_TIMEOUT_ASSERT(!IsAnyCubeConnected(), 5) {
        SendConnectToCube();
        SET_TEST_STATE(WaitingForConnectToCubeA_again);
      }
      break;
    }
    
    case TestState::WaitingForConnectToCubeA_again:
    {
      // We should connect to cube A again (even though it's not the closest cube)
      // since we did _not_ reset the preferred cube.
      IF_CONDITION_WITH_TIMEOUT_ASSERT(GetConnectedCube() == kCubeA, 5) {
        // Now disconnect and forget the preferred cube. Next connection attempt
        // should connect to cube B (since it is now the closest cube).
        SendForgetPreferredCube();
        SendDisconnectFromCube(0.f);
        
        SET_TEST_STATE(WaitingForDisconnectFromCubeA_again);
      }
      break;
    }
      
    case TestState::WaitingForDisconnectFromCubeA_again:
    {
      IF_CONDITION_WITH_TIMEOUT_ASSERT(!IsAnyCubeConnected(), 5) {
        SendConnectToCube();
        SET_TEST_STATE(WaitingForConnectToCubeB);
      }
      break;
    }
      
    case TestState::WaitingForConnectToCubeB:
    {
      // We should be connected to the closest cube, which is now cube B.
      IF_CONDITION_WITH_TIMEOUT_ASSERT(GetConnectedCube() == kCubeB, 5) {
        SendDisconnectFromCube(0.f);
        // Set cube C as the preferred cube. We should now
        // connect to cube C even though it is not the closest.
        SendSetPreferredCube(kCubeC);
        SET_TEST_STATE(WaitingForDisconnectFromCubeB);
      }
      break;
    }
      
    case TestState::WaitingForDisconnectFromCubeB:
    {
      IF_CONDITION_WITH_TIMEOUT_ASSERT(!IsAnyCubeConnected(), 5) {
        SendConnectToCube();
        SET_TEST_STATE(WaitingForConnectToCubeC);
      }
      break;
    }
      
    case TestState::WaitingForConnectToCubeC:
    {
      // We should be connected to the perferred cube, which is now cube C.
      IF_CONDITION_WITH_TIMEOUT_ASSERT(GetConnectedCube() == kCubeC, 5) {
        // Remove cube C from the world, which should trigger a disconnection message
        _cubes[kCubeC].node->remove();
        SET_TEST_STATE(WaitingForUnexpectedDisconnect);
      }
      break;
    }
    
    case TestState::WaitingForUnexpectedDisconnect:
    {
      IF_CONDITION_WITH_TIMEOUT_ASSERT(!IsAnyCubeConnected(), 5) {
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

void CST_CubeConnection::HandleActiveObjectAvailable(const ExternalInterface::ObjectAvailable &msg)
{
  auto cube = _cubes.find(msg.factory_id);
  if (cube != _cubes.end()) {
    ++(cube->second.objectAvailableCnt);
  }
}
  
void CST_CubeConnection::HandleActiveObjectConnectionState(const ExternalInterface::ObjectConnectionState& msg)
{
  CST_ASSERT(_cubes.find(msg.factoryID) != _cubes.end(), "Received ObjectConnectionState from unknown cube");
  auto& cube = _cubes[msg.factoryID];
 
  cube.objectId = msg.objectID;
  cube.connected = msg.connected;
}

std::string CST_CubeConnection::GetConnectedCube()
{
  std::string connectedId;
  for (const auto& mapEntry : _cubes) {
    const auto& id = mapEntry.first;
    const auto& cube = mapEntry.second;
    if (cube.connected) {
      CST_ASSERT(connectedId.empty(), "Should only have one connected cube!");
      connectedId = id;
    }
  }
  return connectedId;
}

bool CST_CubeConnection::IsAnyCubeConnected()
{
  return !GetConnectedCube().empty();
}

}  // namespace Vector
}  // namespace Anki
