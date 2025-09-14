## Hierarchy
 The "Renderer" Uses the "Framework", which uses the "Wrappers" to implement a functional renderer. The renderer features simple PBR rendering and an easy-to-use interface. The cook-torrance shading model will be used for lighting.

## Shader Compilation
 Shaders will be merged with base shader classes that handl things like clean instancing and integration with the culling system and memory handling.

## Deferred\Forward Rendering
 This renderer will have a deferred rendering capable framebuffer, but the attachments will only be used during the deferred pass in the scene renderpass. After deferred rendering is finished, The deferred attachments will be cleared for the next render, but the Depth and Swapchain iamge will be stored for use during forward rendering. Most order-dependent objects will be rendered during forward rendering (i.e. transparent objects.).

## Rendering
 The Renderer starts a render by updating a mesh's draw buffer. This is done by putting the mesh's instance buffer through a compute pipeline that uses the "Draw.comp" shader. This shader takes all the instances of the mesh and performs frustum culling, occlusion culling, and light culling. If the instance is occluded, it is not included in the draw call. The shader outputs a list of the lights affecting the instance as well, reducing the concern of expensive lighting calculations.

## Memory
 All the memory the framework uses (with a few exceptions,) is contained in a hidden global structure declared in src/Framework.cpp called ApplicationMemory. src/Framework.cpp also contains definitions for global memory and structure creation functions (such as functions for creating buffers and images, as well as functions for allocating them.). The file also contains two other hidden variables (which are available through getter functions) called "gContext" and "gWindow".


## Planned
 Change the current "Vert" and "Frag" glsl files (instanced shaders) and use them as templates I.E. let user write shaders without worrying about how to access the right data by always calling Vert() and Frag() similar to gdScript or shadertoy. Users will also be allowed to write forward or deferred lighting functions in .light Files.

## References
 https://gpuopen.com/learn/mesh_shaders/mesh_shaders-from_vertex_shader_to_mesh_shader/
