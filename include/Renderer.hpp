#pragma once

#include "Wrappers.hpp"

#include <map>

struct Vertex
{
    glm::vec3 Position;
    glm::vec2 UV;
};

struct Texture
{
    Resources::Image* Img;
    VkSampler2D Sampler;
};

class pbrMesh
{
    std::vector<Vertex> Vertices;
    std::vector<uint32_t> Indices;

    Resources::Buffer MeshBuffer;

    Resources::Buffer MVP;
    Texture Albedo;
    Texture Normal;

    Resources::DescriptorSet MeshDesc;
};


/*! \brief Contains heaps(arrays of allocators), pipelines, renderpasses, and drawables and renders them to the screen */
class SceneRenderer
{
    std::unordered_map<VkDescriptorSetLayout, Allocators::DescriptorPool> DescriptorHeaps;

    /* Scene Render Information */
    VkRenderPass ScenePass;
    PipelineProfile SceneProfile;

    std::vector<Pipeline> Pipelines;
    std::vector<pbrMesh> Drawables;
};
