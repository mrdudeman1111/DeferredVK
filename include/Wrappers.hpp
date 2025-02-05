#pragma once

#include <vector>
#include <string>

#include "SDL3/SDL.h"
#include "SDL3/SDL_vulkan.h"

#include "vulkan/vulkan.h"

#include "glm/glm.hpp"

struct PipeLocation
{
    VkAccessFlagBits Access;
    VkPipelineStageFlagBits Stage;
};

namespace Resources
{
    class Allocation
    {
    public:
        uint32_t Size;
        uint32_t Offset;

        VkDeviceMemory* pMemory;
    };

    struct Image
    {
        VkImage Img;

        VkFormat Format;
        VkExtent2D Resolution;

        Allocation Alloc;
    };

    struct Buffer
    {
        VkBuffer Buff;

        Allocation Alloc;

        void* pData = nullptr;
    };

    class CommandBuffer
    {
    public:
        void Bake(VkCommandPool* pCmdPool, bool bCompute);
        void Start();
        void Stop();

        operator VkCommandBuffer()
        {
            return cmdBuffer;
        }
        operator VkCommandBuffer*()
        {
            return &cmdBuffer;
        }

        VkFence Fence;

    private:
        VkCommandBuffer cmdBuffer;
        bool bCompute;
    };

    class DescriptorLayout
    {
    public:
        void AddBinding(VkDescriptorSetLayoutBinding Binding);

        VkDescriptorSetLayout Layout = VK_NULL_HANDLE;

        VkDescriptorSetLayoutBinding* GetBindings(uint32_t& Count) { Count = Bindings.size(); return Bindings.data(); };

    private:
        std::vector<VkDescriptorSetLayoutBinding> Bindings;
    };

    struct DescUpdate
    {
        VkDescriptorType DescType;
        uint32_t DescCount;
        uint32_t DescIndex;

        uint32_t Binding;

        Buffer* pBuff;
        size_t Offset;
        size_t Range;
    };

    class DescriptorSet
    {
    public:
        VkDescriptorSet DescSet; //! > The vulkan api handle to the allocated descriptor set.
        DescriptorLayout* DescLayout; //! > A pointer to the descriptor layout used to create this descriptor set.

        //! \brief Wraps descriptor writes using a custom struct.
        void Update(DescUpdate UpdateInfo);
    };

    // Wraps FrameBufferInformation
    class FrameBuffer
    {
    public:
        void AddBuffer(VkImageCreateInfo ImgCI);
        void AddBuffer(VkImageView ImgView);
        void Bake(VkRenderPass Renderpass);

    private:
        VkFramebuffer Framebuff;
        std::vector<VkImageCreateInfo> AttachmentInfos;

        std::vector<VkImage> Attachments;
        std::vector<VkDeviceMemory> AttachmentAllocations;
        std::vector<VkImageView> AttachmentViews;
    };
}

namespace Allocators
{
    class DescriptorPool
    {
    public:
        void SetLayout(Resources::DescriptorLayout* Layout) { DescLayout = Layout; };

        void Bake(uint32_t SetCount);

        Resources::DescriptorSet CreateSet();

        VkDescriptorPool DescPool;

        uint32_t AllocatedSets;
        size_t MaxSets;

    private:
        Resources::DescriptorLayout* DescLayout;

        std::vector<VkDescriptorPoolSize> Sizes;
    };

    class CommandPool
    {
    public:
        void Bake(bool bCompute);
        void Submit(Resources::CommandBuffer* pCmdBuffer, VkFence* pFence, uint32_t SignalSemCount = 0, VkSemaphore* SignalSemaphores = nullptr, uint32_t WaitSemCount = 0, VkSemaphore* WaitSemaphores = nullptr);
        Resources::CommandBuffer CreateBuffer();

        VkCommandPool cmdPool;

        bool bComp;
    };
}

struct Subpass
{
    inline void AddColorAttachment(uint32_t AttIdx, VkImageLayout ImgLayout) { ColorAttachments.push_back({AttIdx, ImgLayout}); }
    inline void AddInputAttachment(uint32_t AttIdx, VkImageLayout ImgLayout) { InputAttachments.push_back({AttIdx, ImgLayout}); }
    inline void AddResolveAttachment(uint32_t AttIdx, VkImageLayout ImgLayout) { ResolveAttachments.push_back({AttIdx, ImgLayout}); }
    inline void AddDepthAttachmment(uint32_t AttIdx, VkImageLayout ImgLayout) { DepthAttachment = {AttIdx, ImgLayout}; }
    inline void AddPreserveAttachment(uint32_t AttIdx) { PreserveAttachments.push_back(AttIdx); }

    VkAttachmentReference* GetColorAttachments(uint32_t& Count) { Count = ColorAttachments.size(); return ColorAttachments.data(); }
    VkAttachmentReference* GetInputAttachments(uint32_t& Count) { Count = InputAttachments.size(); return InputAttachments.data(); }
    VkAttachmentReference* GetResolveAttachments(uint32_t& Count) { Count = ResolveAttachments.size(); return ResolveAttachments.data(); }
    VkAttachmentReference* GetDepthAttachment() { return &DepthAttachment; }
    uint32_t* GetPreserveAttachments(uint32_t& Count) { Count = PreserveAttachments.size(); return PreserveAttachments.data(); }

private:
    std::vector<VkAttachmentReference> ColorAttachments;
    std::vector<VkAttachmentReference> InputAttachments;
    std::vector<VkAttachmentReference> ResolveAttachments;
    VkAttachmentReference DepthAttachment;
    std::vector<uint32_t> PreserveAttachments;
};

struct RenderPass
{
    void AddAttachmentDesc(VkFormat Format, VkImageLayout InitLay, VkImageLayout FinLay, VkAttachmentStoreOp StoreOp, VkAttachmentLoadOp LoadOp, VkAttachmentStoreOp StencilStoreOp, VkAttachmentLoadOp StencilLoadOp, VkSampleCountFlagBits Samples = VK_SAMPLE_COUNT_1_BIT);
    inline void AddPass(Subpass& sPass) { Subpasses.push_back(sPass); }

    void Bake();

    VkRenderPass rPass;

private:
    std::vector<Subpass> Subpasses;
    std::vector<VkAttachmentDescription> Attachments;
};

struct VertDesc
{
    std::vector<VkVertexInputBindingDescription> Bindings;
    std::vector<VkVertexInputAttributeDescription> Attributes;
};

class PipelineProfile
{
public:
    VkPrimitiveTopology Topo;

    VkSampleCountFlagBits MsaaSamples;

    VkExtent3D RenderSize;
    VkRect2D RenderOffset;

    std::pair<float, float> DepthRange;
    bool bStencilTesting;
    bool bDepthTesting;
    VkCompareOp DepthCompareOp;

    void AddBinding(uint32_t Binding, size_t Stride, VkVertexInputRate InRate)
    {
        VkVertexInputBindingDescription tmp{Binding, Stride, InRate};

        Bindings.push_back(tmp);
    }

    void AddAttribute(uint32_t Binding, VkFormat Format, uint32_t Location, size_t Offset)
    {
        VkVertexInputAttributeDescription tmp{Location, Binding, Format, Offset};

        Attributes.push_back(tmp);
    }

    VkPipelineMultisampleStateCreateInfo* GetMsaa();
    VkPipelineViewportStateCreateInfo* GetViewport();
    VkPipelineDepthStencilStateCreateInfo* GetDepthStencil();
    VkPipelineVertexInputStateCreateInfo* GetVtxInput();
    VkPipelineInputAssemblyStateCreateInfo* GetAssembly();

private:
    VkPipelineMultisampleStateCreateInfo Msaa{};

    VkViewport Viewport{};
    VkRect2D Rect{};
    VkPipelineViewportStateCreateInfo ViewportState{};

    VkPipelineDepthStencilStateCreateInfo DepthState{};
    VkPipelineVertexInputStateCreateInfo VtxInput{};
    VkPipelineInputAssemblyStateCreateInfo AssemblyState{};

    std::vector<VkVertexInputBindingDescription> Bindings;
    std::vector<VkVertexInputAttributeDescription> Attributes;
};

class Pipeline
{
public:
    void AddAttachmentBlending(VkPipelineColorBlendAttachmentState AttBlend) { OutputBlending.push_back(AttBlend); }
    inline void SetProfile(PipelineProfile PipeProf) { Profile = PipeProf; }

    void Bake(RenderPass rPass, uint32_t Subpass, const char* Vtx, const char* Frag);
    void AddDescriptor(Resources::DescriptorLayout* pDesc) { Descriptors.push_back(pDesc->Layout); }

    VkPipelineLayout PipeLayout;

private:
    PipelineProfile Profile;

    VkPipeline Pipe;

    VkShaderModule VtxShader;
    VkShaderModule FragShader;

    std::vector<VkPipelineColorBlendAttachmentState> OutputBlending;
    std::vector<VkDescriptorSetLayout> Descriptors;
};

struct Context
{
    VkInstance Instance;
    VkPhysicalDevice PhysDevice;
    VkDevice Device;

    uint32_t Local;
    uint32_t Host;

    uint32_t GraphicsFamily;
    VkQueue GraphicsQueue;

    uint32_t ComputeFamily;
    VkQueue ComputeQueue;
};

struct Window
{
public:
    VkExtent2D Resolution;
    VkSurfaceFormatKHR SurfFormat;

    SDL_Window* sdlWindow;

    std::vector<VkImage> SwapchainImages;
    std::vector<VkImageView> SwapchainAttachments;

    VkSurfaceKHR Surface;
    VkSwapchainKHR Swapchain;
};
