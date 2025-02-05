#version 440 core

#pragma shader_stage(vertex)

layout(location = 0) in vec3 Position;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec2 outUV;

void main()
{
    gl_Position = vec4(Position, 1.f);
    outUV = inUV;
}
