#version 440 core

#pragma shader_stage(fragment)

layout(location = 0) in vec3 Pos;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec2 UV;

layout(location = 0) out vec4 outCol;

struct Light_t
{
    vec4 Position;
    vec4 Color;
};

layout(set = 0, binding = 3) buffer readonly LightBuffer
{
    int CurrentLightCount;
    Light_t Lights[10000];
} SceneLights;

// returns the lighting intensity based on different factors (also takes into account the light's color)
vec4 Light(int LightIdx)
{
    vec3 LightDir = SceneLights.Lights[LightIdx].Position.xyz-Pos;
    float nDl = dot(Normal, normalize(LightDir));
    float Dist = LightDir.length();
    // float Falloff = 1.f/(Dist*Dist);
    float Falloff = 1.f;

    float TotCoefficient = nDl * Falloff;

    return SceneLights.Lights[LightIdx].Color*TotCoefficient;
}

void main()
{
    // only call if the light will have an effect on the fragment.

    outCol = vec4(0.01f, 0.01f, 0.01f, 0.01f);

    for(int i = 0; i < SceneLights.CurrentLightCount; i++)
    {
        outCol += Light(i);
    }

    // int i = 0;
    // outCol = Light(i);
}
