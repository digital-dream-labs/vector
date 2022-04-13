//
//  SignalDefMacros.h
//
//  Description: Macros for processing signal definition files
//
//  Created by Kevin Yoon 01/22/2015
//  Copyright (c) 2015 Anki, Inc. All rights reserved.
//


// Explicitly undefine so we can redefine without warnings from previous includes
#undef DEF_SIGNAL


#ifndef SIGNAL_DEFINITION_MODE

// Define the available modes
#undef SIGNAL_DEFINITION_MODE
#undef SIGNAL_CLASS_DECLARATION_MODE
#undef SIGNAL_CLASS_DEFINITION_MODE

#define SIGNAL_CLASS_DECLARATION_MODE         0
#define SIGNAL_CLASS_DEFINITION_MODE          1

#define DEF_SIGNAL(__SIGNAME__, ...)

#else // We have a message definition mode set

#ifndef SIGNAL_CLASS_NAME
#error SIGNAL_CLASS_NAME must be defined
#endif




// Macro for declaring signal type and accessor function
// e.g. DEF_SIGNAL(RobotConnect, RobotID_t robotID, bool successful) creates
//
//  static Signal::Signal<void (RobotID_t robotID, bool successful)>& GetRobotConnectSignal();
//
//  A listener may subscribe to a signal with a callback function like so.
//
//      auto cbRobotConnectSignal = [this](RobotID_t robotID, bool successful) {
//        this->HandleRobotConnectSignal(robotID, successful);
//      };
//      _signalHandles.emplace_back( CozmoEngineSignals::GetRobotConnectSignal().ScopedSubscribe(cbRobotConnectSignal));
//
//      where _signalHandles is a container for the SmartHandle that ScopedSubscribe() returns.
//      In this case, _signalHandles should be a member of the same class as HandleRobotConnectSignal().
//      See simpleSignal.hpp for more details.
//
#if SIGNAL_DEFINITION_MODE == SIGNAL_CLASS_DECLARATION_MODE

#define DEF_SIGNAL(__SIGNAME__, ...) \
static Signal::Signal<void (__VA_ARGS__)>& __SIGNAME__##Signal();


// Macro for creating signal defintion
// e.g.
// e.g. DEF_SIGNAL(RobotConnect, RobotID_t robotID, bool successful) creates
//
//  static Signal::Signal<void (RobotID_t robotID, bool successful)> _RobotConnectSignal;
//  Signal::Signal<void (RobotID_t robotID, bool successful)>& CozmoEngineSignals::GetRobotConnectSignal() { return _RobotConnectSignal; }
//

#elif SIGNAL_DEFINITION_MODE == SIGNAL_CLASS_DEFINITION_MODE

#define DEF_SIGNAL(__SIGNAME__, ...) \
static Signal::Signal<void (__VA_ARGS__)> _##__SIGNAME__##Signal; \
Signal::Signal<void (__VA_ARGS__)>& SIGNAL_CLASS_NAME::__SIGNAME__##Signal() { return _##__SIGNAME__##Signal; } \


//
// Unrecognized mode
//
#else
#error Invalid value for MESSAGE_DEFINITION_MODE
#endif

#undef SIGNAL_DEFINITION_MODE

#endif  // ifndef SIGNAL_DEFINITION_MODE
