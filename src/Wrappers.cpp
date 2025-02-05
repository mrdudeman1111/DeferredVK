#include "Framework.hpp"

#include <iostream>
#include <fstream>

namespace Resources
{
    void DescriptorLayout::AddBinding(VkDescriptorSetLayoutBinding Binding)
    {
        Bindings.push_back(Binding);

        VkDescriptorSetLayoutCreateInfo LayCI{};
        LayCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        LayCI.bindingCount = Bindings.size();
        LayCI.pBindings = Bindings.data();

        Context* pCtx = GetContext();

        if(Layout != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorSetLayout(pCtx->Device, Layout, nullptr);
        }

        vkCreateDescriptorSetLayout(pCtx->Device, &LayCI, nullptr, &Layout);
    }

    void CommandBuffer::Bake(VkCommandPool* pCmdPool, bool bComp)
    {
        VkResult Err;

        VkCommandBufferAllocateInfo AllocInf{};
        AllocInf.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        AllocInf.commandBufferCount = 1;
        AllocInf.commandPool = *pCmdPool;
        AllocInf.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

        if((Err = vkAllocateCommandBuffers(GetContext()->Device, &AllocInf, &cmdBuffer)) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to allocate command buffer");
        }

        bCompute = bComp;
    }

    void CommandBuffer::Start()
    {
        VkCommandBufferBeginInfo BegInf{};
        BegInf.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        vkBeginCommandBuffer(cmdBuffer, &BegInf);
    }

    void CommandBuffer::Stop()
    {
        vkEndCommandBuffer(cmdBuffer);
    }

    void DescriptorSet::Update(DescUpdate UpdateInfo)
    {
        VkWriteDescriptorSet WriteInfo{};
        WriteInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        WriteInfo.descriptorType = UpdateInfo.DescType;
        WriteInfo.descriptorCount = UpdateInfo.DescCount;
        WriteInfo.dstArrayElement = UpdateInfo.DescIndex;
        WriteInfo.dstBinding = UpdateInfo.Binding;
        WriteInfo.dstSet = DescSet;

        VkDescriptorBufferInfo BuffInfo{};
        BuffInfo.buffer = UpdateInfo.pBuff->Buff;
        BuffInfo.offset = UpdateInfo.Offset;
        BuffInfo.range = UpdateInfo.Range;

        WriteInfo.pBufferInfo = &BuffInfo;

        vkUpdateDescriptorSets(GetContext()->Device, 1, &WriteInfo, 0, nullptr);
    }

    void FrameBuffer::AddBuffer(VkImageCreateInfo ImgInf)
    {
        AttachmentInfos.push_back(ImgInf);
    }

    void FrameBuffer::AddBuffer(VkImageView ImgView)
    {
        AttachmentViews.push_back(ImgView);
    }

    void FrameBuffer::Bake(VkRenderPass Renderpass)
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

        for(uint32_t i = 0; i < AttachmentInfos.size(); i++)
        {
            VkImageView tmpView;

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
        }

        VkFramebufferCreateInfo FbCI{};
        FbCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        FbCI.attachmentCount = AttachmentViews.size();
        FbCI.pAttachments = AttachmentViews.data();
        FbCI.layers = 1;
        FbCI.width = AttachmentInfos[0].extent.width;
        FbCI.height = AttachmentInfos[0].extent.height;
        FbCI.renderPass = Renderpass;

        if((Err = vkCreateFramebuffer(pCtx->Device, &FbCI, nullptr, &Framebuff)) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create framebuffer");
        }
    }
}

namespace Allocators
{
    void DescriptorPool::Bake(uint32_t SetCount)
    {
        VkResult Err;

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

    Resources::DescriptorSet DescriptorPool::CreateSet()
    {
        VkResult Err;

        Resources::DescriptorSet Ret;

        VkDescriptorSetAllocateInfo AllocInfo{};
        AllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        AllocInfo.descriptorPool = DescPool;
        AllocInfo.descriptorSetCount = 1;
        AllocInfo.pSetLayouts = &DescLayout->Layout;

        if((Err = vkAllocateDescriptorSets(GetContext()->Device, &AllocInfo, &Ret.DescSet)) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to allocate descriptor sets");
        }

        Ret.DescLayout = DescLayout;

        return Ret;
    }

    void CommandPool::Bake(bool bCompute)
    {
        VkResult Err;

        Context* pCtx = GetContext();

        VkCommandPoolCreateInfo PoolCI{};
        PoolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        PoolCI.queueFamilyIndex = (bCompute) ? pCtx->ComputeFamily : pCtx->GraphicsFamily;

        if((Err = vkCreateCommandPool(pCtx->Device, &PoolCI, nullptr, &cmdPool)) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create command pool");
        }

        bCompute = bCompute;
    }

    void CommandPool::Submit(Resources::CommandBuffer* pCmdBuffer, VkFence* pFence, uint32_t SignalSemCount, VkSemaphore* SignalSemaphores, uint32_t WaitSemCount, VkSemaphore* WaitSemaphores)
    {
        VkResult Err;

        VkSubmitInfo SubInf{};
        SubInf.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        SubInf.commandBufferCount = 1;
        SubInf.pCommandBuffers = *pCmdBuffer;
        SubInf.signalSemaphoreCount = SignalSemCount;
        SubInf.pSignalSemaphores = SignalSemaphores;
        SubInf.waitSemaphoreCount = WaitSemCount;
        SubInf.pWaitSemaphores = WaitSemaphores;

        if(pFence != nullptr)
        {
            vkQueueSubmit(GetContext()->GraphicsQueue, 1, &SubInf, *pFence);
        }
        else
        {
            vkQueueSubmit(GetContext()->GraphicsQueue, 1, &SubInf, VK_NULL_HANDLE);
        }
    }

    Resources::CommandBuffer CommandPool::CreateBuffer()
    {
        Resources::CommandBuffer Ret;
        Ret.Bake(&cmdPool, false);

        VkFenceCreateInfo FenceCI{};
        FenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

        vkCreateFence(GetContext()->Device, &FenceCI, nullptr, &Ret.Fence);

        return Ret;
    }
}

void RenderPass::AddAttachmentDesc(VkFormat Format, VkImageLayout InitLay, VkImageLayout FinLay, VkAttachmentStoreOp StoreOp, VkAttachmentLoadOp LoadOp, VkAttachmentStoreOp StencilStoreOp, VkAttachmentLoadOp StencilLoadOp, VkSampleCountFlagBits Samples)
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
    RpCI.attachmentCount = Attachments.size();
    RpCI.pAttachments = Attachments.data();
    RpCI.subpassCount = Subpasses.size();
    RpCI.pSubpasses = sPasses;

    if((Err = vkCreateRenderPass(pCtx->Device, &RpCI, nullptr, &rPass)) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create renderpass");
    }
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
    DepthState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    DepthState.minDepthBounds = DepthRange.first;
    DepthState.maxDepthBounds = DepthRange.second;
    DepthState.stencilTestEnable = bStencilTesting;
    DepthState.depthTestEnable = bDepthTesting;
    DepthState.depthWriteEnable = VK_TRUE;

    return &DepthState;
}

VkPipelineVertexInputStateCreateInfo* PipelineProfile::GetVtxInput()
{
    VtxInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VtxInput.vertexAttributeDescriptionCount = Attributes.size();
    VtxInput.pVertexAttributeDescriptions = Attributes.data();
    VtxInput.vertexBindingDescriptionCount = Bindings.size();
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

void Pipeline::Bake(RenderPass rPass, uint32_t Subpass, const char* Vtx, const char* Frag)
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

    uint32_t VtxCodeSize = VtxSize/4;
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

    uint32_t FragCodeSize = FragSize/4;
    if(FragCodeSize*FragSize < 4) FragCodeSize++; // ceil if needed.

    uint32_t* FragCode = new uint32_t[FragCodeSize];

    cFrag.read(reinterpret_cast<char*>(FragCode), FragSize);

    VkShaderModuleCreateInfo FragCI{};
    FragCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    FragCI.codeSize = FragSize;
    FragCI.pCode = FragCode;

    if((Err = vkCreateShaderModule(pCtx->Device, &FragCI, nullptr, &FragShader)) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create shader module for pipeline");
    }

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
    BlendState.attachmentCount = OutputBlending.size();
    BlendState.pAttachments = OutputBlending.data();
    BlendState.logicOpEnable = VK_FALSE;

    VkPipelineVertexInputStateCreateInfo VtxInput{};
    VtxInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VtxInput.vertexAttributeDescriptionCount = 0;
    VtxInput.vertexBindingDescriptionCount = 0;

    VkPipelineLayoutCreateInfo LayCI{};
    LayCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    LayCI.setLayoutCount = Descriptors.size();
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
    PipeCI.renderPass = rPass.rPass;
    PipeCI.subpass = Subpass;

    if((Err = vkCreateGraphicsPipelines(pCtx->Device, VK_NULL_HANDLE, 1, &PipeCI, nullptr, &Pipe)) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create pipeline");
    }
}
