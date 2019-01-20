|Branch      | OSX / Linux |
|------------|-------------|
|master      | [![Build Status](https://travis-ci.org/DanEngelbrecht/sogo.svg?branch=master)](https://travis-ci.org/DanEngelbrecht/sogo?branch=master) |

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
* Ideally I would like to make it a single-header library - will try to keep core small, not sure about dependencies - need hashing and hash-to-index lookup which depends on xxHash and JCash/containers currently
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

### Linux/MacOS (clang)

* Execute 'compile_clang.bat' or 'compile_clang_debug.bat' of optimized vs un-optimized build respectively
* Output is in 'build' folder

## TODO

* Decide exactly what is part of core and what is not
* Event triggering - simple "trigger" event or something more complex?
  * A node can trigger another node using the Trigger(graph, node_index, trigger_index).
  * Need trigger input and output and connections - rename NodeConnection to AudioConnection?
* Should a node be able to alter parameters of another node? Doubtful...
* FrameRate is now set at Graph construction, not sure this is a good design choice
* Authoring tools - considering DearImGUI for this, but haven't decided yet
* Tons of node types - OGG, WAVE, Pan, Random/Sequential triggers, Mixing, Effects etc
* Audio output base so it is possible to actually hear the results, not part of core, likely part of separate test app
* Much more that I forgot to list...

## DONE
* How to facilitate node specific buffer allocations for things like delay/reverb? Can be frame rate dependant
  * GetNodePropertiesCallback - specify how much memory is needed based on max frame count per batch and frame rate
* Move hash-based lookup of parameters out of code, core uses direct addressing by node index and parameter/trigger index. Authoring tool should allow for making "public named" parameters/triggers that can be used by the runtime outside the core, lookup is name (hash) to node + index.
This makes it easier to keep core down and remove dependencies.
* Nodes can now be named using the HAccess utility. Setting parameters with only knowing node name and parameter name is now supported without affecting core part.
* Triggers
  * Triggers need to be queued - can't have just a "trigger x was triggered n times since last render call". Order matters for triggers - start vs stop etc.
  * We allocate max_trigger_event_count per node that has any triggers.
  * Queue is array of TTriggerIndex
