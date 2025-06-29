## Hierarchy
The "Renderer" Uses the "Framework", which uses the "Wrappers" to implement a functional renderer. The renderer includes PBR rendered meshes and a simple, easy to use interface.

## Memory
 All the memory the framework uses (with a few exceptions,) is contained in a hidden global structure declared in src/Framework.cpp called ApplicationMemory. src/Framework.cpp also contains definitions for global memory and structure creation functions (such as functions for creating buffers and images, as well as functions for allocating them.). The file also contains two other hidden variables (which are available through getter functions) called "gContext" and "gWindow".

