#include <iostream>

#include "Renderer.hpp"

#include "glm/gtc/matrix_transform.hpp"

SceneRenderer Scene;

struct
{
    bool Forward;
    bool Back;
    bool Left;
    bool Right;
    float MouseX;
    float MouseY;
} InputMap;

class Camera
{
public:
    void Move()
    {
        glm::vec3 Move;

        CamMat = glm::translate(CamMat, (Move*Speed));
    }

    void Rotate()
    {
        glm::vec2 MouseDelta = PrevMouse - glm::vec2(InputMap.MouseX, InputMap.MouseY);

        PrevMouse.x = InputMap.MouseX;
        PrevMouse.y = InputMap.MouseY;

        CamMat = glm::rotate(CamMat, MouseDelta.x, glm::vec3(0.f, 1.f, 0.f)); // rotate along the y-axis (up)
        CamMat = glm::rotate(CamMat, MouseDelta.y, glm::vec3(1.f, 0.f, 0.f)); // rotate along the x-axis (right)
    }

    glm::mat4 CamMat;

private:
    float Speed = 0.3f;
    glm::vec2 PrevMouse;
} SceneCamera;

struct
{
    VkSemaphore FrameSem;
    VkSemaphore RenderSem;
} SceneSync;

bool PollInputs()
{
    SDL_Event FrameEvent;
    while(SDL_PollEvent(&FrameEvent))
    {
        if(FrameEvent.type == SDL_EVENT_QUIT)
        {
            return false;
        }
        else if(FrameEvent.type == SDL_EVENT_KEY_DOWN)
        {
            if(FrameEvent.key.key == SDLK_W)
            {
                InputMap.Forward = true;
            }
            else if(FrameEvent.key.key == SDLK_S)
            {
                InputMap.Back = true;
            }
            else if(FrameEvent.key.key == SDLK_A)
            {
                InputMap.Left = true;
            }
            else if(FrameEvent.key.key == SDLK_D)
            {
                InputMap.Right = true;
            }
        }
        else if(FrameEvent.type == SDL_EVENT_KEY_UP)
        {
            if(FrameEvent.key.key == SDLK_ESCAPE)
            {
                return false;
            }
            else if(FrameEvent.key.key == SDLK_W)
            {
                InputMap.Forward = false;
            }
            else if(FrameEvent.key.key == SDLK_S)
            {
                InputMap.Back = false;
            }
            else if(FrameEvent.key.key == SDLK_A)
            {
                InputMap.Left = false;
            }
            else if(FrameEvent.key.key == SDLK_D)
            {
                InputMap.Right = false;
            }
        }
        else if(FrameEvent.type == SDL_EVENT_MOUSE_MOTION)
        {
            InputMap.MouseX = FrameEvent.motion.x;
            InputMap.MouseY = FrameEvent.motion.y;
        }
    }

    return true;
}

int main()
{
    InitWrapperFW();

    VkImageCreateInfo DepthBuffer{};
    DepthBuffer.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    DepthBuffer.extent = { GetWindow()->Resolution.width, GetWindow()->Resolution.height, 1 };
    DepthBuffer.format = VK_FORMAT_D32_SFLOAT;
    DepthBuffer.arrayLayers = 1;
    DepthBuffer.imageType = VK_IMAGE_TYPE_2D;
    DepthBuffer.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    DepthBuffer.mipLevels = 1;
    DepthBuffer.samples = VK_SAMPLE_COUNT_1_BIT;
    DepthBuffer.tiling = VK_IMAGE_TILING_OPTIMAL;
    DepthBuffer.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    DepthBuffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    std::vector<Resources::FrameBuffer> FrameBuffers(GetWindow()->SwapchainAttachments.size());
    for(uint32_t i = 0; i < FrameBuffers.size(); i++)
    {
        FrameBuffers[i].AddBuffer(GetWindow()->SwapchainAttachments[i]);
        FrameBuffers[i].AddBuffer(DepthBuffer);
    }

    VkClearValue ColorClear; ColorClear.color.float32[0] = 0.f; ColorClear.color.float32[1] = 0.f; ColorClear.color.float32[2] = 0.f; ColorClear.color.float32[3] = 0.f;
    VkClearValue DepthClear; DepthClear.depthStencil.depth = 1.f;

    RenderPass ForwardRenderPass;
    // Swapchain/output attachment
    ForwardRenderPass.AddAttachmentDesc(GetWindow()->SurfFormat.format, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, ColorClear);
    // Depth attachment
    ForwardRenderPass.AddAttachmentDesc(VK_FORMAT_D32_SFLOAT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_CLEAR, DepthClear);

    Subpass ForwardPass;
    // Swapchain/output attachment
    ForwardPass.AddColorAttachment(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    // Depth attachment
    ForwardPass.AddDepthAttachmment(1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    ForwardRenderPass.AddPass(ForwardPass);

    ForwardRenderPass.Bake();

    Allocators::CommandPool GraphicsPool;
    GraphicsPool.Bake(false);
    Resources::CommandBuffer RenderBuff = GraphicsPool.CreateBuffer();

    RenderBuff.Start();
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

        vkCmdPipelineBarrier(RenderBuff, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, ImgBarriers.size(), ImgBarriers.data());
    RenderBuff.Stop();

    GraphicsPool.Submit(&RenderBuff, VK_NULL_HANDLE);

    for(uint32_t i = 0; i < GetWindow()->SwapchainAttachments.size(); i++)
    {
        FrameBuffers[i].Bake(ForwardRenderPass.rPass);
    }

    Allocators::DescriptorPool WvpPool{};
    Resources::DescriptorLayout WvpLayout;
    Resources::Buffer WvpBuffer;
    Resources::DescriptorSet* pWvpUniform;

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

        WvpPool.SetLayout(&WvpLayout);
        WvpPool.Bake(1);

        pWvpUniform = WvpPool.CreateSet();

        Resources::DescUpdate UpdateInfo{};
        UpdateInfo.DescType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        UpdateInfo.Binding = 0;
        UpdateInfo.DescCount = 1;
        UpdateInfo.DescIndex = 0;

        UpdateInfo.pBuff = &WvpBuffer;
        UpdateInfo.Range = sizeof(glm::mat4)*3;
        UpdateInfo.Offset = 0;

        pWvpUniform->Update(UpdateInfo);
    }

    Pipeline ForwardPipe;
    ForwardPipe.SetProfile(MainProf);
    ForwardPipe.AddDescriptor(&WvpLayout);

    VkPipelineColorBlendAttachmentState ColorState{};
    ColorState.blendEnable = VK_TRUE;
    ColorState.alphaBlendOp = VK_BLEND_OP_ADD;
    ColorState.colorBlendOp = VK_BLEND_OP_ADD;
    ColorState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    ColorState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    ColorState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    ColorState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    ColorState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;

    ForwardPipe.AddAttachmentBlending(ColorState);
    ForwardPipe.Bake(&ForwardRenderPass, 0, "Vert.spv", "Frag.spv");

    glm::mat4* pWVP = (glm::mat4*)WvpBuffer.pData;

    SceneSync.FrameSem = CreateSemaphore();
    SceneSync.RenderSem = CreateSemaphore();

    uint32_t FrameIdx;

    while(PollInputs())
    {
        SceneCamera.Rotate();
        SceneCamera.Move();

        pWVP[1] = glm::inverse(SceneCamera.CamMat);

        FrameIdx = GetWindow()->GetNextFrame(SceneSync.FrameSem);

        RenderBuff.Start();
            ForwardRenderPass.Begin(RenderBuff, FrameBuffers[FrameIdx]);
                ForwardPipe.Bind(RenderBuff);

            ForwardRenderPass.End(RenderBuff);
        RenderBuff.Stop();

        GraphicsPool.Submit(&RenderBuff, nullptr, 1, &SceneSync.RenderSem);

        GetWindow()->PresentFrame(FrameIdx, &SceneSync.RenderSem);
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
