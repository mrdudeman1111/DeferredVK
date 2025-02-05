#pragma once

#include "Framework.hpp"

#include <unordered_map>

struct Vertex
{
    glm::vec3 Position;
    glm::vec2 UV;
};

struct Texture
{
    Resources::Image* Img = nullptr;
    VkSampler Sampler;
};

class pbrMesh
{
    std::vector<Vertex> Vertices;
    std::vector<uint32_t> Indices;

    Resources::Buffer MeshBuffer;

    Resources::Buffer MVP;
    Texture Albedo = {};
    Texture Normal = {};

    Resources::DescriptorSet MeshDesc;
};

/*! \brief Contains heaps(arrays of allocator memory wrappers), pipelines, renderpasses, and drawables and renders them to the screen */
class SceneRenderer
{
    std::unordered_map<VkDescriptorSetLayout, Allocators::DescriptorPool> DescriptorHeaps;

    Allocators::CommandPool GraphicsHeap;
    Allocators::CommandPool ComputeHeap;

    /* Scene Render Information */
    RenderPass ScenePass;
    PipelineProfile SceneProfile;

    std::vector<Pipeline> Pipelines;
    std::vector<pbrMesh> Drawables;
};
