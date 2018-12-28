# sogo
Lightweight sound graph framework

## Design choices

* Will *not* contain any actual sound interface code
* Will *not* do resource management, resources lifetime etc are handled outside, resources for the graph are only pointer+size
* Will *not* support sub-graphs, the rendering engine should only work with "flat" graphs to reduce complexity. Authoring tools should handle this, just focus on designing data structures so it is easy to add/remove parts of a graph (which may be grouped in authoring tool)
* Will *not* support recursive connections - the target is not creative sound design but rather simpler game sound engine. Reduces complexity in rendering
* Pitching (by altering playback speed) is responsability of generator nodes (WAV/OGG/Procedural)
* Simple data structures - serialization should be very simple (but built outside of core)
* Small memory footprint
* Simple interface to add more node types - API should make it easy to define nodes in a shared or static library
* Straight forward rendering, since no recursive connections are allowed it is simple to determine rendering order (Authoring tools should write out the nodes in the correct rendering order)
* Design should allow for a task-basked job system to multithread the rendering - easy dependency tracking
* Simple static type/size of nodes, extra data are either parameters or resources provided by the graph
* External input and outputs (both sound and events) should be easily accessible
* External access to node parameters
* External access to node triggers
* Ideally I would like to make it a single-header library - will try to keep core small, not sure about dependecies - need hashing and hash-to-index lookup which depends on xxHash and JCash/containers currently
* Some form or node type registry will be needed but should be built outside of the core

## Third party libraries

Third party libraries are added as git submodules in the third-party folder

* containers from https://github.com/JCash/containers.git
* xxHash from https://github.com/Cyan4973/xxHash.git

## Building

Building is just the tests right now, no actual application.

### Windows

* Start a Visual Studio command prompt with the visual studio environment variables and path set up
* Execute 'compile_cl.bat' or 'compile_cl_debug.bat' of optimized vs un-optimized build respectively
* Output is in 'build' folder

### Linux/MacOS (clang) CURRENTLY BROKEN/INCOMPLETE/UNTESTED

* Execute 'compile_clang.bat' or 'compile_clang_debug.bat' of optimized vs un-optimized build respectively
* Output is in 'build' folder

## TODO

* Event triggering - simple "trigger" event or something more complex? A node should be able to trigger other nodes
* How to facilitate node specific buffer allocations for things like delay/reverb? Can be frame rate dependant
* FrameRate is now set at Graph construction, not sure this is a good design choice
* Authoring tools - considering DearImGUI for this, but haven't decided yet
* Tons of node types - OGG, WAVE, Pan, Random/Sequential triggers, Mixing, Effects etc
* Much more that I forgot to list...
