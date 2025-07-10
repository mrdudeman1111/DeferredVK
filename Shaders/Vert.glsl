#version 440 core

#pragma shader_stage(vertex)

layout(location = 0) in vec3 Position;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec2 UV;

layout(std140, set = 0, binding = 0) uniform Cam_t
{
    mat4 World;
    mat4 View;
    mat4 Proj;
} Camera;

//layout(location = 1) in vec2 inUV;

//layout(location = 0) out vec2 outUV;

void main()
{
    gl_Position = Camera.Proj * Camera.View * Camera.World * vec4(Position, 1.0f);
    //outUV = inUV;
}
