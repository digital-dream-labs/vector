/**
 * File: webotsHelpers.h
 *
 * Author: Matt Michini
 * Date:  01/22/2018
 *
 * Description: A few helper functions for common webots node queries
 *
 * Copyright: Anki, Inc. 2018
**/

#ifndef __Simulator_WebotsCtrlShared_WebotsHelpers_H__
#define __Simulator_WebotsCtrlShared_WebotsHelpers_H__

#include <webots/Node.hpp>

namespace webots {
  class Supervisor;
}

namespace Anki {
class ColorRGBA;
class Pose3d;
namespace WebotsHelpers {

struct RootNodeInfo {
  RootNodeInfo(webots::Node* nodePtr = nullptr,
               int type = webots::Node::NO_NODE,
               const std::string& typeName = "")
  : nodePtr(nodePtr)
  , type(type)
  , typeName(typeName)
  {};
  
  webots::Node* nodePtr;  // pointer to the node
  int type;               // node type (e.g. ROBOT)
  std::string typeName;   // type name. For protos, it's just the proto name e.g. "CozmoBot"
};
  
// Returns a vector containing information about all of the nodes in the Webots scene tree
std::vector<RootNodeInfo> GetAllSceneTreeNodes(const webots::Supervisor& supervisor);

// Returns information about the first node in the scene tree whose TypeName matches the input string.
// Note: As long as the input string exists in the node's TypeName, it's considered a match (e.g. if
// input string is "LightCube", then "LightCubeNew" and "LightCubeOld" would both match)
RootNodeInfo GetFirstMatchingSceneTreeNode(const webots::Supervisor& supervisor,
                                           const std::string& typeNameToMatch);

// Return all scene tree nodes whose typeNames match the input string.
std::vector<RootNodeInfo> GetMatchingSceneTreeNodes(const webots::Supervisor& supervisor,
                                                    const std::string& typeNameToMatch);

// Populates outputStr with the string contents of the specified field. Returns true on success.
// Does not modify outputStr if not successful.
bool GetFieldAsString(const webots::Node& parentNode,
                      const std::string& fieldName,
                      std::string& outputStr,
                      const bool failOnEmptyString = true);
  
// Create a new node and append it to the end of the scene tree. The node is defined by nodeStr.
// Returns the Webots node ID (unique identifier) of the new node.
int AddSceneTreeNode(const webots::Supervisor& supervisor,
                     const std::string& nodeStr);
  
// Extract a Webots translation array from the given Anki Pose3d. If convertToMeters = true, then the input Pose3d is
// assumed to have a translation in millimeters, and the output translation vector will be in meters.
void GetWebotsTranslation(const Pose3d& poseIn,
                          double* webotsTranslationOut,
                          const bool convertToMeters = false);
  
// Extract a Webots rotation array from the given Anki Pose3d
void GetWebotsRotation(const Pose3d& poseIn,
                       double* webotsRotationOut);

// Convert from a Webots translation/rotation to an Anki Pose3d. If convertToMillimeters = true, then the input
// translation array is assumed to be in meters, and the output pose translation will be in millimeters.
Pose3d ConvertTranslationRotationToPose(const double* transIn,
                                        const double* rotIn,
                                        const bool convertToMillimeters = false);

// Convert a 32-bit RGBA color to an array of RGB colors (with range of 0.0 to 1.0)
void ConvertRgbaToWebotsColorArray(const uint32_t rgbaColor,
                                   double* webotsColorOut);

// Set the "translation" and "rotation" fields of a node based on the input newPose. If convertToMeters = true, then
// the input Pose3d is assumed to have a translation in millimeters.
void SetNodePose(webots::Node& node, const Pose3d& newPose, const bool convertToMeters = false);

// Set the SFVec3F "color" field of a node based on the input rgbaColor.
void SetNodeColor(webots::Node& node, uint32_t rgbaColor);
  
}; // namespace
}; // namespace

#endif // __Simulator_WebotsCtrlShared_WebotsHelpers_H__
