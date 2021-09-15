#include <string>
#include "physicsController.h"
#include "clad/vizInterface/messageViz.h"


namespace Anki {
namespace Vector {

void PhysicsController::Init()
{
  _eventManager.UnsubscribeAll();

  Subscribe(PhysicsInterface::MessageSimPhysicsTag::ApplyForce,
    std::bind(&PhysicsController::ProcessApplyForceMessage, this, std::placeholders::_1));

  _server.StopListening();
  _server.StartListening((uint16_t)VizConstants::WEBOTS_PHYSICS_CONTROLLER_PORT);
}

void PhysicsController::Update()
{
  // Arbitrary maxPacketSize 3000
  const size_t maxPacketSize {3000};
  uint8_t data[maxPacketSize] {0};
  ssize_t numBytesRecvd;
  while ((numBytesRecvd = _server.Recv((char*)data, maxPacketSize)) > 0) {
    ProcessMessage(PhysicsInterface::MessageSimPhysics(data, (size_t)numBytesRecvd));
  }
}

void PhysicsController::Cleanup()
{
  _server.StopListening();
}

void PhysicsController::ProcessMessage(PhysicsInterface::MessageSimPhysics&& message)
{
  dWebotsConsolePrintf("Processing msgs from game controller: Got msg %s\n",
                       PhysicsInterface::MessageSimPhysicsTagToString(message.GetTag()));

  uint32_t type = static_cast<uint32_t>(message.GetTag());
  _eventManager.Broadcast(AnkiEvent<PhysicsInterface::MessageSimPhysics>(type, std::move(message)));
}

void PhysicsController::ProcessApplyForceMessage(const AnkiEvent<PhysicsInterface::MessageSimPhysics>& msg)
{
  const auto& payload = msg.GetData().Get_ApplyForce();
  dWebotsConsolePrintf("x: %f", payload.xForce);
  dWebotsConsolePrintf("y: %f", payload.yForce);
  dWebotsConsolePrintf("z: %f", payload.zForce);
  dBodyEnable(GetdBodyID(payload.DefName));
  dBodyAddForce(GetdBodyID(payload.DefName), payload.xForce, payload.yForce, payload.zForce);
}

void PhysicsController::SetLinearVelocity(const std::string objectName,
                                          const Point<3, float> velVector)
{
  dBodyID body = GetdBodyID(objectName);
  if (body == NULL) { return; }
  dBodySetLinearVel(body, velVector[0], velVector[1], velVector[2]);
}

const dBodyID PhysicsController::GetdBodyID(const std::string& objectName)
{
  if (_dBodyIDMap.find(objectName) == _dBodyIDMap.end()) {
    dWebotsConsolePrintf("Need to lookup the dBodyID for the first time");
    dBodyID body = dWebotsGetBodyFromDEF(objectName.c_str());
    if (body == NULL) {
      dWebotsConsolePrintf(
        "ERROR: ODE couldn't find the body with name `%s`. There is no Solid (or derived) node with"
        "the specified DEF name or the physics field of the Solid node is undefined",
        objectName.c_str());

      return NULL;
    }
    _dBodyIDMap[objectName] = body;
  }

  return _dBodyIDMap[objectName];
}

}  // namespace Vector
}  // namespace Anki
