#version 440 core

#define MAX_RENDERABLE_INSTANCES 600

#pragma shader_stage(vertex)

layout(location = 0) in vec3 Position;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec2 UV;

layout(location = 0) out vec3 oPos;
layout(location = 1) out vec3 oNorm;
layout(location = 2) out vec2 oUV;

struct VkDrawCommand
{
    uint IndexCount;
    uint InstanceCount;
    uint FirstIndex;
    int VertexOffset;
    uint FirstInstance;
};

layout(set = 0, binding = 0) uniform Cam_t
{
    mat4 World;
    mat4 View;
    mat4 Proj;
} Camera;

layout(set = 0, binding = 2) buffer readonly DynamicBuff
{
    mat4 Transforms[];
} DynSceneBuffer;

layout(std430, set = 1, binding = 0) buffer readonly Draw_t
{
    VkDrawCommand Cmd;

    uint MeshBounds;
    uint InstanceCount;

    uint InstanceSceneIndices[MAX_RENDERABLE_INSTANCES]; // in // maps VisibleInstances[gl_InstanceIndex] to the scene object/instance to be rendered. (in other words use InstanceSceneIndex[VisibleInstances[gl_InstanceIndex]] to get the index of the mesh transform in scene buffer.)
    uint VisibleInstances[]; // manually map the instances to the global instance structure. (some meshes may have instances at indices like 1, 4, 12, 48, 49, 61, 76, etc. This lists the instances in the global instance buffer that represents this mesh's instances.)
} Mesh;

//layout(location = 1) in vec2 inUV;

//layout(location = 0) out vec2 outUV;

void main()
{
    // Issue here. the MeshInstanceIndices is supposed to be mapped to scene buffer indices. But we're skipping that step, pass the instanced::Instances, then use it to map MeshInstanceIndices into scene buffer indices.
    // uint TransformIdx = Mesh.InstanceSceneIndices[gl_InstanceIndex];
    uint TransformIdx = Mesh.VisibleInstances[gl_InstanceIndex];
    gl_Position = Camera.Proj * Camera.View * DynSceneBuffer.Transforms[TransformIdx]* vec4(Position, 1.0f);

    oPos = Position;
    // oNorm = normalize(transpose(inverse(mat3(DynSceneBuffer.Transforms[TransformIdx]*Camera.View)))*Normal);
    oNorm = Normal;
    oUV = UV;
    //outUV = inUV;
}
