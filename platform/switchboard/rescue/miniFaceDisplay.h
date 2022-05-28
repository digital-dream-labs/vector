/**
* File: miniFaceDisplay.h
*
* Author: chapados
* Date:   02/22/2019
*
* Description: Minimal face display functionality to support emergency pairing in a 
*              fault-code situation.
*
* Copyright: Anki, Inc. 2019
**/
#pragma once

#include <cstdint>
#include <string>

namespace Anki {
namespace Vector {

void DrawFaultCode(uint16_t fault, bool willRestart);

bool DrawImage(std::string& image_path);

// Draws BLE name and url to screen
bool DrawStartPairingScreen(const std::string& robotName);

// Draws BLE name, key icon, and BLE pin to screen
void DrawShowPinScreen(const std::string& robotName, const std::string& pin);

} // namespace Vector
} // namespace Anki