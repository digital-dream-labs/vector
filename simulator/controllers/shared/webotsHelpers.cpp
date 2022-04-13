/**
 * File: webotsHelpers.cpp
 *
 * Author: Matt Michini
 * Date:  01/22/2018
 *
 * Description: A few helper functions for common webots node queries
 *
 * Copyright: Anki, Inc. 2018
**/

#include "simulator/controllers/shared/webotsHelpers.h"

#include "coretech/common/engine/math/pose.h"
#include "util/logging/logging.h"

#include <webots/Supervisor.hpp>
#include <webots/Field.hpp>

namespace Anki {
namespace WebotsHelpers {

std::vector<RootNodeInfo> GetAllSceneTreeNodes(const webots::Supervisor& supervisor)
{
  const auto* rootNode = supervisor.getRoot();
  DEV_ASSERT(rootNode != nullptr, "WebotsHelpers.GetAllSceneTreeNodes.NullSupervisorRoot");
  
  const auto* rootChildren = rootNode->getField("children");
  DEV_ASSERT(rootChildren != nullptr, "WebotsHelpers.GetAllSceneTreeNodes.NullRootChildren");
  
  std::vector<RootNodeInfo> sceneTreeNodes;
  
  int numRootChildren = rootChildren->getCount();
  for (int n = 0 ; n < numRootChildren; ++n) {
    webots::Node* nd = rootChildren->getMFNode(n);
    DEV_ASSERT(nd != nullptr, "WebotsHelpers.GetAllSceneTreeNodes.NullNode");
    
    sceneTreeNodes.emplace_back(nd, nd->getType(), nd->getTypeName());
  }
  return sceneTreeNodes;
}


RootNodeInfo GetFirstMatchingSceneTreeNode(const webots::Supervisor& supervisor, const std::string& typeNameToMatch)
{
  RootNodeInfo foundNode;
  for (const auto& node : GetAllSceneTreeNodes(supervisor)) {
    if (node.typeName.find(typeNameToMatch) != std::string::npos) {
      foundNode = node;
      break;
    }
  }
  return foundNode;
}


std::vector<RootNodeInfo> GetMatchingSceneTreeNodes(const webots::Supervisor& supervisor, const std::string& typeNameToMatch)
{
  std::vector<RootNodeInfo> foundNodes;
  for (const auto& node : GetAllSceneTreeNodes(supervisor)) {
    if (node.typeName.find(typeNameToMatch) != std::string::npos) {
      foundNodes.push_back(node);
    }
  }
  return foundNodes;
}


bool GetFieldAsString(const webots::Node& parentNode,
                      const std::string& fieldName,
                      std::string& outputStr,
                      const bool failOnEmptyString)
{
  const auto* field = parentNode.getField(fieldName);
  if (field == nullptr) {
    PRINT_NAMED_ERROR("WebotsHelpers.GetFieldAsString.NullField",
                      "Field named %s does not exist (parent node type %s)",
                      fieldName.c_str(),
                      parentNode.getTypeName().c_str());
    return false;
  } else if (field->getType() != webots::Field::SF_STRING) {
    PRINT_NAMED_ERROR("WebotsHelpers.GetFieldAsString.WrongFieldType",
                      "Wrong field type '%s' for field %s (should be string)",
                      field->getTypeName().c_str(),
                      fieldName.c_str());
    return false;
  }

  outputStr = field->getSFString();
  
  if (failOnEmptyString && outputStr.empty()) {
    PRINT_NAMED_WARNING("WebotsHelpers.GetFieldAsString.EmptyString",
                        "Empty string for field name %s",
                        fieldName.c_str());
    return false;
  }

  return true;
}


int AddSceneTreeNode(const webots::Supervisor& supervisor,
                     const std::string& nodeStr)
{
  const auto* rootNode = supervisor.getRoot();
  DEV_ASSERT(rootNode != nullptr, "WebotsHelpers.AddSceneTreeNode.NullSupervisorRoot");
  
  auto* rootChildren = rootNode->getField("children");
  DEV_ASSERT(rootChildren != nullptr, "WebotsHelpers.AddSceneTreeNode.NullRootChildren");
  
  const auto nRootChildren = rootChildren->getCount();
  
  rootChildren->importMFNodeFromString(nRootChildren, nodeStr);
  auto* newNode = rootChildren->getMFNode(nRootChildren);
  DEV_ASSERT(newNode != nullptr, "WebotsHelpers.AddSceneTreeNode.FailedToAddNode");
  
  const int newNodeId = newNode->getId();
  return newNodeId;
}

  
void GetWebotsTranslation(const Pose3d& poseIn,
                          double* webotsTranslationOut,
                          const bool convertToMeters)
{
  const auto& trans = poseIn.GetTranslation();
  for (int i=0 ; i<3 ; i++) {
    webotsTranslationOut[i] = convertToMeters ? MM_TO_M(trans[i]) : trans[i];
  }
}

  
void GetWebotsRotation(const Pose3d& poseIn,
                       double* webotsRotationOut)
{
  const auto& angle_rad = poseIn.GetRotationAngle().ToDouble();
  const auto& axis = poseIn.GetRotationAxis();
  
  webotsRotationOut[0] = axis.x();
  webotsRotationOut[1] = axis.y();
  webotsRotationOut[2] = axis.z();
  webotsRotationOut[3] = angle_rad;
}


Pose3d ConvertTranslationRotationToPose(const double* transIn,
                                        const double* rotIn,
                                        const bool convertToMillimeters)
{
  Vec3f translation;
  for (int i=0 ; i<3 ; i++) {
    translation[i] = convertToMillimeters ? M_TO_MM(transIn[i]) : transIn[i];
  }
  
  Pose3d outPose(rotIn[3],
                 Vec3f(rotIn[0], rotIn[1], rotIn[2]),
                 std::move(translation));
  
  return outPose;
}

  
void ConvertRgbaToWebotsColorArray(const uint32_t rgbaColor, double* webotsColorOut)
{
  webotsColorOut[0] = ((rgbaColor >> 24) & 0xFF) / 255.0;
  webotsColorOut[1] = ((rgbaColor >> 16) & 0xFF) / 255.0;
  webotsColorOut[2] = ((rgbaColor >>  8) & 0xFF) / 255.0;
}


void SetNodePose(webots::Node& node, const Pose3d& newPose, const bool convertToMeters)
{
  double trans[3] = {0};
  GetWebotsTranslation(newPose, trans, convertToMeters);
  node.getField("translation")->setSFVec3f(trans);
  
  double rot[4] = {0};
  GetWebotsRotation(newPose, rot);
  node.getField("rotation")->setSFRotation(rot);
}


void SetNodeColor(webots::Node& node, uint32_t rgbaColor)
{
  double webotsColor[3] = {0};
  ConvertRgbaToWebotsColorArray(rgbaColor, webotsColor);
  node.getField("color")->setSFColor(webotsColor);
}

}; // namespace
}; // namespace

