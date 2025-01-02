#pragma once

#include <vector>
#include <string>

#include "SDL3/SDL.h"
#include "SDL3/SDL_vulkan.h"

#include "vulkan/vulkan.h"
#include "glm/glm.hpp"

namespace Wrappers
{
    struct PipeLocation
    {
        VkAccessFlagBits Access;
        VkPipelineStageFlagBits Stage;
    };

    namespace Resources
    {
        class Allocation
        {
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

        private:
            VkCommandBuffer cmdBuffer;
        };

        class DescriptorLayout
        {
        public:
            void AddBinding(VkDescriptorSetLayoutBinding Binding);

            VkDescriptorSetLayout Layout;

        private:
            std::vector<VkDescriptorSetLayoutBinding> Bindings;
        };

        class DescriptorSet
        {
            VkDescriptorSet DescSet;
            DescriptorLayout* DescLayout;
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
            std::vector<VkImageView> AttachmentViews;
        };
    }

    namespace Allocators
    {
        class DescriptorPool
        {
            void bake();
            Resources::DescriptorSet CreateSet(Resources::DescriptorLayout Layout);

            VkDescriptorPool DescPool;

            uint32_t AllocatedSets;
            size_t MaxSets;

            private:
            std::vector<VkDescriptorPoolSize> Sizes;
        };

        class CommandPool
        {
            Resources::CommandBuffer* CreateBuffer();

            VkCommandPool cmdPool;
        };
    }

    struct Subpass
    {
        void AddAttachment(uint32_t AttIdx, VkImageLayout ImgLayout);

        VkSubpassDescription SubpassDesc;
        std::vector<VkAttachmentReference> Attachments;
    };

    struct RenderPass
    {
        void AddAttachmentDesc(VkFormat Format, VkImageLayout InitLay, VkImageLayout FinLay, VkAttachmentStoreOp StoreOp, VkAttachmentLoadOp LoadOp, VkAttachmentStoreOp StencilStoreOp, VkAttachmentLoadOp StencilLoadOp, VkSampleCountFlagBits Samples = VK_SAMPLE_COUNT_1_BIT);

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

        VkPipelineMultisampleStateCreateInfo* GetMsaa();
        VkPipelineViewportStateCreateInfo* GetViewport();
        VkPipelineDepthStencilStateCreateInfo* GetDepthStencil();
        VkPipelineInputAssemblyStateCreateInfo* GetAssembly();

    private:
        VkPipelineMultisampleStateCreateInfo Msaa;

        VkViewport Viewport;
        VkRect2D Rect;
        VkPipelineViewportStateCreateInfo ViewportState;

        VkPipelineDepthStencilStateCreateInfo DepthState;
        VkPipelineInputAssemblyStateCreateInfo AssemblyState;
    };

    class Pipeline
    {
        void AddAttachmentBlending(VkPipelineColorBlendAttachmentState AttBlend) { OutputBlending.push_back(AttBlend); }
        inline void SetProfile(PipelineProfile PipeProf) { Profile = PipeProf; }

        void Bake(RenderPass rPass, uint32_t Subpass, std::string& Vtx, std::string& Frag);
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

        VkQueue GraphicsQueue;
        VkQueue ComputeQueue;
        

        private:
            struct MemoryHeap
            {
                size_t Size;
                size_t Available;

                uint32_t AllocTail;

                VkDeviceMemory Memory;
            };

            std::vector<MemoryHeap> HostHeaps; // host visible memory heaps
            std::vector<MemoryHeap> LocalHeaps; // device local memory heaps
    };

    struct Window
    {
        VkExtent2D Resolution;
        VkSurfaceFormatKHR SurfFormat;

        SDL_Window* sdlWindow;
        VkSurfaceKHR Surface;
        VkSwapchainKHR Swapchain;
        std::vector<VkImage> SwpImages;
    };

    class AllocatorInterface
    {
    public:
        virtual VkResult Allocate(Resources::Image& Image, Resources::Allocation* pAllocation, bool bVisible = true) = 0;
        virtual VkResult Allocate(Resources::Buffer& Buffer, Resources::Allocation* pAllocation, bool bVisible = true) = 0;

        virtual void* Map(Resources::Allocation* pAllocation) = 0;
        virtual void Unmap(Resources::Allocation* pAllocation) = 0;
    };
}
