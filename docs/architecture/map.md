# Map component

* Holds the robots navigation map
* Tracks memory of things seen and places been
* BlockWorld is separate
* Nav Map is 2D (planar), Block/Face/Pet World is 3D
* Underlying QuadTree

## Nav Map aka. Memory Map

The [map component](/engine/navMap/mapComponent.cpp) owns the "nav(igation) map", which holds information about the "memory" of the things the robot has seen: including cliffs, observable objects (from vision), prox obstacles (from the distance sensor) and ground that the robot has successfully traversed in the past. In general, `MapComponent` should be the main gateway for either pushing content too, or getting information from, the NavMap, and no external system should access the MemoryMap directly.

The map consists of regions of `MemoryMapData`. The data has a content type, `EContentType` defined in [memoryMapTypes.h](/engine/navMap/memoryMap/memoryMapTypes.h). This includes, for example, `Unknown`, `ObstacleCharger`, `Cliff`. Additional data can also be stored along with the content type (e.g. a cube ID). Data can be added to the memory map using `Insert()`, modified with the `TransformContent()` functions, or queried with `FindContentIf()` or `AnyOf()`

Spatially constrained queries to the map are done with `MemoryMapRegions`, which are a collection of geometries that have built in optimizations for collision checking inside the map data structure. Most commonly include 2d implementations of `FastPolygon`, `Ball` (disk in 2d), or `Unions` of these types. As a rule of thumb, any [Bounded Convex Set](/coretech/common/engine/math/pointSet.h) can be inserted relatively efficiently, though performance is closely tied to geometric complexity. A rule of thumb the order of performance for fastest to slowest map traversal: Point, Ball/Disk, LineSegment, and then FastPloygon (traversal time grows geometrically with the number of vertices). Union and Intersection performance is the sum of the performance of the contained base geometries, but gets performance improvements by minimizing the total number of traversals through the map.

## Storage

As an implementation detail, the memory map is stored as a QuadTree. These details are entirely internal to the map and shouldn't affect the usage, but do provide efficient storage for large maps, including automatically splitting and merging cells in the QuadTree as data gets added or updated. This is easily visible in the Webots viz.

There is a limit to the amount of memory we want to use to store this data, so if the quad tree becomes "too large" (defined in [quadTree.cpp](/engine/navMap/quadTree/quadTree.cpp)) it will be truncated. I.e. if you drive forever in one direction, you start to lose memory of the things far away from the robot as it drives.

## Navigation

Some things in the nav map are obstacles, and will be added as obstacles for the [path planner](planner.md). These must be defined explicitly for all derived instances of `MemoryMapData`.
