#version 440 core

#pragma shader_stage(fragment)

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outCol;

void main()
{
    outCol = vec4(1.f);
}
