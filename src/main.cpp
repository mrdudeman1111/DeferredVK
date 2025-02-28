#include <iostream>

#include "Renderer.hpp"

SceneRenderer Scene;


int main()
{
    Scene.AddFrameBufferAttachment(VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

    VkClearValue ColorClear; ColorClear.color.float32[0] = 0.f; ColorClear.color.float32[1] = 0.f; ColorClear.color.float32[2] = 0.f; ColorClear.color.float32[3] = 0.f;
    VkClearValue DepthClear; DepthClear.depthStencil.depth = 1.f;

    // Swapchain/output attachment
    Scene.AddRenderAttachment(GetWindow()->SurfFormat.format, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, ColorClear, VK_SAMPLE_COUNT_1_BIT);
    // Depth attachment
    Scene.AddRenderAttachment(VK_FORMAT_D32_SFLOAT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_CLEAR, DepthClear, VK_SAMPLE_COUNT_1_BIT);

    Subpass ForwardPass;
    // Swapchain/output attachment
    ForwardPass.AddColorAttachment(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    // Depth attachment
    ForwardPass.AddDepthAttachmment(1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    Scene.AddPass(&ForwardPass);

    Allocators::CommandPool GraphicsPool;
    GraphicsPool.Bake(false);
    Resources::CommandBuffer* RenderBuff = GraphicsPool.CreateBuffer();

    RenderBuff->Start();
        std::vector<VkImageMemoryBarrier> ImgBarriers;

        for(uint32_t i = 0; i < FrameBuffers.size(); i++)
        {
            VkImageMemoryBarrier ImgBarrier{};
            ImgBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            ImgBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            ImgBarrier.subresourceRange.levelCount = 1;
            ImgBarrier.subresourceRange.layerCount = 1;
            ImgBarrier.subresourceRange.baseArrayLayer = 0;
            ImgBarrier.subresourceRange.baseMipLevel = 0;
            ImgBarrier.srcAccessMask = VK_ACCESS_NONE;
            ImgBarrier.dstAccessMask = VK_ACCESS_NONE;
            ImgBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            ImgBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            ImgBarrier.image = GetWindow()->SwapchainImages[i];

            ImgBarriers.push_back(ImgBarrier);
        }

        vkCmdPipelineBarrier(*RenderBuff, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, ImgBarriers.size(), ImgBarriers.data());
    RenderBuff->Stop();

    GraphicsPool.Submit(RenderBuff, VK_NULL_HANDLE);

    for(uint32_t i = 0; i < GetWindow()->SwapchainAttachments.size(); i++)
    {
        //FrameBuffers[i].Bake(ForwardRenderPass.rPass);
    }

    VkPipelineColorBlendAttachmentState ColorState{};
    ColorState.blendEnable = VK_TRUE;
    ColorState.alphaBlendOp = VK_BLEND_OP_ADD;
    ColorState.colorBlendOp = VK_BLEND_OP_ADD;
    ColorState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    ColorState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    ColorState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    ColorState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    ColorState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;

    Pipeline* pForwardPipe = Scene.CreatePipeline("Forward Pipeline", 0, "Vert.spv", "Frag.spv", 1, &ColorState);

    SceneSync.FrameSem = CreateSemaphore();
    SceneSync.RenderSem = CreateSemaphore();

    uint32_t FrameIdx;

    while(Input::PollInputs())
    {
        Scene.SceneCam->Rotate();
        Scene.SceneCam->Move();

        FrameIdx = GetWindow()->GetNextFrame(SceneSync.FrameSem);

        /*
        RenderBuff.Start();
            ForwardRenderPass.Begin(RenderBuff, FrameBuffers[FrameIdx]);
                pForwardPipe->Bind(RenderBuff);

            ForwardRenderPass.End(RenderBuff);
        RenderBuff.Stop();

        GraphicsPool.Submit(&RenderBuff, nullptr, 1, &SceneSync.RenderSem);

        GetWindow()->PresentFrame(FrameIdx, &SceneSync.RenderSem);
        */
    }

    /*
    Wrappers::RenderPass ForwardPass;
        Wrappers::Subpass zPrePass; // Depth prepass to avoid overdraw
        Wrappers::Subpass ForwardPass; // Forward render all meshes with lighting
        Wrappers::Subpass OITPass; // render order independent transparency

    Wrappers::RenderPass DeferredPass;
        Wrappers::Subpass ODTPass; // render order dependent transparency to stencil
        Wrappers::Subpass GeoPass; // render geometry to gBuffer
        Wrappers::Subpass Lighting; // perform texturing and lighting on gBuffer
    */

    CloseWrapperFW();

    std::cout << "Good run\n";
    return 0;
}
