#pragma once

#include "Framework.hpp"

#include <unordered_map>

struct Vertex
{
    glm::vec3 Position;
    glm::vec3 Normal;
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
    pbrMesh();

    static Resources::DescriptorLayout MeshLayout;

    std::vector<Vertex> Vertices;
    std::vector<uint32_t> Indices;

    uint32_t IndexOffset;

    Resources::Buffer MeshBuffer;

    Resources::Buffer MeshMat;
    Texture Albedo = {};
    Texture Normal = {};

    Resources::DescriptorSet MeshDescriptor;

    void Draw(VkCommandBuffer& cmdBuff, VkPipelineLayout Layout, uint32_t MeshDescriptorIdx);
};

struct PipeStage
{
    Pipeline Pipe;
    std::vector<pbrMesh*> Drawables;
};

/*! \brief Contains heaps(arrays of allocator memory wrappers), pipelines, renderpasses, and drawables and renders them to the screen */
class SceneRenderer
{
public:
    SceneRenderer();

    Pipeline* CreatePipeline() {}
    pbrMesh* CreateDrawable(std::string Path);

    std::unordered_map<VkDescriptorSetLayout, Allocators::DescriptorPool> DescriptorHeaps;

    Allocators::CommandPool GraphicsHeap;
    Allocators::CommandPool ComputeHeap;
    Allocators::CommandPool TransferHeap;

    void* pTransit = nullptr;

    /* Scene Render Information */
    PipelineProfile SceneProfile;

private:
    RenderPass ScenePass;

    Resources::Buffer TransitBuffer;

    std::vector<PipeStage> RenderStages;
}
