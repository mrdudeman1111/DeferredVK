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
public:
    std::vector<Vertex> Vertices;
    std::vector<uint32_t> Indices;

    Resources::Buffer MeshBuffer;

    Resources::Buffer MVP;
    Texture Albedo = {};
    Texture Normal = {};

    Resources::DescriptorSet Mesh$
};

/*! \brief Contains heaps(arrays of allocator memory wrappers), pipelines, renderpasses, and drawables and renders them to the screen */
class SceneRenderer
{
public:
    Pipeline* CreatePipeline() {}
    pbrMesh* CreateDrawable(const char* Path);

    std::unordered_map<VkDescriptorSetLayout, Allocators::DescriptorPool> DescriptorHeaps;

    Allocators::CommandPool GraphicsHeap;
    Allocators::CommandPool ComputeHeap;

    /* Scene Render Information */
    PipelineProfile SceneProfile;

private:
    RenderPass ScenePass;

    std::vector<Pipeline> Pipelines;
    std::vector<pbrMesh> Drawables;
};
