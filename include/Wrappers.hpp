#pragma once
#include <iostream>
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
    class Fence
    {
        public:
            Fence() : bInUse(false) {}
            Fence(std::string Name) : bInUse(false), FenceName(Name) {};

            /* Blocks execution until fence is signaled, then resets the fence for future use. */
            void Wait();

            VkFence* GetFence();

            std::string FenceName;

        private:
            VkFence vkFence;
            bool bInUse;
    };

    /*! \brief A wrapper class that stores information about an object's binding, including a pointer to the heap/memory it's bound to */
    class Allocation
    {
    public:
        uint32_t Size;
        uint32_t Offset;

        VkDeviceMemory* pMemory;
    };

    /*! \brief A wrapper around Vulkan Images.
    *   Contains the Image handle, format, resolution, and allocation.
    */
    struct Image
    {
    public:
        ~Image();

        VkImage Img;

        VkFormat Format;
        VkExtent2D Resolution;

        Allocation Alloc;
    };

    /*! \brief A wrapper around vulkan buffers.
    *   Contains the buffer and allocation, as well as a pointer to the memory if it is mapped.
    */
    struct Buffer
    {
    public:
        ~Buffer();

        operator VkBuffer() { return Buff; }
        operator VkBuffer*() { return &Buff; }

        Allocation Alloc;

        void* pData = nullptr;

    private:
        VkBuffer Buff;
    };

    /*! \brief A wrapper around command buffers.
    *   contains the command buffer, a fence, and a pointer to the command pool. Also contains methods for recording.
    */
    class CommandBuffer
    {
    public:
        ~CommandBuffer();

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

        Fence cmdFence;

    private:
        VkCommandPool* pPool;
        VkCommandBuffer cmdBuffer;
        bool bCompute;
    };

    class DescriptorLayout
    {
    public:
        ~DescriptorLayout();

        void AddBinding(VkDescriptorSetLayoutBinding Binding);

        VkDescriptorSetLayout Layout = VK_NULL_HANDLE;

        VkDescriptorSetLayoutBinding* GetBindings(uint32_t& Count) { Count = Bindings.size(); return Bindings.data(); }

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
        ~DescriptorSet();

        VkDescriptorSet DescSet; //! > The vulkan api handle to the allocated descriptor set.
        DescriptorLayout* DescLayout; //! > A pointer to the descriptor layout used to create this descriptor set.

        //! \brief Wraps descriptor writes using a custom struct.
        void Update(DescUpdate* pUpdateInfos, size_t Count);

        VkDescriptorPool* pPool;
    };

    // Wraps FrameBuffer Information and creation
    class FrameBuffer
    {
    public:
        ~FrameBuffer();

        operator VkFramebuffer*() { return &Framebuff; }

        void AddBuffer(VkImageCreateInfo ImgCI, VkImageLayout InitLayout);
        void AddBuffer(VkImageView ImgView, VkImageLayout InitLayout);
        void Bake(VkRenderPass Renderpass, VkCommandBuffer* pCmdBuffer);

    private:
        VkFramebuffer Framebuff;
        std::vector<VkImageCreateInfo> AttachmentInfos;

        std::vector<VkImage> Attachments;
        std::vector<VkDeviceMemory> AttachmentAllocations;
        std::vector<VkImageView> AttachmentViews;
        std::vector<VkImageLayout> AttachmentLayouts;
    };
}

namespace Allocators
{
    class DescriptorPool
    {
    public:
        ~DescriptorPool();

        void Bake(Resources::DescriptorLayout* pLayout, uint32_t SetCount);

        Resources::DescriptorSet* CreateSet();

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
        ~CommandPool();

        void Bake(bool bCompute);
        void Submit(Resources::CommandBuffer* pCmdBuffer, Resources::Fence* pFence, uint32_t SignalSemCount = 0, VkSemaphore* SignalSemaphores = nullptr, uint32_t WaitSemCount = 0, VkSemaphore* WaitSemaphores = nullptr);
        Resources::CommandBuffer* CreateBuffer();

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
public:
    ~RenderPass();

    void AddAttachmentDesc(VkFormat Format, VkImageLayout InitLay, VkImageLayout FinLay, VkAttachmentStoreOp StoreOp, VkAttachmentLoadOp LoadOp, VkAttachmentStoreOp StencilStoreOp, VkAttachmentLoadOp StencilLoadOp, VkClearValue* ClearValue, VkSampleCountFlagBits Samples = VK_SAMPLE_COUNT_1_BIT);
    inline void AddPass(Subpass& sPass) { Subpasses.push_back(sPass); }

    void Bake();

    void Begin(Resources::CommandBuffer& cmdBuffer, Resources::FrameBuffer& FrameBuffer);
    void End(Resources::CommandBuffer& cmdBuffer);
    void NextPass();

    VkRenderPass rPass;

private:
    std::vector<Subpass> Subpasses;
    std::vector<VkAttachmentDescription> Attachments;
    std::vector<VkClearValue> BufferClears;
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
    ~Pipeline();

    operator VkPipeline() { return Pipe; }

    void Bind(VkCommandBuffer* pCmdBuffer);

    void AddAttachmentBlending(VkPipelineColorBlendAttachmentState AttBlend) { OutputBlending.push_back(AttBlend); }
    inline void SetProfile(PipelineProfile PipeProf) { Profile = PipeProf; }

    void Bake(RenderPass* rPass, uint32_t Subpass, const char* Vtx, const char* Frag);
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

class ComputePipeline
{
    public:
        ~ComputePipeline();

        operator VkPipeline() { return Pipe; }

        void Bind(VkCommandBuffer* pCmdBuffer);

        void Bake(const char* Comp);
        void AddDescriptor(Resources::DescriptorLayout* pDesc) { Descriptors.push_back(pDesc->Layout); }

        VkPipelineLayout PipeLayout;

    private:
        VkPipeline Pipe;

        VkShaderModule CompShader;

        std::vector<VkDescriptorSetLayout> Descriptors;
};

struct Context
{
    VkInstance Instance;
    VkPhysicalDevice PhysDevice;
    VkPhysicalDeviceFeatures PhysDeviceFeatures;
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
    uint32_t GetNextFrame(VkSemaphore& Semaphore);
    void PresentFrame(uint32_t FrameIdx, VkSemaphore* pWaitSem = nullptr);

    VkExtent2D Resolution;
    VkSurfaceFormatKHR SurfFormat;

    SDL_Window* sdlWindow;

    std::vector<VkImage> SwapchainImages;
    std::vector<VkImageView> SwapchainAttachments;

    VkSurfaceKHR Surface;
    VkSwapchainKHR Swapchain;
};
