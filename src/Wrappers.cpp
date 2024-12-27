#include "Framework.hpp"

#include <iostream>
#include <fstream>

namespace Wrappers
{
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

            vkDestroyDescriptorSetLayout(pCtx->Device, Layout, nullptr);
            vkCreateDescriptorSetLayout(pCtx->Device, &LayCI, nullptr, &Layout);
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
                VkImage tmp;
                VkImageView tmpView;

                if((Err = vkCreateImage(pCtx->Device, &AttachmentInfos[i], nullptr, &tmp)) != VK_SUCCESS)
                {
                    throw std::runtime_error("Failed to create Attachment for framebuffer.");
                }

                VkImageViewCreateInfo ViewCI{};
                ViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                ViewCI.format = AttachmentInfos[i].format;
                ViewCI.components.r = VK_COMPONENT_SWIZZLE_R;
                ViewCI.components.g = VK_COMPONENT_SWIZZLE_G;
                ViewCI.components.b = VK_COMPONENT_SWIZZLE_B;
                ViewCI.components.a = VK_COMPONENT_SWIZZLE_A;
                ViewCI.image = tmp;
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

                Attachments.push_back(tmp);
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

    void RenderPass::Bake()
    {
        VkResult Err;

        VkSubpassDescription* sPasses = new VkSubpassDescription[Subpasses.size()];

        Context* pCtx = GetContext();

        for(uint32_t i = 0; i < Subpasses.size(); i++)
        {
            sPasses[i] = Subpasses[i].SubpassDesc;
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

    VkPipelineInputAssemblyStateCreateInfo* PipelineProfile::GetAssembly()
    {
        AssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        AssemblyState.primitiveRestartEnable = VK_FALSE;
        AssemblyState.topology = Topo;

        return &AssemblyState;
    }

    void Pipeline::Bake(RenderPass rPass, uint32_t Subpass, std::string& Vtx, std::string& Frag)
    {
        VkResult Err;

        Context* pCtx = GetContext();

        std::ifstream cVtx(Vtx.c_str(), std::ios::binary);

        cVtx.seekg(cVtx.end);
        size_t VtxSize = cVtx.tellg();
        cVtx.seekg(cVtx.beg);

        uint32_t VtxCodeSize = VtxSize/4;
        if(VtxCodeSize*VtxSize < 4) VtxCodeSize++; // ceil if needed.

        uint32_t* VtxCode = new uint32_t[VtxCodeSize];

        cVtx.read((char*)VtxCode, VtxSize);

        VkShaderModuleCreateInfo VtxCI{};
        VtxCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        VtxCI.codeSize = VtxSize;
        VtxCI.pCode = VtxCode;

        if((Err = vkCreateShaderModule(pCtx->Device, &VtxCI, nullptr, &VtxShader)) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create shader module for pipeline");
        }

        std::ifstream cFrag(Vtx.c_str(), std::ios::binary);

        cVtx.seekg(cVtx.end);
        size_t FragSize = cVtx.tellg();
        cVtx.seekg(cVtx.beg);

        uint32_t FragCodeSize = FragSize/4;
        if(FragCodeSize*FragSize < 4) FragCodeSize++; // ceil if needed.

        uint32_t* FragCode = new uint32_t[FragCodeSize];

        cVtx.read((char*)FragCode, FragSize);

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
        PipeCI.pInputAssemblyState = Profile.GetAssembly();
        PipeCI.pRasterizationState = &RasterState;
        PipeCI.pColorBlendState = &BlendState;
        PipeCI.pVertexInputState = &VtxInput;
        PipeCI.stageCount = 2;
        PipeCI.pStages = ShaderStages;
        PipeCI.renderPass = rPass.rPass;
        PipeCI.subpass = Subpass;

        if((Err = vkCreateGraphicsPipelines(pCtx->Device, VK_NULL_HANDLE, 1, &PipeCI, nullptr, &Pipe)) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create pipeline");
        }
    }
}