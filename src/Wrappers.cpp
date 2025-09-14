#include "Wrappers.hpp"
#include "Framework.hpp"

#include <iostream>
#include <fstream>
#include <vulkan/vulkan_core.h>

namespace Resources
{
    void Fence::Wait()
    {
        if(bInUse)
        {
          vkWaitForFences(GetContext()->Device, 1, &vkFence, true, UINT64_MAX);
          vkResetFences(GetContext()->Device, 1, &vkFence);
          bInUse = false;
        }
        else
        {
          return;
        }
    }

    VkFence* Fence::GetFence()
    {
        if(bInUse)
        {
            std::cout << "Fence is being obtained, but is currently in use. You should probably reset the fence using Fence::Wait() before reobtaining.\n";
            throw std::runtime_error("");
        }

        bInUse = true;

        return &vkFence;
    }

    Image::Image()
    {

    }

    Image::~Image()
    {
        vkDestroyImageView(GetContext()->Device, View, nullptr);
        vkDestroyImage(GetContext()->Device, Img, nullptr);
    }

    Buffer::Buffer(std::string Name) : Name(Name)
    {

    }
    
    Buffer::~Buffer()
    {
        std::cout << "Destroying Buffer " << Name << '\n';
        vkDestroyBuffer(GetContext()->Device, Buff, nullptr);
    }
    
    CommandBuffer::CommandBuffer()
    {

    }

    CommandBuffer::~CommandBuffer()
    {
        vkFreeCommandBuffers(GetContext()->Device, *pPool, 1, &cmdBuffer);
    }

    void CommandBuffer::Bake(VkCommandPool* pCmdPool)
    {
        VkResult Err;

        VkCommandBufferAllocateInfo AllocInf{};
        AllocInf.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        AllocInf.commandBufferCount = 1;
        AllocInf.commandPool = *pCmdPool;
        AllocInf.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

        Context* pCtx = GetContext();

        if((Err = vkAllocateCommandBuffers(pCtx->Device, &AllocInf, &cmdBuffer)) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to allocate command buffer");
        }

        cmdFence = CreateFence("Commandbuffer Fence");

        pPool = pCmdPool;
    }

    void CommandBuffer::Reset()
    {
        vkResetCommandBuffer(cmdBuffer, 0);
    }

    void CommandBuffer::Start()
    {
        VkResult Err;

        VkCommandBufferBeginInfo BegInf{};
        BegInf.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if((Err = vkBeginCommandBuffer(cmdBuffer, &BegInf)) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to start command buffer recording\n");
        }
    }

    void CommandBuffer::Stop()
    {
        vkEndCommandBuffer(cmdBuffer);
    }

    DescriptorLayout::~DescriptorLayout()
    {
        vkDestroyDescriptorSetLayout(GetContext()->Device, Layout, nullptr);
    }

    void DescriptorLayout::AddBinding(VkDescriptorSetLayoutBinding Binding)
    {
        Bindings.push_back(Binding);

        VkDescriptorSetLayoutCreateInfo LayCI{};
        LayCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        LayCI.bindingCount = (uint32_t)Bindings.size();
        LayCI.pBindings = Bindings.data();

        Context* pCtx = GetContext();

        if(Layout != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorSetLayout(pCtx->Device, Layout, nullptr);
        }

        vkCreateDescriptorSetLayout(pCtx->Device, &LayCI, nullptr, &Layout);
    }

    DescriptorSet::~DescriptorSet()
    {
        vkFreeDescriptorSets(GetContext()->Device, *pPool, 1, &DescSet);
    }

    void DescriptorSet::Update(DescUpdate* pUpdateInfos, size_t Count)
    {
        if(DescSet== VK_NULL_HANDLE)
        {
            throw std::runtime_error("failed to update descriptor set : Descriptor set is NULL\n");
        }

        std::vector<VkWriteDescriptorSet> WriteInfos = {};
        std::vector<VkDescriptorBufferInfo> WriteBuffers = {};

        for(uint32_t i = 0; i < Count; i++)
        {
            VkWriteDescriptorSet WriteInfo{};
            WriteInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            WriteInfo.descriptorType = pUpdateInfos[i].DescType;
            WriteInfo.descriptorCount = pUpdateInfos[i].DescCount;
            WriteInfo.dstArrayElement = pUpdateInfos[i].DescIndex;
            WriteInfo.dstBinding = pUpdateInfos[i].Binding;
            WriteInfo.dstSet = DescSet;

            VkDescriptorBufferInfo BuffInfo{};
            BuffInfo.buffer = *pUpdateInfos[i].pBuff;
            BuffInfo.offset = pUpdateInfos[i].Offset;
            BuffInfo.range = pUpdateInfos[i].Range;

            WriteInfos.push_back(WriteInfo);
            WriteBuffers.push_back(BuffInfo);
        }

        for(uint32_t i = 0; i < WriteInfos.size(); i++)
        {
            WriteInfos[i].pBufferInfo = &WriteBuffers[i];
        }

        vkUpdateDescriptorSets(GetContext()->Device, (uint32_t)WriteInfos.size(), WriteInfos.data(), 0, nullptr);
    }

    FrameBuffer::~FrameBuffer()
    {
        vkDestroyFramebuffer(GetContext()->Device, Framebuff, nullptr);
    }

    void FrameBuffer::AddBuffer(VkImageCreateInfo ImgInf, VkImageLayout InitLayout)
    {
        AttachmentInfos.push_back(ImgInf);
        AttachmentLayouts.push_back(InitLayout);
    }

    void FrameBuffer::AddBuffer(VkImageView ImgView, VkImageLayout InitLayout)
    {
        AttachmentViews.push_back(ImgView);
        AttachmentLayouts.push_back(InitLayout);
    }

    void FrameBuffer::Bake(VkRenderPass Renderpass, VkCommandBuffer* pCmdBuffer, bool bHasSwapImg)
    {
        VkResult Err;

        Context* pCtx = GetContext();

        for(uint32_t i = 0; i < AttachmentInfos.size(); i++)
        {
            VkImage Tmp;
            VkDeviceMemory TmpMem;

            if((Err = vkCreateImage(pCtx->Device, &AttachmentInfos[i], nullptr, &Tmp)) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create Attachment for framebuffer.");
            }

            VkMemoryRequirements MemReq;
            vkGetImageMemoryRequirements(GetContext()->Device, Tmp, &MemReq);

            VkMemoryAllocateInfo Allocinfo{};
            Allocinfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            Allocinfo.memoryTypeIndex = GetContext()->Local;
            Allocinfo.allocationSize = MemReq.size;

            if((Err = vkAllocateMemory(GetContext()->Device, &Allocinfo, nullptr, &TmpMem)) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to allocate framebuffer allocation");
            }

            if((Err = vkBindImageMemory(GetContext()->Device, Tmp, TmpMem, 0)) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to bind framebuffer memory");
            }

            AttachmentAllocations.push_back(TmpMem);
            Attachments.push_back(Tmp);
        }

        std::vector<VkImageMemoryBarrier> MemoryBarriers = {};

        for(uint32_t i = 0; i < AttachmentInfos.size(); i++)
        {
            VkImageView tmpView;
            
            uint32_t Iter = i+1;

            VkImageViewCreateInfo ViewCI{};
            ViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            ViewCI.format = AttachmentInfos[i].format;
            ViewCI.components.r = VK_COMPONENT_SWIZZLE_R;
            ViewCI.components.g = VK_COMPONENT_SWIZZLE_G;
            ViewCI.components.b = VK_COMPONENT_SWIZZLE_B;
            ViewCI.components.a = VK_COMPONENT_SWIZZLE_A;
            ViewCI.image = Attachments[i];
            ViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
            ViewCI.subresourceRange.baseArrayLayer = 0;
            ViewCI.subresourceRange.baseMipLevel = 0;
            ViewCI.subresourceRange.levelCount = 1;
            ViewCI.subresourceRange.layerCount = 1;

            if(AttachmentInfos[i].format == VK_FORMAT_D16_UNORM || AttachmentInfos[i].format == VK_FORMAT_D16_UNORM_S8_UINT || AttachmentInfos[i].format == VK_FORMAT_D32_SFLOAT || AttachmentInfos[i].format == VK_FORMAT_D32_SFLOAT_S8_UINT)
            {
                ViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            }
            else
            {
                ViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            }

            if((Err = vkCreateImageView(pCtx->Device, &ViewCI, nullptr, &tmpView)) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create image view");
            }

            AttachmentViews.push_back(tmpView);

            MemoryBarriers.push_back({});
            MemoryBarriers[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            MemoryBarriers[i].image = Attachments[i];
            MemoryBarriers[i].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            MemoryBarriers[i].newLayout = AttachmentLayouts[Iter];
            MemoryBarriers[i].srcAccessMask = VK_ACCESS_NONE;
            MemoryBarriers[i].dstAccessMask = VK_ACCESS_NONE;

            MemoryBarriers[i].subresourceRange.aspectMask = ViewCI.subresourceRange.aspectMask;
            MemoryBarriers[i].subresourceRange.baseArrayLayer = 0;
            MemoryBarriers[i].subresourceRange.baseMipLevel = 0;
            MemoryBarriers[i].subresourceRange.layerCount = 1;
            MemoryBarriers[i].subresourceRange.levelCount = 1;
        }

        VkFramebufferCreateInfo FbCI{};
        FbCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        FbCI.attachmentCount = (uint32_t)AttachmentViews.size();
        FbCI.pAttachments = AttachmentViews.data();
        FbCI.layers = 1;
        FbCI.width = AttachmentInfos[0].extent.width;
        FbCI.height = AttachmentInfos[0].extent.height;
        FbCI.renderPass = Renderpass;

        if((Err = vkCreateFramebuffer(pCtx->Device, &FbCI, nullptr, &Framebuff)) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create framebuffer");
        }

        vkCmdPipelineBarrier(*pCmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, (uint32_t)MemoryBarriers.size(), MemoryBarriers.data());
    }
}


namespace Allocators
{
    DescriptorPool::~DescriptorPool()
    {
        vkDestroyDescriptorPool(GetContext()->Device, DescPool, nullptr);
    }

    void DescriptorPool::Bake(Resources::DescriptorLayout* pLayout, uint32_t SetCount)
    {
        VkResult Err;

        DescLayout = pLayout;

        uint32_t SizeCount;
        VkDescriptorSetLayoutBinding* BindingSizes = DescLayout->GetBindings(SizeCount);
        VkDescriptorPoolSize PoolSizes[SizeCount];

        for(uint32_t i = 0; i < SizeCount; i++)
        {
            PoolSizes[i].type = BindingSizes[i].descriptorType;
            PoolSizes[i].descriptorCount = BindingSizes[i].descriptorCount * SetCount;
        }

        VkDescriptorPoolCreateInfo PoolCI{};
        PoolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        PoolCI.maxSets = SetCount;
        PoolCI.poolSizeCount = SizeCount;
        PoolCI.pPoolSizes = PoolSizes;

        if((Err = vkCreateDescriptorPool(GetContext()->Device, &PoolCI, nullptr, &DescPool)) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create descriptor pool");
        }
    }

    Resources::DescriptorSet* DescriptorPool::CreateSet()
    {
        VkResult Err;

        Resources::DescriptorSet* Ret = new Resources::DescriptorSet();

        VkDescriptorSetAllocateInfo AllocInfo{};
        AllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        AllocInfo.descriptorPool = DescPool;
        AllocInfo.descriptorSetCount = 1;
        AllocInfo.pSetLayouts = *DescLayout;

        if((Err = vkAllocateDescriptorSets(GetContext()->Device, &AllocInfo, &Ret->DescSet)) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to allocate descriptor sets");
        }

        Ret->DescLayout = DescLayout;

        Ret->pPool = &DescPool;

        return Ret;
    }

    CommandPool::~CommandPool()
    {
        vkDestroyCommandPool(GetContext()->Device, cmdPool, nullptr);
    }

    void CommandPool::Bake(CommandType cmdType)
    {
        VkResult Err;

        Context* pCtx = GetContext();

        VkCommandPoolCreateInfo PoolCI{};
        PoolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        PoolCI.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        if(cmdType == CommandType::eCmdGraphics)
        {
            PoolCI.queueFamilyIndex = pCtx->GraphicsFamily;
        }
        else if(cmdType == CommandType::eCmdCompute)
        {
            PoolCI.queueFamilyIndex = pCtx->ComputeFamily;
        }
        else if(cmdType == CommandType::eCmdTransfer)
        {
            PoolCI.queueFamilyIndex = pCtx->TransferFamily;
        }

        if((Err = vkCreateCommandPool(pCtx->Device, &PoolCI, nullptr, &cmdPool)) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create command pool");
        }

        PoolType = cmdType;
    }

    void CommandPool::Submit(Resources::CommandBuffer* pCmdBuffer, uint32_t SignalSemCount, VkSemaphore* SignalSemaphores, uint32_t WaitSemCount, VkSemaphore* WaitSemaphores, VkPipelineStageFlagBits WaitStages)
    {
        VkResult Err;

        VkPipelineStageFlags Wait = WaitStages;

        VkSubmitInfo SubInf{};
        SubInf.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        SubInf.commandBufferCount = 1;
        SubInf.pCommandBuffers = *pCmdBuffer;
        SubInf.pWaitDstStageMask = &Wait;
        SubInf.signalSemaphoreCount = SignalSemCount;
        SubInf.pSignalSemaphores = SignalSemaphores;
        SubInf.waitSemaphoreCount = WaitSemCount;
        SubInf.pWaitSemaphores = WaitSemaphores;

        VkQueue* pQueue;

        if(PoolType == CommandType::eCmdGraphics)
        {
            pQueue = &(GetContext()->GraphicsQueue);
        }
        else if(PoolType == CommandType::eCmdCompute)
        {
            pQueue = &(GetContext()->ComputeQueue);
        }
        else if(PoolType == CommandType::eCmdTransfer)
        {
            pQueue = &(GetContext()->TransferQueue);
        }
        else
        {
            throw std::runtime_error("Failed to submit command buffer to pool due to uknown command type.\n");
        }

        if((Err = vkQueueSubmit(*pQueue, 1, &SubInf, *(pCmdBuffer->cmdFence->GetFence()))) != VK_SUCCESS)
        {
            printf("Error : %i", Err);
            throw std::runtime_error("Failed to submit a command buffer with error ");
        }
    }

    Resources::CommandBuffer* CommandPool::CreateBuffer()
    {
        Resources::CommandBuffer* Ret = new Resources::CommandBuffer();
        Ret->Bake(&cmdPool);

        Ret->cmdFence = CreateFence();

        return Ret;
    }
}


RenderPass::~RenderPass()
{
    vkDestroyRenderPass(GetContext()->Device, rPass, nullptr);
}

void RenderPass::AddAttachmentDesc(VkFormat Format, VkImageLayout InitLay, VkImageLayout FinLay, VkAttachmentStoreOp StoreOp, VkAttachmentLoadOp LoadOp, VkAttachmentStoreOp StencilStoreOp, VkAttachmentLoadOp StencilLoadOp, VkClearValue* ClearValue, VkSampleCountFlagBits Samples)
{
    VkAttachmentDescription tmp{};
    tmp.samples = VK_SAMPLE_COUNT_1_BIT;
    tmp.format = Format;
    tmp.initialLayout = InitLay;
    tmp.finalLayout = FinLay;
    tmp.storeOp = StoreOp;
    tmp.loadOp = LoadOp;
    tmp.stencilStoreOp = StencilStoreOp;
    tmp.stencilLoadOp = StencilLoadOp;

    if(ClearValue != nullptr)
    {
        BufferClears.push_back(*ClearValue);
    }

    Attachments.push_back(tmp);
}

void RenderPass::Bake()
{
    VkResult Err;

    VkSubpassDescription sPasses[Subpasses.size()];

    Context* pCtx = GetContext();

    for(uint32_t i = 0; i < Subpasses.size(); i++)
    {
        sPasses[i] = {};

        uint32_t ColorAttCount;
        uint32_t InputAttCount;
        uint32_t ResolveAttCount;
        uint32_t PreserveAttCount;

        sPasses[i].pColorAttachments = Subpasses[i].GetColorAttachments(ColorAttCount);
        sPasses[i].colorAttachmentCount = ColorAttCount;
        sPasses[i].pInputAttachments = Subpasses[i].GetInputAttachments(InputAttCount);
        sPasses[i].inputAttachmentCount = InputAttCount;
        sPasses[i].pResolveAttachments = Subpasses[i].GetResolveAttachments(ResolveAttCount);
        sPasses[i].pDepthStencilAttachment = Subpasses[i].GetDepthAttachment();
        sPasses[i].pPreserveAttachments = Subpasses[i].GetPreserveAttachments(PreserveAttCount);
        sPasses[i].preserveAttachmentCount = PreserveAttCount;
    }

    VkRenderPassCreateInfo RpCI{};
    RpCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    RpCI.attachmentCount = (uint32_t)Attachments.size();
    RpCI.pAttachments = Attachments.data();
    RpCI.subpassCount = (uint32_t)Subpasses.size();
    RpCI.pSubpasses = sPasses;

    if((Err = vkCreateRenderPass(pCtx->Device, &RpCI, nullptr, &rPass)) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create renderpass");
    }
}

void RenderPass::Begin(Resources::CommandBuffer& cmdBuffer, Resources::FrameBuffer& FrameBuffer)
{
    VkRenderPassBeginInfo BeginInf{};
    BeginInf.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    BeginInf.renderPass = rPass;
    BeginInf.clearValueCount = (uint32_t)BufferClears.size();
    BeginInf.pClearValues = BufferClears.data();
    BeginInf.renderArea.extent = GetWindow()->Resolution;
    BeginInf.renderArea.offset = {0, 0};
    BeginInf.framebuffer = *(VkFramebuffer*)FrameBuffer;

    vkCmdBeginRenderPass(cmdBuffer, &BeginInf, VK_SUBPASS_CONTENTS_INLINE);
}

void RenderPass::End(Resources::CommandBuffer& cmdBuffer)
{
    vkCmdEndRenderPass(cmdBuffer);
}



VkPipelineMultisampleStateCreateInfo* PipelineProfile::GetMsaa()
{
    Msaa.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    Msaa.sampleShadingEnable = VK_FALSE;
    Msaa.rasterizationSamples = MsaaSamples;

    return &Msaa;
}

VkPipelineViewportStateCreateInfo* PipelineProfile::GetViewport()
{
    Rect.extent = {RenderSize.width, RenderSize.height};
    Rect.offset = RenderOffset.offset;

    Viewport.width = RenderSize.width;
    Viewport.height = RenderSize.height;
    Viewport.minDepth = DepthRange.first;
    Viewport.maxDepth = DepthRange.second;
    Viewport.x = RenderOffset.offset.x;
    Viewport.y = RenderOffset.offset.y;

    ViewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    ViewportState.scissorCount = 1;
    ViewportState.pScissors = &Rect;
    ViewportState.viewportCount = 1;
    ViewportState.pViewports = &Viewport;

    return &ViewportState;
}

VkPipelineDepthStencilStateCreateInfo* PipelineProfile::GetDepthStencil()
{
    DepthState = {};

    DepthState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    DepthState.pNext = nullptr;
    DepthState.minDepthBounds = DepthRange.first;
    DepthState.maxDepthBounds = DepthRange.second;
    DepthState.stencilTestEnable = bStencilTesting;
    DepthState.depthTestEnable = bDepthTesting;
    DepthState.depthWriteEnable = VK_TRUE;
    DepthState.depthCompareOp = DepthCompareOp;
    DepthState.back = {};
    DepthState.front = {};

    return &DepthState;
}

VkPipelineVertexInputStateCreateInfo* PipelineProfile::GetVtxInput()
{
    VtxInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VtxInput.vertexAttributeDescriptionCount = (uint32_t)Attributes.size();
    VtxInput.pVertexAttributeDescriptions = Attributes.data();
    VtxInput.vertexBindingDescriptionCount = (uint32_t)Bindings.size();
    VtxInput.pVertexBindingDescriptions = Bindings.data();

    return &VtxInput;
}

VkPipelineInputAssemblyStateCreateInfo* PipelineProfile::GetAssembly()
{
    AssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    AssemblyState.primitiveRestartEnable = VK_FALSE;
    AssemblyState.topology = Topo;

    return &AssemblyState;
}



Pipeline::~Pipeline()
{
    Context* pCtx = GetContext();

    vkDestroyPipelineLayout(pCtx->Device, PipeLayout, nullptr);
    vkDestroyShaderModule(pCtx->Device, VtxShader, nullptr);
    vkDestroyShaderModule(pCtx->Device, FragShader, nullptr);
    vkDestroyPipeline(pCtx->Device, Pipe, nullptr);
}

void Pipeline::Bake(RenderPass* rPass, uint32_t Subpass, const char* Vtx, const char* Frag)
{
    VkResult Err;

    Context* pCtx = GetContext();

    std::string VtxPath = shader_path;
    VtxPath += Vtx;

    std::ifstream cVtx(VtxPath, std::ios::binary);

    if(!cVtx.is_open())
    {
        throw std::runtime_error("Failed to open vertex shader to open pipeline");
    }

    cVtx.seekg(0, cVtx.end);
    size_t VtxSize = cVtx.tellg();
    cVtx.seekg(0, cVtx.beg);

    uint32_t VtxCodeSize = (uint32_t)VtxSize/4;
    if(VtxCodeSize*VtxSize < 4) VtxCodeSize++; // ceil if needed.

    uint32_t* VtxCode = new uint32_t[VtxCodeSize];

    cVtx.read(reinterpret_cast<char*>(VtxCode), VtxSize);

    VkShaderModuleCreateInfo VtxCI{};
    VtxCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    VtxCI.codeSize = VtxSize;
    VtxCI.pCode = VtxCode;

    if((Err = vkCreateShaderModule(pCtx->Device, &VtxCI, nullptr, &VtxShader)) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create shader module for pipeline");
    }
    
    delete[] VtxCode;

    std::string FragPath = shader_path;
    FragPath += Frag;

    std::ifstream cFrag(FragPath, std::ios::binary);
    
    if(!cFrag.is_open())
    {
        throw std::runtime_error("Failed to open Fragment shader to open pipeline");
    }

    cFrag.seekg(0, cFrag.end);
    size_t FragSize = cFrag.tellg();
    cFrag.seekg(0, cFrag.beg);

    uint32_t FragCodeSize = (uint32_t)FragSize/sizeof(uint8_t);
    if(FragCodeSize*FragSize < 4) FragCodeSize++; // ceil if needed.

    uint32_t* FragCode = new uint32_t[FragCodeSize];

    cFrag.read(reinterpret_cast<char*>(FragCode), FragSize);

    VkShaderModuleCreateInfo FragCI{};
    FragCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    FragCI.codeSize = FragSize;
    FragCI.pCode = FragCode;

    if((Err = vkCreateShaderModule(pCtx->Device, &FragCI, nullptr, &FragShader)) != VK_SUCCESS)
    {
        printf("Error : %i", Err);
        throw std::runtime_error("Failed to create shader module for pipeline");
    }
    
    delete[] FragCode;
    
    VkPipelineShaderStageCreateInfo ShaderStages[2] = {};

    ShaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    ShaderStages[0].module = VtxShader;
    ShaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    ShaderStages[0].pName = "main";

    ShaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    ShaderStages[1].module = FragShader;
    ShaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    ShaderStages[1].pName = "main";

    VkPipelineRasterizationStateCreateInfo RasterState{};
    RasterState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    RasterState.lineWidth = 1.f;
    RasterState.cullMode = VK_CULL_MODE_BACK_BIT;
    RasterState.polygonMode = VK_POLYGON_MODE_FILL;
    RasterState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    RasterState.depthClampEnable = VK_FALSE;
    RasterState.depthBiasEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo BlendState{};
    BlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    BlendState.attachmentCount = (uint32_t)OutputBlending.size();
    BlendState.pAttachments = OutputBlending.data();
    BlendState.logicOpEnable = VK_FALSE;

    VkPipelineLayoutCreateInfo LayCI{};
    LayCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    LayCI.setLayoutCount = (uint32_t)Descriptors.size();
    LayCI.pSetLayouts = Descriptors.data();

    if((Err = vkCreatePipelineLayout(pCtx->Device, &LayCI, nullptr, &PipeLayout)) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create pipeline layout");
    }

    VkGraphicsPipelineCreateInfo PipeCI{};
    PipeCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    PipeCI.layout = PipeLayout;
    PipeCI.pMultisampleState = Profile.GetMsaa();
    PipeCI.pViewportState = Profile.GetViewport();
    PipeCI.pDepthStencilState = Profile.GetDepthStencil();
    PipeCI.pVertexInputState = Profile.GetVtxInput();
    PipeCI.pInputAssemblyState = Profile.GetAssembly();
    PipeCI.pRasterizationState = &RasterState;
    PipeCI.pColorBlendState = &BlendState;
    PipeCI.stageCount = 2;
    PipeCI.pStages = ShaderStages;
    PipeCI.renderPass = rPass->rPass;
    PipeCI.subpass = Subpass;

    if((Err = vkCreateGraphicsPipelines(pCtx->Device, VK_NULL_HANDLE, 1, &PipeCI, nullptr, &Pipe)) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create pipeline");
    }
}

void Pipeline::Bind(VkCommandBuffer* pCmdBuffer)
{
    vkCmdBindPipeline(*pCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipe);
}



ComputePipeline::~ComputePipeline()
{
    vkDestroyShaderModule(GetContext()->Device, CompShader, nullptr);
    vkDestroyPipelineLayout(GetContext()->Device, PipeLayout, nullptr);
    vkDestroyPipeline(GetContext()->Device, Pipe, nullptr);
}

void ComputePipeline::Bake(const char* Comp)
{
    VkResult Err;

    Context* pCtx = GetContext();

    std::string CompPath = shader_path;
    CompPath += Comp;
    vkDestroyPipeline(GetContext()->Device, Pipe, nullptr);

    std::ifstream cComp(CompPath, std::ios::binary);

    if(!cComp.is_open())
    {
        throw std::runtime_error("Failed to open vertex shader to open pipeline");
    }

    cComp.seekg(0, cComp.end);
    size_t CompSize = cComp.tellg();
    cComp.seekg(0, cComp.beg);

    uint32_t CompCodeSize = (uint32_t)CompSize/4;
    if(CompCodeSize*CompSize < 4) CompCodeSize++; // ceil if needed.

    uint32_t* CompCode = new uint32_t[CompCodeSize];

    cComp.read(reinterpret_cast<char*>(CompCode), CompSize);

    VkShaderModuleCreateInfo CompCI{};
    CompCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    CompCI.codeSize = CompSize;
    CompCI.pCode = CompCode;

    if((Err = vkCreateShaderModule(pCtx->Device, &CompCI, nullptr, &CompShader)) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create shader module for pipeline");
    }

    VkPipelineShaderStageCreateInfo ShaderStage{};
    ShaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    ShaderStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    ShaderStage.pName = "main";
    ShaderStage.module = CompShader;

    VkPipelineLayoutCreateInfo LayCI{};
    LayCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    LayCI.setLayoutCount = (uint32_t)Descriptors.size();
    LayCI.pSetLayouts = Descriptors.data();

    if((Err = vkCreatePipelineLayout(pCtx->Device, &LayCI, nullptr, &PipeLayout)) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create pipeline layout");
    }

    VkComputePipelineCreateInfo CompPipeCI{};
    CompPipeCI.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    CompPipeCI.layout = PipeLayout;
    CompPipeCI.stage = ShaderStage;

    if((Err = vkCreateComputePipelines(pCtx->Device, VK_NULL_HANDLE, 1, &CompPipeCI, nullptr, &Pipe)) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create compute pipeline\n");
    }
}



uint32_t Window::GetNextFrame(Resources::Fence* pFence, VkSemaphore* pSemaphore)
{
    uint32_t Ret;

    vkAcquireNextImageKHR(GetContext()->Device, Swapchain, UINT64_MAX, (pSemaphore == nullptr) ? nullptr : *pSemaphore, (pFence == nullptr) ? 0 : *pFence->GetFence(), &Ret);

    return Ret;
}

void Window::PresentFrame(uint32_t FrameIdx, VkSemaphore* pWaitSem)
{
    VkPresentInfoKHR PresInf{};
    PresInf.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    PresInf.swapchainCount = 1;
    PresInf.pSwapchains = &Swapchain;
    PresInf.pImageIndices = &FrameIdx;

    if(pWaitSem != nullptr)
    {
        PresInf.waitSemaphoreCount = 1;
        PresInf.pWaitSemaphores = pWaitSem;
    }

    vkQueuePresentKHR(GetContext()->GraphicsQueue, &PresInf);
}
