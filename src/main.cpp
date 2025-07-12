#include <iostream>

#include "Renderer.hpp"

SceneRenderer Scene;
AssetManager AssetMan;

int main()
{
    Context* pCtx = GetContext();

    AssetMan.pRenderer = &Scene;

    Scene.AddFrameBufferAttachment(VK_FORMAT_D32_SFLOAT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

    VkClearValue ColorClear; ColorClear.color.float32[0] = 0.f; ColorClear.color.float32[1] = 0.f; ColorClear.color.float32[2] = 0.f; ColorClear.color.float32[3] = 0.f;
    VkClearValue DepthClear; DepthClear.depthStencil.depth = 1.f;

    // Swapchain/output attachment
    Scene.AddRenderAttachment(GetWindow()->SurfFormat.format, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, &ColorClear, VK_SAMPLE_COUNT_1_BIT);
    // Depth attachment
    Scene.AddRenderAttachment(VK_FORMAT_D32_SFLOAT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_CLEAR, &DepthClear, VK_SAMPLE_COUNT_1_BIT);

    Subpass ForwardPass;
    // Swapchain/output attachment
    ForwardPass.AddColorAttachment(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    // Depth attachment
    ForwardPass.AddDepthAttachmment(1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    Scene.AddPass(&ForwardPass);
    Scene.Bake();

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

    pbrMesh* pMesh = AssetMan.CreateMesh("tMesh.glb", "Forward Pipeline");
    pbrMesh* pWorldMesh = AssetMan.CreateMesh("tPlane.glb", "Forward Pipeline");

    Drawable* pMeshInstance = Scene.CreateDrawable(pMesh, true);
    Drawable* pWorldInstance = Scene.CreateDrawable(pWorldMesh, true);

    pMeshInstance->SetTransform(glm::vec3(0.f), glm::vec3(0.f), glm::vec3(1.f));
    pMeshInstance->UpdateTransform();

    pWorldInstance->SetTransform(glm::vec3(0.f), glm::vec3(0.f), glm::vec3(1.f));
    pWorldInstance->UpdateTransform();

    while(Input::PollInputs())
    {
       Scene.Render();

        //FrameIdx = GetWindow()->GetNextFrame(SceneSync.FrameSem);

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
        Wrappers::Subpass GeoPass; // render geometry to gBuffer
        Wrappers::Subpass ODTPass; // render order dependent transparency to stencil
        Wrappers::Subpass Lighting; // perform texturing and lighting on gBuffer using GGX lit model
        // geometric specular anti-aliasing
    */

    std::cout << "Good run\n";
    return 0;
}
