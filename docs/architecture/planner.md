# Path planning

* Checks for obstacles in the [map component](map.md)
* Automatic replanning around new obstacles
* Grid-based A* planner, runs in thread
* Simpler planners can run in special cases

Path planning refers to the problem of navigating the robot from point A to B without collisions and, for Vector, in a way that fits with his character.

The following things happen, roughly in order, and skipping a bunch of details:

1. A [behavior](behaviors.md) (or behavior helper) executes a [DriveToPoseAction](actions.md) (or an action that contains one). This action has a goal pose (or set of goal poses)
   
1. The robot selects an appropriate planner based on the robot pose and goal. For long distances, this is always the `XYPlanner`, but for short distances or specific use cases (like aligning to dock with a cube) another simpler planner may be selected

1. The [`XYPlanner`](/engine/xyPlanner.h) lives in the `Vector` namespace and incorporates all of the obstacles from the [nav map](map.md). The `XYPlanner` owns a "worker" thread where the planning will be done
   
1. `XYPlanner` planner hands off to [`BidirectionalAStar`](/coretech/planning/engine/bidirectionalAStar.h) in its thread, which is part of coretech and doesn't know anything about, e.g. the `Robot` class. Planning takes some time to run, and runs in its own thread

1. During each tick of engine, the planning status is checked. When the plan is complete, it is processed into a path the robot can follow
   
1. The `PathComponent` and `SpeedChooser` use robot parameters + randomness to pick the speed that the robot should follow the path

1. The action, together with the `DrivingAnimationHandler` runs the animations which get layered onto the driving (e.g. sounds, eyes, head motion)
   
1. The [path following](/robot/supervisor/src/pathFollower.h) happens in the robot process.

1. While following the path, safety checks are made to determine if _replanning_ is needed. If there is a new obstacle along the path, the robot will replan a new one. Ideally this will happen in the background and be non-noticeable to the user, but if the robot gets too close to the obstacle, it'll have to stop and wait for the new plan to be computed

## A* on a uniform grid

XYPlanner uses a [canonical A* search](https://en.wikipedia.org/wiki/A*_search_algorithm) on a 2D grid. XYPlanner interfaces with the map component to determine if successor states are in collision with obstacles.

To help navigation in tight spaces, we use a hierarchical resolution on the 4-connected grid that uses smaller step sizes near obstacles to fine-tune the search space.

One drawback to this planner is that it is 2D, meaning the robot must always execute a point turn at the beginning and end of the path, since the path planner does not take the starting or ending heading into account.

Since the output of the A* search is a path on a grid, a smoothing step is employed to create a path made up of drive-able motion primitives such as straight lines and arcs.

We use a [bidirectional planner](/coretech/planning/engine/bidirectionalAStar.h) so that we terminate early if the goal state is not reachable (e.g. surrounded by obstacles).

## A* on a lattice

Note: We no longer use a lattice A* planner for motion planning. Instead, we use the A* on a grid as described above.

Some docs on planning on a lattice can be found in the [ROS SBPL docs](http://wiki.ros.org/sbpl). A brief overview is here.

Instead of planning on a grid, we plan with _motion primitives_. These are sections of arcs and straight lines that fit within the constraints of what the robot can execute. The space is discretized into 16 (or some other number) of angles, and set up so that each primitive starts and ends _exactly_ on a grid cell center aligned with one of the discrete angles.

In this way, once a plan is complete, it is ready to be executed as-is without any smoothing or trajectory optimization on top.

By varying relative cost of different actions, you can produce qualitatively different paths -- for example preferring backing up vs. turning around, and smooth arc turns vs. sharp "in place" turns.

Tools associated with the creation and visualization of the motion primitives can be found in the [planning](/coretech/planning) directory (look in [`tools`](/coretech/planning/tools) and [`matlab`](/coretech/planning/matlab)).

### Soft obstacles

To prevent the planner from getting stuck when the robot is immediately next to a known obstacle, it treats any collisions with the start point as "soft obstacles", and first plans the shortest path out of collision (see [`EscapeObstaclePlanner`](/engine/xyPlannerConfig.h)). It then prepends this trajectory onto the output of the Bidirectional planner, which treats all collisions as fatal.
