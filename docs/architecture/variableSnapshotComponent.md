# The Variable Snapshot Component

When thinking about how Vector's character should behave/evolve over long periods of time, there is often no difference between that evolution while the robot is powered on or powered off. To remove the need for every behavior/system to independently monitor the robot's power state and save data to disk before powering off, the Variable Snapshot Component (VSC) was introduced to provide a uniform way of thinking about data over long periods of time. Initializing a member variable using the VSC means that a) if it's the first time the variable has been initialized it will be added to the VSC and b) if the variable was initialized and/or updated during a previous robot boot cycle the last state of the data will be restored.

The variable snapshot component does not (as of now) explicitly support versioning. If you really need to do migration from an older version, just create a new variable snapshot ID and load in both variables

## Usage
  * Variables are identified in the VSC by a unique clad enum value. When introducing a new variable, add a new enum to [`/clad/src/clad/types/variableSnapshotIds.clad`](/clad/src/clad/types/variableSnapshotIds.clad)
  * To keep data synchronized between the member variable and VSC the member variable must be a shared_ptr. The VSC then keeps an internal copy of the shared_ptr, allowing it to save the most up to date data to disk without the need for an explicit save call from external code
  * If you would like to take a snapshot of a custom data type, simply add a new encoder to [`/engine/components/variableSnapshot/variableSnapshotEncoder.cpp`](/engine/components/variableSnapshot/variableSnapshotEncoder.cpp).
    * The outermost layer of the stored Json should be an object with keys `kVariableSnapshotIdKey` for the id (which should not generally need to be used in the encoder/decoder) and `kVariableSnapshotKey` for the Json object in which the data is stored.
    * The encoder should return a `bool` upon successful encoding and take as arguments a `shared_ptr<T>` with data to be stored and a `Json::Value&` to be filled.
    * The decoder should be `void` and take as arguments a `shared_ptr<T>` to fill with data and a `Json::Value&` with previously stored data.
    * See other encoders and decoders as examples.  

## Example Uses
  * See the [`GreetAfterLongTime` behavior](/engine/aiComponent/behaviorComponent/behaviors/behaviorGreetAfterLongTime.cpp) for an example use of this component.

## Current System Limitations (as of 8/9/2018)
  * The component can not handle multiple reinitializations of variables. As of now, the component ignores reinitializations, does not save them, and should have a warning.
  * Since variables can not be reinitialized, they also can not be used across multiple locations without sharing the shared pointer from the original initialization.
  * As of now, Vector loads data on startup and saves data on shutdown. A force restart would interfere with this process. For this reason, *the Variable Snapshot Component should not be used for critical information*.

[Variable Snapshot Component Folder](/engine/components/variableSnapshot/)
