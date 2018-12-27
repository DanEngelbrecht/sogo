# sogo
Lightweight sound graph framework

## Design choices

* Decided to *not* support sub-graphs, the rendering engine should only work with "flat" graphs to reduce complexity. Authoring tools should handle this, just focus on designing data structures so it is easy to add/remove parts of a graph (which may be grouped in authoring tool)
* Pitching (by altering playback speed) is responsability of generator nodes (WAV/OGG/Procedural)
* No recursive connections allowed - the target is not creative sound design but rather simpler game sound engine. Reduces complexity in rendering
* Simple data structures
* Small memory footprint
* Simple interface to add more node types
* Straight forward rendering, since no recursive connections are allowed it is simple to determine rendering order
* Design should allow for a task-basked job system to multithread the rendering
* Simple static type/size of nodes, extra data are either parameters or resources provided by the graph
* External input and outputs (both sound and events) should be easily accessible
* External access to node parameters
* No built in resource manager, resources lifetime etc are handled outside, resources for the graph are only pointer+size

## Third party libraries

Third party libraries are added as git submodules in the third-party folder

* containers from https://github.com/JCash/containers.git
* xxHash from https://github.com/Cyan4973/xxHash.git

## Building

Building is just the tests right now, not actuall application.

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
* Much more that I forgot to list...
