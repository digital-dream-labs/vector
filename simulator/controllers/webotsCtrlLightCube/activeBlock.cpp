/*
 * File:          activeBlock.cpp
 * Date:
 * Description:   Main controller for simulated blocks
 * Author:        
 * Modifications: 
 */

#include "activeBlock.h"
#include "clad/types/ledTypes.h"
#include "clad/externalInterface/messageCubeToEngine.h"
#include "clad/externalInterface/messageEngineToCube.h"
#include "anki/cozmo/shared/cozmoConfig.h"
#include "util/logging/logging.h"
#include "util/helpers/templateHelpers.h"
#include "util/math/math.h"
#include "util/math/numericCast.h"
#include "util/random/randomGenerator.h"

#include "robot/cube_firmware/app/animation.h"

#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <string.h>

#include <webots/Supervisor.hpp>
#include <webots/Receiver.hpp>
#include <webots/Emitter.hpp>
#include <webots/Accelerometer.hpp>
#include <webots/LED.hpp>

#include <array>
#include <iomanip>
#include <map>

webots::Supervisor active_object_controller;

// animation.c externs this, and populates it with the LED color information
uint8_t intensity[ANIMATION_CHANNELS * COLOR_CHANNELS];

namespace Anki {
namespace Vector {
namespace ActiveBlock {

namespace {

  // Length of time in between transmission of ObjectAvailable messages
  const u32 kObjectAvailableMessagePeriod_ms = 1000;
  const u32 kObjectAvailableMessagePeriod_cycles = kObjectAvailableMessagePeriod_ms / CUBE_TIME_STEP_MS;
  
  // Length of time in between transmission of battery voltage messages
  const u32 kBatteryVoltageMessagePeriod_ms = 1000;
  const u32 kBatteryVoltageMessagePeriod_cycles = kBatteryVoltageMessagePeriod_ms / CUBE_TIME_STEP_MS;
  
  // To convert between battery voltage and the cube firmware's raw ADC counts (used to simulate how the physical cube
  // sends battery voltage to engine). The raw ADC value follows the equation: actualVolts = railVoltageCnts * 3.6 / 1024
  const float kBatteryVoltsToRawCnts = 1024.f / 3.6f;
  
  constexpr int kNumCubeLeds = Util::EnumToUnderlying(CubeConstants::NUM_CUBE_LEDS);
  
  webots::Receiver* receiver_;
  webots::Emitter*  emitter_;
  webots::Emitter*  discoveryEmitter_;
  
  // Webots comm channel used for the discovery emitter/receiver
  constexpr int kDiscoveryChannel = 0;
  
  webots::Accelerometer* accel_;
  
  // The cube accelerometer/tap message to be sent to engine
  CubeAccelData cubeAccelMsg_;
  
  // Raw accelerometer readings are buffered before being sent
  // to engine (due to BLE message rate limits on the cubes),
  // so keep track of which index we're on.
  size_t rawCubeAccelInd = 0;
  
  // Accelerometer filter window
  const u32 MAX_ACCEL_BUFFER_SIZE = 30;
  f32 accelBuffer_[3][MAX_ACCEL_BUFFER_SIZE];
  u32 accelBufferStartIdx_ = 0;
  u32 accelBufferSize_ = 0;
  
  // High-pass filter params for tap detection
  const f32 TAP_DETECT_THRESH = 9;
  const u32 TAP_DETECT_WINDOW_MS = 100;
  const f32 CUTOFF_FREQ = 50;
  const f32 RC = 1.0f/(CUTOFF_FREQ*2*3.14f);
  const f32 dt = 0.001f*CUBE_TIME_STEP_MS;
  const f32 alpha = RC/(RC + dt);
  
  // Pointers to the LED object to set the simulated cube's lights
  webots::LED* led_[kNumCubeLeds];
  
  // Pointer to the webots MFVec3f field which mirrors the current LED
  // colors so that the webots tests can monitor the current color.
  webots::Field* ledColorField_ = nullptr;
  // Updates to this field must be cached and only executed once per
  // simulation timestep due to a Webots R2018a bug (see COZMO-16021).
  // Store them here (key is LED index, value is RGB color).
  std::map<u32, std::array<double, 3>> pendingLedColors_;
  
  std::string factoryID_;
  
  ObjectType objectType_ = ObjectType::UnknownObject;  
  
  Util::RandomGenerator randGen_;
  
  // Pointer to webots field which contains the current battery voltage of the cube (this is to be able to simulate a
  // low cube battery condition)
  webots::Field* batteryVoltsField_ = nullptr;
  
} // private namespace

  
template <typename T>
void SendMessageHelper(webots::Emitter* emitter, T&& msg)
{
  DEV_ASSERT(emitter != nullptr, "ActiveBlock.SendMessageHelper.NullEmitter");
  
  // Construct a MessageCubeToEngine union from the passed-in msg (uses MessageCubeToEngine specialized rvalue constructors)
  MessageCubeToEngine cubeMessage(std::move(msg));
  
  // Stuff this message into a buffer and send it
  u8 buff[cubeMessage.Size()];
  cubeMessage.Pack(buff, cubeMessage.Size());
  emitter->send(buff, (int) cubeMessage.Size());
}
  
// takes a 24bit RGB color
inline void SetLED_helper(u32 index, u32 rgbColor) {
  DEV_ASSERT((rgbColor & 0xFF000000) == 0, "ActiveBlock.SetLedHelper.InvalidRgbColor");
  
  led_[index]->set(rgbColor);
  
  double red =   Util::numeric_cast<double>((rgbColor >> 16) & 0xFF);
  double green = Util::numeric_cast<double>((rgbColor >> 8) & 0xFF);
  double blue =  Util::numeric_cast<double>(rgbColor & 0xFF);

  // Store the RGB value, then only send it to Webots once per time step (in Update())
  pendingLedColors_[index] = {{red, green, blue}};
}


void ProcessMessage(const MessageEngineToCube& msg)
{
  const auto tag = msg.GetTag();
  switch (tag) {
    case MessageEngineToCubeTag::lightSequence:
    {
      MapCommand mapCommand;
      u8* dest = (u8*) (&mapCommand);
      msg.Pack(dest, msg.Size());
      animation_index(&mapCommand);
    }
    break;
    case MessageEngineToCubeTag::lightKeyframes:
    {
      FrameCommand frameCommand;
      u8* dest = (u8*) (&frameCommand);
      msg.Pack(dest, msg.Size());
      animation_frames(&frameCommand);
    }
    break;
    default:
      PRINT_NAMED_ERROR("ActiveBlock.ProcessMessage.UnexpectedTag",
                        "Received message with unexpected tag %s",
                        MessageEngineToCubeTagToString(tag));
      break;
  }
}


Result Init()
{
  animation_init();

  active_object_controller.step(CUBE_TIME_STEP_MS);
  
  webots::Node* selfNode = active_object_controller.getSelf();
  
  // Get this block's object type
  webots::Field* typeField = selfNode->getField("objectType");
  if (nullptr == typeField) {
    PRINT_NAMED_ERROR("ActiveBlock.Init.NoObjectType", "Failed to find lightCubeType");
    return RESULT_FAIL;
  }
  
  // Grab ObjectType and its integer value
  const auto& typeString = typeField->getSFString();
  objectType_ = ObjectTypeFromString(typeString);
  
  DEV_ASSERT_MSG(objectType_ == ObjectType::Block_LIGHTCUBE1,
                 "ActiveBlock.Init.InvalidLightCubeType",
                 "Invalid object type \"%s\". Only Block_LIGHTCUBE1 should be an active "
                 "object. All other object types should not be active blocks.",
                 typeString.c_str());

  // Generate a factory ID
  // If PROTO factoryID is nonempty, use that.
  // Otherwise randomly generate a factoryID.
  webots::Field* factoryIdField = selfNode->getField("factoryID");
  if (factoryIdField) {
    factoryID_ = factoryIdField->getSFString();
  }
  if (factoryID_.empty()) {
    // factoryID is still empty - generate a unique one.
    std::ostringstream ss;
    for (int i=0 ; i < 6 ; i++) {
      const int rand = randGen_.RandIntInRange(0, std::numeric_limits<uint8_t>::max());
      ss << std::setw(2) << std::setfill('0') << std::hex << (int) rand << ":";
    }
    factoryID_ = ss.str();
    factoryID_.pop_back(); // pop off the trailing ":"
  }
  PRINT_NAMED_INFO("ActiveBlock", "Starting active object (factoryID %s)", factoryID_.c_str());
  
  // Get all LED handles
  for (int i=0; i<kNumCubeLeds; ++i) {
    char led_name[32];
    sprintf(led_name, "led%d", i);
    led_[i] = active_object_controller.getLED(led_name);
    assert(led_[i] != nullptr);
  }
  
  // Field for monitoring color from webots tests
  ledColorField_ = selfNode->getField("ledColors");
  assert(ledColorField_ != nullptr);
  
  // Field for battery voltage
  batteryVoltsField_ = selfNode->getField("batteryVolts");
  assert(batteryVoltsField_ != nullptr);
  
  // Get radio emitter
  emitter_ = active_object_controller.getEmitter("emitter");
  assert(emitter_ != nullptr);
  // Generate a unique channel for this cube using factory ID
  const int emitterChannel = (int) (std::hash<std::string>{}(factoryID_) & 0x3FFFFFFF);
  emitter_->setChannel(emitterChannel);
  
  // Get radio receiver (channel = 1 + emitterChannel)
  receiver_ = active_object_controller.getReceiver("receiver");
  assert(receiver_ != nullptr);
  receiver_->setChannel(emitterChannel + 1);
  receiver_->enable(CUBE_TIME_STEP_MS);

  // Get radio emitter for discovery
  discoveryEmitter_ = active_object_controller.getEmitter("discoveryEmitter");
  assert(discoveryEmitter_ != nullptr);
  discoveryEmitter_->setChannel(kDiscoveryChannel);
  
  // Get accelerometer
  accel_ = active_object_controller.getAccelerometer("accel");
  assert(accel_ != nullptr);
  accel_->enable(CUBE_TIME_STEP_MS);
  
  return RESULT_OK;
}

void DeInit() {
  if (receiver_) {
    receiver_->disable();
  }
  if (accel_) {
    accel_->disable();
  }
}


// Returns true if a tap was detected
// Given the unfiltered accel values of the current frame, these values are
// accumulated into a buffer. Once the buffer has filled, apply high-pass
// filter to each axis' buffer. If there is a value that exceeds a threshold
// a tap is considered to have been detected.
// When a tap is detected
bool CheckForTap(f32 accelX, f32 accelY, f32 accelZ)
{
  bool tapDetected = false;
  
  // Add accel values to buffer
  u32 newIdx = accelBufferStartIdx_;
  if (accelBufferSize_ < MAX_ACCEL_BUFFER_SIZE-1) {
    newIdx += accelBufferSize_;
    ++accelBufferSize_;
  } else {
    accelBufferStartIdx_ = (accelBufferStartIdx_+1) % MAX_ACCEL_BUFFER_SIZE;
    accelBufferSize_ = MAX_ACCEL_BUFFER_SIZE;
  }
  newIdx = newIdx % MAX_ACCEL_BUFFER_SIZE;
  
  accelBuffer_[0][newIdx] = accelX;
  accelBuffer_[1][newIdx] = accelY;
  accelBuffer_[2][newIdx] = accelZ;
  
  if (accelBufferSize_ == MAX_ACCEL_BUFFER_SIZE) {
    
    // Compute high-pass filtered values
    for (u8 axis = 0; axis < 3 && !tapDetected; ++axis) {
      
      f32 prevAccelVal = accelBuffer_[axis][accelBufferStartIdx_];
      f32 prevAccelFiltVal = prevAccelVal;
      for (u32 i=1; i<MAX_ACCEL_BUFFER_SIZE; ++i) {
        u32 idx = (accelBufferStartIdx_ + i) % MAX_ACCEL_BUFFER_SIZE;
        f32 currAccelFiltVal = alpha * (prevAccelFiltVal + accelBuffer_[axis][idx] - prevAccelVal);
        prevAccelVal = accelBuffer_[axis][idx];
        prevAccelFiltVal = currAccelFiltVal;
        
        if (currAccelFiltVal > TAP_DETECT_THRESH) {
          PRINT_NAMED_INFO("ActiveBlock", "TapDetected: axis %d, val %f", axis, currAccelFiltVal);
          tapDetected = true;
          
          // Fast forward in buffer so that we don't allow tap detection again until
          // TAP_DETECT_WINDOW_MS later.
          u32 idxOffset = i + (TAP_DETECT_WINDOW_MS / CUBE_TIME_STEP_MS);
          accelBufferStartIdx_ = (accelBufferStartIdx_ + idxOffset) % MAX_ACCEL_BUFFER_SIZE;
          if (accelBufferSize_ > idxOffset) {
            accelBufferSize_ -= idxOffset;
          } else {
            accelBufferSize_ = 0;
          }
          
          break;
        }
      }
    }
  }
  
  return tapDetected;
}


Result Update()
{
  if (active_object_controller.step(CUBE_TIME_STEP_MS) != -1) {
    
    // Read incoming messages
    while (receiver_->getQueueLength() > 0) {
      int dataSize = receiver_->getDataSize();
      u8* data = (u8*)receiver_->getData();
      MessageEngineToCube msg(data, (size_t) dataSize);
      ProcessMessage(msg);
      receiver_->nextPacket();
    }
    
    // Send ObjectAvailable message if it's time.
    // Start the counter at a random number, or else all cubes
    // will send advertisement messages at the same time.
    static u32 objAvailableSendCtr = (u32) randGen_.RandIntInRange(0, kObjectAvailableMessagePeriod_cycles);
    if (objAvailableSendCtr-- == 0) {
      SendMessageHelper(discoveryEmitter_,
                        ExternalInterface::ObjectAvailable(factoryID_,
                                                           objectType_,
                                                           0));
      objAvailableSendCtr = kObjectAvailableMessagePeriod_cycles;
    }
    
    // Send BatteryVoltage message if it's time
    static u32 batteryVoltageSendCtr = kBatteryVoltageMessagePeriod_cycles;
    if (batteryVoltageSendCtr-- == 0) {
      const auto batteryVolts = batteryVoltsField_->getSFFloat();
      CubeVoltageData msg;
      msg.railVoltageCnts = static_cast<decltype(msg.railVoltageCnts)>(batteryVolts * kBatteryVoltsToRawCnts);
      SendMessageHelper(emitter_, msg);
      batteryVoltageSendCtr = kBatteryVoltageMessagePeriod_cycles;
    }
    
    // Update cube LED animations
    animation_tick();
    
    // Extract the RGB color for each LED from the
    // intensity variable
    for (int i=0 ; i < kNumCubeLeds ; i++) {
      u32 color = 0;
      color |= intensity[3*i + 0] << 16;
      color |= intensity[3*i + 1] << 8;
      color |= intensity[3*i + 2];
      
      SetLED_helper(i, color);
    }
    
    // Get accel values (units of m/sec^2)
    const double* accelVals = accel_->getValues();
    
    // Tap count just increments if a tap was detected (this emulates
    // the behavior of the actual cube firmware)
    if (CheckForTap(accelVals[0], accelVals[1], accelVals[2])) {
      ++cubeAccelMsg_.tap_count;
    }
    
    // For cube accel message, cube firmware buffers ACCEL_FRAMES_PER_MSG of them before
    // sending (due to BLE message rate limits)
    auto& thisRawAccel = cubeAccelMsg_.accelReadings[rawCubeAccelInd];
    for (int i = 0 ; i < 3 ; i++) {
      // Webots accelerometer returns values in m/sec^2
      // Convert from this to what the actual cube would report.
      // Actual cubes send 16 bit signed accelerations, with a
      // range of -4g to 4g.
      const double accel_g = accelVals[i] / 9.81;
      const double scaledAccelValue = accel_g * std::numeric_limits<int16_t>::max() / 4.0;
      thisRawAccel.accel[i] = Util::numeric_cast_clamped<int16_t>(scaledAccelValue);
    }
    
    // Send the cube accel message if it's time
    if (++rawCubeAccelInd >= ACCEL_FRAMES_PER_MSG) {
      SendMessageHelper(emitter_, CubeAccelData(cubeAccelMsg_));
      rawCubeAccelInd = 0;
    }

    // Set any pending LED color fields. This must be done here since setMFVec3f can only
    // be called once per simulation time step for a given field (known Webots R2018a bug)
    // TODO: Remove?
    if (!pendingLedColors_.empty()) {
      for (const auto& newColorEntry : pendingLedColors_) {
        const auto index = newColorEntry.first;
        const auto& color = newColorEntry.second;
        ledColorField_->setMFVec3f(index, color.data());
      }
      pendingLedColors_.clear();
    }

    return RESULT_OK;
  }
  return RESULT_FAIL;
}


}  // namespace ActiveBlock
}  // namespace Vector
}  // namespace Anki
