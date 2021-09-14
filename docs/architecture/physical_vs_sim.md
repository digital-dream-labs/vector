
# Physical vs. Simulated

The Victor project utilizes [Webots](https://www.cyberbotics.com) for simulation purposes. (Webots setup details can be found [here](../../simulator)). Most of the engine, animation, and robot process code runs on both the physical and simulated robot. The code that differs is that which is responsible for interfacing to a hardware component or OS-specific feature.

In the Robot process, this code is referred to as the [HAL](arch_overview.md#HAL) and its interface is defined in [hal.h](../../robot/hal/include/anki/cozmo/robot/hal.h). There exists both a [simulated](../../robot/hal/sim)(which uses simulated Webots sensors, motors, displays, etc.) and [physical](../../robot/hal/src) version of the implmentation.

In the animation and engine processes, this separation is done on a per-hardware component basis with a class dedicated to each. For example, the animation process's interface to the face display is defined by [`faceDisplay.h`](../../animProcess/src/cozmoAnim/faceDisplay/faceDisplay.h). The simulated implementation is defined by [`faceDisplayImpl_mac.cpp`](../../animProcess/src/cozmoAnim/faceDisplay/faceDisplayImpl_mac.cpp) while the physical version is defined by by [`faceDisplayImpl_vicos.cpp`](../../animProcess/src/cozmoAnim/faceDisplay/faceDisplayImpl_vicos.cpp)


### `#ifdef SIMULATOR`

The `SIMULATOR` macro is sometimes used in areas where logic between simulation and physical differs, but does not warrant an entirely separate class or file.


### Webots Tests

In addition to standard unit tests on engine and coretech components, our continuous integration process also includes Webots tests. A Webots test, an example of which can be found [here](../../simulator/controllers/webotsCtrlBuildServerTest/), invokes the simulator and allows you to define a special "UI/App" client to execute various commands to the engine. In so doing you can run complex, high-level tests in a simulated environment that might otherwise be impossible or too cumbersome to do as a unit test.

Webots tests are executed in all pull requests to master and on nightly master tests, but they can also be run locally via [`webotsTest.py`](../../project/build-scripts/webots/webotsTest.py). The tests that are executed are defined in [`webotsTests.cfg`](../../project/build-scripts/webots/webotsTests.cfg).


