#pragma once

#include "Framework.hpp"
#include "Input.hpp"

#include "glm/gtc/matrix_transform.hpp"

#include <unordered_map>

// seperate drawables and meshes.

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
    void Draw(Resources::CommandBuffer* pCmdBuffer);

    Pipeline* Pipe = nullptr;
    uint32_t PassIdx;
    std::vector<pbrMesh*> Drawables;
};

struct PassStage
{
    std::vector<PipeStage*> PipeStages;
};

class Camera
{
public:
    Camera()
    {
        CreateBuffer(WvpBuffer, sizeof(glm::mat4)*3, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

        Allocate(WvpBuffer, true);

        Map(&WvpBuffer);

        glm::mat4* pWvp = (glm::mat4*)WvpBuffer.pData;
        pWvp[0] = glm::mat4(1.f);
        pWvp[2] = glm::perspective(45.f, 16.f/9.f, 0.f, 9999.f);

        VkDescriptorSetLayoutBinding uBufferBinding{};
        uBufferBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        uBufferBinding.binding = 0;
        uBufferBinding.descriptorCount = 1;
        uBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

        WvpLayout.AddBinding(uBufferBinding);
    }

    void Move()
    {
        glm::vec3 Move;

        CamMat = glm::translate(CamMat, (Move*Speed));

        pWvp[1] = glm::inverse(CamMat);
    }

    void Rotate()
    {
        glm::vec2 MouseDelta = PrevMouse - glm::vec2(Input::InputMap.MouseX, Input::InputMap.MouseY);

        PrevMouse.x = Input::InputMap.MouseX;
        PrevMouse.y = Input::InputMap.MouseY;

        CamMat = glm::rotate(CamMat, MouseDelta.x, glm::vec3(0.f, 1.f, 0.f)); // rotate along the y-axis (up)
        CamMat = glm::rotate(CamMat, MouseDelta.y, glm::vec3(1.f, 0.f, 0.f)); // rotate along the x-axis (right)

        pWvp[1] = glm::inverse(CamMat);
    }

    Resources::DescriptorLayout WvpLayout;
    Resources::Buffer WvpBuffer;
    Resources::DescriptorSet* pWvpUniform;

    glm::mat4 CamMat;

private:
    glm::mat4* pWvp;
    float Speed = 0.3f;
    glm::vec2 PrevMouse;
};

/*! \brief Abstracts framebuffer creation by creating all images, views, and framebuffers from basic descriptions of the attachments provided by the user. */
class FrameBufferChain
{
public:
    void Bake(RenderPass* pPass);
    inline void AddAtt(VkImageCreateInfo AttDesc) { AttachmentDescs.push_back(AttDesc); }

    std::vector<Resources::FrameBuffer> FrameBuffers;

private:
    std::vector<VkImageCreateInfo> AttachmentDescs;

    uint32_t FBCount;
};

/*! \brief Contains heaps(arrays of allocator memory wrappers), pipelines, renderpasses, and drawables and renders them to the screen */
class SceneRenderer
{
public:
    SceneRenderer();

    Pipeline* CreatePipeline(std::string PipeName, uint32_t SubpassIdx, const char* VtxPath, const char* FragPath, uint32_t BlendAttCount = 0, VkPipelineColorBlendAttachmentState* BlendAttachments = nullptr, uint32_t DescriptorCount = 0, Resources::DescriptorLayout* pDescriptors = nullptr, PipelineProfile* pProfile = nullptr);
    pbrMesh* CreateDrawable(std::string Path, std::string PipeName);

    inline void AddFrameBufferAttachment(VkFormat Format, VkImageUsageFlagBits Usage)
    {
        VkImageCreateInfo DepthBuffer{};
        DepthBuffer.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        DepthBuffer.extent = { GetWindow()->Resolution.width, GetWindow()->Resolution.height, 1 };
        DepthBuffer.arrayLayers = 1;
        DepthBuffer.imageType = VK_IMAGE_TYPE_2D;
        DepthBuffer.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        DepthBuffer.mipLevels = 1;
        DepthBuffer.samples = VK_SAMPLE_COUNT_1_BIT;
        DepthBuffer.tiling = VK_IMAGE_TILING_OPTIMAL;
        DepthBuffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        DepthBuffer.format = Format;
        DepthBuffer.usage = Usage;

        FrameChain.AddAtt(DepthBuffer);
    }

    inline void AddRenderAttachment(VkFormat Format, VkImageLayout InitLay, VkImageLayout FinLay, VkAttachmentStoreOp StoreOp, VkAttachmentLoadOp LoadOp, VkAttachmentStoreOp StencilStoreOp, VkAttachmentLoadOp StencilLoadOp, VkClearValue* ClearValue, VkSampleCountFlagBits Samples)
    {
        if(LoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR && ClearValue == nullptr)
        {
            throw std::runtime_error("Scene Renderer : Tried to add a renderpass attachment with no clear value even though LoadOp is set to CLEAR");
        }

        ScenePass.AddAttachmentDesc(Format, InitLay, FinLay, StoreOp, LoadOp, StencilStoreOp, StencilLoadOp, ClearValue, Samples);
    }

    void AddPass(Subpass* pSubpass);

    void Bake();

    void Render();

    /* Descriptors */
        std::unordered_map<VkDescriptorSetLayout, Allocators::DescriptorPool> DescriptorHeaps; //! > map assigns each type of descriptorsetlayout in the scene a corresponding descriptor pool

    /* Command Heaps */
        Allocators::CommandPool GraphicsHeap; //! > Graphics command buffer allocator
        Allocators::CommandPool ComputeHeap; //! > Compute command buffer allocator
        Allocators::CommandPool TransferHeap; //! > Transfer command buffer allocator

    /* Transfer */
        void* pTransit = nullptr; //! > Raw transit memory pointer. Maps to the TransitBuffer

    /* Scene Render Information */
        PipelineProfile SceneProfile; //! > Scene-wide pipeline profile.
        Camera* SceneCam; //! > Scene camera structure.

private:
    /* Scene Pass */
        struct
        {
            VkSemaphore FrameSem;
            VkSemaphore RenderSem;
        } SceneSync;

        RenderPass ScenePass; //! > Scene wide renderpass. Passes can be added to the scene through AddPass().

    /* FrameBuffers */
        FrameBufferChain FrameChain; //! > Scene framebuffer chain. Attachments can be added through AddFrameBufferAttachment()


    Resources::Buffer TransitBuffer; //! > Scene transit buffer for memory copy operations.

    std::unordered_map<std::string, PipeStage*> PipeStages; //! > Pipeline stages (Pipeline+Drawables) mapped to string names
    std::vector<PassStage> PassStages; //! > Pipeline stages sorted by subpass.

    Resources::CommandBuffer* pRenderBuffer = nullptr; //! > Render buffer
};
