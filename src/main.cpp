#include <iostream>
#include <vulkan/vulkan_core.h>

#include "Renderer.hpp"

// TODO : implement OpenImageIO and MaterialX

// implement lighting to the shader side. P.S. The light count variable does not exist on the application side buffer yet.

SceneRenderer Scene;
AssetManager AssetMan;

int main()
{
    #ifdef DEBUG_MODE
        std::cout << "Debug mode enabled\n";
    #endif

    AssetMan.pRenderer = &Scene;

    Scene.AddFrameBufferAttachment(VK_FORMAT_D32_SFLOAT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT); // Depth Attachmen
    Scene.AddFrameBufferAttachment(VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT); // Albedo/Roughness Attachment
    Scene.AddFrameBufferAttachment(VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT); // Normal\Metallic Attachment

    VkClearValue ColorClear; ColorClear.color.float32[0] = 0.f; ColorClear.color.float32[1] = 0.f; ColorClear.color.float32[2] = 0.f; ColorClear.color.float32[3] = 0.f;

    VkClearValue DepthClear; DepthClear.depthStencil.depth = 1.f;

    // Depth attachment
    Scene.AddRenderAttachment(VK_FORMAT_D32_SFLOAT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, &DepthClear, VK_SAMPLE_COUNT_1_BIT);
    // Albedo/Roughness attachment
    Scene.AddRenderAttachment(VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, &ColorClear, VK_SAMPLE_COUNT_1_BIT); // RGB: Albedo Color, A: Roughness
    // Normal/Metallic attachment
    Scene.AddRenderAttachment(VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, &ColorClear, VK_SAMPLE_COUNT_1_BIT); // RGB: Normal, A: Metallic


    Subpass ForwardPass;
    // Swapchain/output attachment
    ForwardPass.AddColorAttachment(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    // Depth attachment
    ForwardPass.AddDepthAttachmment(1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    // Albedo/Roughness attachment
    ForwardPass.AddColorAttachment(2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    // Normal/Metallic attachment
    ForwardPass.AddColorAttachment(3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

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

    VkPipelineColorBlendAttachmentState ColorStates[3] = { ColorState, ColorState, ColorState };

    Pipeline* pForwardPipe = Scene.CreatePipeline("Forward Pipeline", 0, "Vert.spv", "Frag.spv", 3, ColorStates);

    uint32_t tMeshCount;
    pbrMesh** pMesh = AssetMan.CreateMesh(working_directory"tMesh.glb", "Forward Pipeline", tMeshCount);
    printf("Loaded %d meshes from tMesh.glb", tMeshCount);
    
    uint32_t tWorldCount;
    pbrMesh** pWorldMesh = AssetMan.CreateMesh(working_directory"tPlane.glb", "Forward Pipeline", tWorldCount);
    printf("Loaded %d meshes from tPlane.glb\n", tWorldCount);

    Drawable* pMeshInstance = Scene.CreateDrawable(pMesh[0], true);
    Drawable* pWorldInstance = Scene.CreateDrawable(pWorldMesh[0], true);

    pMeshInstance->SetTransform(glm::vec3(0.f, 1.f, 0.f), glm::vec3(0.f), glm::vec3(1.f));
    pMeshInstance->UpdateTransform();

    pWorldInstance->SetTransform(glm::vec3(1.f, 0.f, 1.f), glm::vec3(0.f), glm::vec3(1.f));
    pWorldInstance->UpdateTransform();

     uint32_t SceneLight = Scene.CreatePointLight(glm::vec3(0.f, 0.f, 3.f), glm::vec3(1.f, 1.f, 1.f));
    
    while(Input::PollInputs())
    {
        Scene.Render();
        // wait for current render to update resources??
        Scene.Update();
        
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
        Wrappers::Subpass ODTPass; // render order dependent transparency to stencil. (This will likely be done in a forward pass
        Wrappers::Subpass Lighting; // perform texturing and lighting on gBuffer using GGX lit model
        // geometric specular anti-aliasing
    */
    
    delete pMesh;
    delete pWorldMesh;

    std::cout << "Good run\n";
    return 0;
}
