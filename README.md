# Island Generator

This project is developed for our indie game, and will undergo significant changes based on the game design at this stage.

## Created for Unreal Engine 5.3
`NOTE: Many experimental features are used in the project`

### [Geometry Scripting](https://dev.epicgames.com/documentation/en-us/unreal-engine/geometry-scripting-users-guide-in-unreal-engine)
Geometry Scripting is used to generate island meshes.

### [PCG](https://dev.epicgames.com/community/learning/tutorials/j4xJ/unreal-engine-introduction-to-procedural-generation-plugin-in-ue5-3)
PCG is used to realize the decoration on the island.

# Credits

* The project is based on the [Unreal-Polygonal-Map-Gen](https://github.com/Jay2645/Unreal-Polygonal-Map-Gen) repository. The Unreal-Polygonal-Map-Gen project is excellent, but it has different goals, so I copied it and modified and expanded it.

* The original code was released under the Apache 2.0 license; this C++ port of the code is also released under the Apache 2.0 license. Again, this was based on the [mapgen2](https://github.com/redblobgames/mapgen2) repository.

* Also included in this code is a port of the [DualMesh](https://github.com/redblobgames/dual-mesh) library; DualMesh is also licensed under Apache 2.0.

* Poisson Disc Sampling is created using code from the [Random Distribution Plugin](https://github.com/Xaymar/RandomDistributionPlugin) and used under the Apache 2.0 license.

* Delaunay Triangulation is created [using the MIT-licensed Delaunator](https://github.com/delfrrr/delaunator-cpp) and made accessible through a number of Unreal helper functions. Something that's fairly annoying: Delaunay Triangulation is [built into the engine](https://github.com/EpicGames/UnrealEngine/blob/08ee319f80ef47dbf0988e14b546b65214838ec4/Engine/Source/Editor/Persona/Private/AnimationBlendSpaceHelpers.h), but is only accessible from the Unreal Editor. The data structures aren't exposed to other modules or Blueprint, so you can't use it without linker errors when shipping your game. The Unreal Engine code has a different license, so a third-party library has to be used.

* The [canvas_ity](https://github.com/a-e-k/canvas_ity) repository was used when drawing the texture, which is licensed under the ISC License

* The [Clipper2](https://github.com/AngusJohnson/Clipper2) is used to better offset the boundaries of the island, and it's licensed under the Boost Software License 1.0.

* The [PolyPartition](https://github.com/a-e-k/canvas_ity) is used to split polygons and is licensed under the MIT License.
