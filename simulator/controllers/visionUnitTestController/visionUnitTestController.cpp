/*
 * File:          visionUnitTestController.cpp
 * Author:        Andrew Stein
 * Date:          2/13/2014
 *
 * Description:   Webot controller to load vision test worlds and create JSON
 *                ground truth files for vision system unit tests.
 *
 * Modifications: 
 * 
 * Copyright 2014, Anki, Inc.
 */


#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <array>
#include <fstream>
#include <webots/Supervisor.hpp>
#include <webots/Camera.hpp>
#include <webots/PositionSensor.hpp>
#include <webots/Motor.hpp>

#include "json/json.h"

#include "anki/common/types.h"
#include "anki/common/basestation/objectTypesAndIDs.h"

#include "anki/common/basestation/jsonTools.h"
#include "anki/common/basestation/platformPathManager.h"

#include "anki/vision/basestation/camera.h"

#include "anki/cozmo/basestation/comms/robot/robotMessages.h"

#include "anki/cozmo/shared/cozmoConfig.h"

#include "anki/vision/robot/fiducialMarkers.h"

#define USE_MATLAB_DETECTION 1

#if USE_MATLAB_DETECTION
#include "anki/common/basestation/matlabInterface.h"
#else
#include "visionParameters.h"
#include "anki/vision/robot/fiducialDetection.h"
#endif // #if USE_MATLAB_DETECTION

using namespace Anki;

#ifdef SIMULATOR
Anki::Vision::CameraCalibration GetCameraCalibration(const webots::Camera* camera)
{
  const u16 nrows  = static_cast<u16>(camera->getHeight());
  const u16 ncols  = static_cast<u16>(camera->getWidth());
  
  const f32 width  = static_cast<f32>(ncols);
  const f32 height = static_cast<f32>(nrows);
  //f32 aspect = width/height;
  
  // See sim_hal::FillCameraInfo() for more info
  const f32 fov_hor = camera->getFov();
  const f32 f = width / (2.f * std::tan(0.5f*fov_hor));
  
  const f32 center_x = 0.5f*width;
  const f32 center_y = 0.5f*height;
  
  const f32 skew = 0.f;
  
  return Anki::Vision::CameraCalibration(nrows, ncols, f, f, center_x, center_y, skew);
}
#endif


/*
 * This is the main program.
 * The arguments of the main function can be specified by the
 * "controllerArgs" field of the Robot node
 */
int main(int argc, char **argv)
{
  const int TIME_STEP = 5;
  
  // TODO: Add specification of head angle too
  
  const int NUM_POSE_VALS = 8;
  
  if(argc < (NUM_POSE_VALS+1)) {
    fprintf(stderr, "Not enough controllerArgs to specify a single robot pose.\n");
    return -1;
  }
  else if( ((argc-1) % NUM_POSE_VALS) != 0 ) {
    fprintf(stderr, "Robot poses should be specified in groups of 8 values (Xaxis,Yaxis,Zaxis,Angle,Tx,Ty,Tz,HeadAngle).\n");
    return -1;
  }
  
  const int numPoses = (argc-1)/NUM_POSE_VALS;
  int rotIndex = 1;
  int transIndex = 5;
  int headAngleIndex = 8;
  
#if USE_MATLAB_DETECTION
  // Create a Matlab engine and initialize the path
  Matlab matlab(false);
  matlab.EvalStringEcho("run(fullfile('..', '..', '..', '..', 'matlab', 'initCozmoPath'));");
#else
  Anki::Vector::VisionSystem::DetectFiducialMarkersParameters detectionParams;
  detectionParams.Initialize();
#endif
  
  webots::Supervisor webotRobot_;
  
  // Motors
  webots::Motor* headMotor_  = webotRobot_.getMotor("HeadMotor");
  webots::Motor* liftMotor_  = webotRobot_.getMotor("LiftMotor");
  
  webots::PositionSensor* headPosSensor_ = webotRobot_.getPositionSensor("HeadPosSensor");
  webots::PositionSensor* liftPosSensor_ = webotRobot_.getPositionSensor("LiftPosSensor");
  
  // Enable position measurements on head and lift
  headPosSensor_->enable(TIME_STEP);
  liftPosSensor_->enable(TIME_STEP);
 
  // Lower the lift out of the way
  liftMotor_->setPosition(0.f);

  // Camera
  webots::Camera* headCam_ = webotRobot_.getCamera("HeadCamera");
  headCam_->enable(TIME_STEP);
  Vision::CameraCalibration calib = GetCameraCalibration(headCam_);
  Json::Value jsonCalib;
  calib.CreateJson(jsonCalib);
  
  // Grab the robot node and its rotation/translation fields so we can
  // manually move it around to the specified poses
  const std::string robotName(webotRobot_.getName());
  webots::Node* robotNode   = webotRobot_.getFromDef(robotName);
  if(robotNode == NULL) {
    fprintf(stderr, "Could not robot node with DEF '%s'.\n", robotName.c_str());
    return -1;
  }
  webots::Field* transField = robotNode->getField("translation");
  webots::Field* rotField   = robotNode->getField("rotation");
  
  webotRobot_.step(TIME_STEP);
  
  
  Json::Value root;
  
  // Store the ground truth objects poses and world name
  int numObjects = 0;
  webots::Node* rootNode = webotRobot_.getRoot();
  webots::Field* children = rootNode->getField("children");
  const int numNodes = children->getCount();
  for(int i_node=0; i_node<numNodes; ++i_node) {
    webots::Node* child = children->getMFNode(i_node);
    
    webots::Field* nameField = child->getField("name");
    if(nameField != NULL &&
       (nameField->getSFString().compare(0,5,"Block") == 0 ||
        nameField->getSFString().compare(0,4,"Ramp") == 0))
    {
      std::string objectType = child->getField("type")->getSFString();
      if(!objectType.empty())
      {
        Json::Value jsonObject;
        jsonObject["Type"] = objectType;
        
        jsonObject["ObjectName"] = child->getField("name")->getSFString();
        
        const double *objectTrans_m = child->getField("translation")->getSFVec3f();
        const double *objectRot     = child->getField("rotation")->getSFRotation();
        for(int i=0; i<3; ++i) {
          jsonObject["ObjectPose"]["Translation"].append(M_TO_MM(objectTrans_m[i]));
          jsonObject["ObjectPose"]["Axis"].append(objectRot[i]);
        }
        jsonObject["ObjectPose"]["Angle"] = objectRot[3];
        
        root["Objects"].append(jsonObject);
        numObjects++;
      }
      else {
        fprintf(stdout, "Skipping object with no type.\n");
      }
    } // if this is a block
    else if(child->getType() == webots::Node::WORLD_INFO) {
      root["WorldTitle"] = child->getField("title")->getSFString();
      
      std::string checkPoseStr = child->getField("info")->getMFString(0);
      if(checkPoseStr.back() == '0') {
        root["CheckRobotPose"] = false;
      }
      else if(checkPoseStr.back() == '1') {
        root["CheckRobotPose"] = true;
      }
      else {
        CORETECH_THROW("Unexpected character when looking for CheckRobotPose "
                       "setting in WorldInfo.\n");
      }
    }
    
  } // for each node
  root["NumObjects"] = numObjects;
  
  // Store the camera calibration
  root["CameraCalibration"] = jsonCalib;
  
  CORETECH_ASSERT(root.isMember("WorldTitle"));
  
  //std::string outputPath = PlatformPathManager::getInstance()->PrependPath(PlatformPathManager::Test, "basestation/test/blockWorldTests/") + root["WorldTitle"].asString();
  std::string outputPath("basestation/test/blockWorldTests/");
  outputPath += root["WorldTitle"].asString();
  
  for(int i_pose=0; i_pose<numPoses; ++i_pose,
      rotIndex+=NUM_POSE_VALS, transIndex+=NUM_POSE_VALS, headAngleIndex+=NUM_POSE_VALS)
  {
   
    // Move to next pose
    const double translation_m[3] = {
      atof(argv[transIndex]),
      atof(argv[transIndex+1]),
      atof(argv[transIndex+2])
    };
    
    const double rotation[4] = {
      atof(argv[rotIndex]),
      atof(argv[rotIndex+1]),
      atof(argv[rotIndex+2]),
      atof(argv[rotIndex+3])
    };

    const double headAngle = atof(argv[headAngleIndex]);
    headMotor_->setPosition(headAngle);
    
    rotField->setSFRotation(rotation);
    transField->setSFVec3f(translation_m);

    fprintf(stdout, "Moving robot '%s' to (%.3f,%.3f,%.3f), "
            "%.1fdeg @ (%.3f,%.3f,%.3f), with headAngle=%.1fdeg\n", robotName.c_str(),
            translation_m[0], translation_m[1], translation_m[2],
            RAD_TO_DEG(rotation[3]), rotation[0], rotation[1], rotation[2],
            RAD_TO_DEG(headAngle));
    
    // Step until the head and lift are in position
    const float TOL = DEG_TO_RAD(0.5f);
    Radians headErr, liftErr;
    do {
      webotRobot_.step(TIME_STEP);
      headErr  = fabs(headPosSensor_->getValue()  - headMotor_->getTargetPosition());
      liftErr  = fabs(liftPosSensor_->getValue()  - liftMotor_->getTargetPosition());
      //fprintf(stdout, "HeadErr = %.4f, LiftErr = %.4f\n", headErr.ToFloat(), liftErr.ToFloat());
    } while(headErr.getAbsoluteVal() > TOL || liftErr.getAbsoluteVal() > TOL);
    //fprintf(stdout, "Head and lift in position. Continuing.\n");
    
    Json::Value currentPose;

    // Store the image from the current position
    std::string imgFilename = outputPath + std::to_string(i_pose) + ".png";
    headCam_->saveImage(PlatformPathManager::GetInstance()->PrependPath(PlatformPathManager::Test, imgFilename), 100);
    
    // Store the associated image file
    currentPose["ImageFile"]  = imgFilename;
    
    // Store the ground truth robot pose
    for(int i=0; i<3; ++i) {
      currentPose["RobotPose"]["Translation"].append(M_TO_MM(translation_m[i]));
      currentPose["RobotPose"]["Axis"].append(rotation[i]);
    }
    currentPose["RobotPose"]["Angle"]     = rotation[3];
    currentPose["RobotPose"]["HeadAngle"] = headAngle;
    
    std::vector<Vector::MessageVisionMarker> markers;

#if USE_MATLAB_DETECTION
    // Process the image with Matlab to detect the vision markers
    matlab.EvalStringEcho("img = imread('%s'); "
                          "img = separable_filter(img, gaussian_kernel(0.5)); "
                          "imwrite(img, '%s'); "
                          "markers = simpleDetector(img); "
                          "numMarkers = length(markers);",
                          PlatformPathManager::GetInstance()->PrependPath(PlatformPathManager::Test, imgFilename).c_str(),
                          PlatformPathManager::GetInstance()->PrependPath(PlatformPathManager::Test, imgFilename).c_str());

    double *temp = matlab.Get<double>("numMarkers");
    const int numMarkers = static_cast<int>(*temp);
    free(temp);
    
    fprintf(stdout, "Detected %d markers at pose %d.\n", numMarkers, i_pose);
    
    for(int i_marker=0; i_marker<numMarkers; ++i_marker) {
      Vector::MessageVisionMarker msg;
      msg.timestamp = 0;
      
      matlab.EvalStringEcho("marker = markers{%d}; "
                            "corners = marker.corners; "
                            "code = marker.codeID; ", i_marker+1);

      const double* x_corners = mxGetPr(matlab.GetArray("corners"));
      const double* y_corners = x_corners + 4;
      
      // Sutract one for Matlab indexing!
      msg.x_imgUpperLeft  = x_corners[0]-1.f;
      msg.y_imgUpperLeft  = y_corners[0]-1.f;
      
      msg.x_imgLowerLeft  = x_corners[1]-1.f;
      msg.y_imgLowerLeft  = y_corners[1]-1.f;
      
      msg.x_imgUpperRight = x_corners[2]-1.f;
      msg.y_imgUpperRight = y_corners[2]-1.f;
      
      msg.x_imgLowerRight = x_corners[3]-1.f;
      msg.y_imgLowerRight = y_corners[3]-1.f;
      
      // Look up unoriented code
      using namespace Anki;
      Vision::MarkerType orientedMarkerCode = static_cast<Vision::MarkerType>(mxGetScalar(matlab.GetArray("code"))-1);
      
      msg.markerType = static_cast<u16>(Embedded::VisionMarker::RemoveOrientation(orientedMarkerCode));
      
      /*
      mxArray* mxByteArray = matlab.GetArray("byteArray");
      CORETECH_ASSERT(mxGetNumberOfElements(mxByteArray) == VISION_MARKER_CODE_LENGTH);
      const u8* code = reinterpret_cast<const u8*>(mxGetData(mxByteArray));
      std::copy(code, code + VISION_MARKER_CODE_LENGTH, msg.code.begin());
      */
      
      markers.emplace_back(msg);
      
    } // for each marker
#else
    
    {
      using namespace Anki::Embedded;

      AnkiAssert(detectionParams.isInitialized);
      
      const s32 maxMarkers = 100;
      FixedLengthList<VisionMarker> visionMarkers(maxMarkers, scratch);
      visionMarkers.get_maximumSize();
      
      FixedLengthList<Array<f32> > homographies(maxMarkers, scratch);
      
      visionMarkers.set_size(maxMarkers);
      homographies.set_size(maxMarkers);
      
      for(s32 i=0; i<maxMarkers; i++) {
        Array<f32> newArray(3, 3, scratch);
        homographies[i] = newArray;
      }
      
      const Result result = DetectFiducialMarkers(image,
                                                  markers,
                                                  homographies,
                                                  detectionParams.scaleImage_numPyramidLevels, parameters.scaleImage_thresholdMultiplier,
                                                  detectionParams.component1d_minComponentWidth, parameters.component1d_maxSkipDistance,
                                                  detectionParams.component_minimumNumPixels, parameters.component_maximumNumPixels,
                                                  detectionParams.component_sparseMultiplyThreshold, parameters.component_solidMultiplyThreshold,
                                                  detectionParams.component_minHollowRatio,
                                                  detectionParams.quads_minQuadArea, parameters.quads_quadSymmetryThreshold, parameters.quads_minDistanceFromImageEdge,
                                                  detectionParams.decode_minContrastRatio,
                                                  detectionParams.maxConnectedComponentSegments,
                                                  detectionParams.maxExtractedQuads,
                                                  detectionParams.quadRefinementIterations,
                                                  false,
                                                  ccmScratch, onchipScratch, offchipScratch);
      
      AnkiAssert(result == RESULT_OK);
    }
    
    const s32 numMarkers = VisionMemory::markers_.get_size();
    bool isTrackingMarkerFound = false;
    for(s32 i_marker = 0; i_marker < numMarkers; ++i_marker)
    {
      const VisionMarker& crntMarker = VisionMemory::markers_[i_marker];
      
      // Create a vision marker message and process it (which just queues it
      // in the mailbox to be picked up and sent out by main execution)
      {
        Messages::VisionMarker msg;
        msg.timestamp  = imageTimeStamp;
        msg.markerType = crntMarker.markerType;
        
        msg.x_imgLowerLeft = crntMarker.corners[Quadrilateral<f32>::BottomLeft].x;
        msg.y_imgLowerLeft = crntMarker.corners[Quadrilateral<f32>::BottomLeft].y;
        
        msg.x_imgUpperLeft = crntMarker.corners[Quadrilateral<f32>::TopLeft].x;
        msg.y_imgUpperLeft = crntMarker.corners[Quadrilateral<f32>::TopLeft].y;
        
        msg.x_imgUpperRight = crntMarker.corners[Quadrilateral<f32>::TopRight].x;
        msg.y_imgUpperRight = crntMarker.corners[Quadrilateral<f32>::TopRight].y;
        
        msg.x_imgLowerRight = crntMarker.corners[Quadrilateral<f32>::BottomRight].x;
        msg.y_imgLowerRight = crntMarker.corners[Quadrilateral<f32>::BottomRight].y;
        
        HAL::RadioSendMessage(GET_MESSAGE_ID(Messages::VisionMarker),&msg);
      }

    
#endif
    
    // Store the VisionMarkers
    currentPose["NumMarkers"] = numMarkers;
    for(auto & marker : markers) {
      Json::Value jsonMarker = marker.CreateJson();
      
      // Kludge to replace the marker type enum with its name instead of its
      // value.  This makes the file more human-readable and means we don't
      // have to recreate it every time the list of enums changes (e.g. after
      // retraining)
      {
        CORETECH_ASSERT(jsonMarker.isMember("markerType"));
        const Vision::MarkerType markerType = static_cast<Vision::MarkerType>(jsonMarker["markerType"].asInt());
        jsonMarker["markerType"] = Vision::MarkerTypeStrings[markerType];
      }
      
      fprintf(stdout, "Creating JSON for marker type %s with corners (%.1f,%.1f), (%.1f,%.1f), "
              "(%.1f,%.1f), (%.1f,%.1f)\n",
              jsonMarker["markerType"].asString().c_str(), //Vision::MarkerTypeStrings[marker.markerType],
              marker.x_imgUpperLeft,  marker.y_imgUpperLeft,
              marker.x_imgLowerLeft,  marker.y_imgLowerLeft,
              marker.x_imgUpperRight, marker.y_imgUpperRight,
              marker.x_imgLowerRight, marker.y_imgLowerRight);
      
      currentPose["VisionMarkers"].append(jsonMarker);
      
    } // for each marker
    
    root["Poses"].append(currentPose);
    
  } // for each pose
 
  // Actually write the Json to file
  
  std::string jsonFilename = PlatformPathManager::GetInstance()->PrependPath(PlatformPathManager::Test, outputPath) + ".json";
  std::ofstream jsonFile(jsonFilename, std::ofstream::out);
  
  fprintf(stdout, "Writing JSON to file %s.\n", jsonFilename.c_str());
  jsonFile << root.toStyledString();
  jsonFile.close();
  
  //webotRobot_.simulationQuit(RESULT_OK);
  
  return 0;
}
