#include "Renderer.hpp"
#include "Mesh.hpp"

#include <cstddef>
#include <algorithm>
#include <glm/common.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <vulkan/vulkan_core.h>

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tgltf/tiny_gltf.h"

tinygltf::TinyGLTF MeshLoader;

Resources::DescriptorLayout* Instanced::pMeshPassLayout = {};

bool ExtractVtxComp(tinygltf::Model& Model, tinygltf::Primitive& Prim, std::string CompName, int CompCount, std::vector<float>& OutArr)
{
    auto Iter = Prim.attributes.find(CompName);

    if(Iter == Prim.attributes.end()) return false;

    const tinygltf::Accessor& Accessor = Model.accessors[Iter->second]; // the component can be found in the accessor, so change compcount to an output variable later.
    const tinygltf::BufferView& BuffView = Model.bufferViews[Accessor.bufferView];
    const tinygltf::Buffer& Buff = Model.buffers[BuffView.buffer];

    uint32_t Count = Accessor.count;
    const uint8_t* pData = &Buff.data[BuffView.byteOffset + Accessor.byteOffset];
    int Type = Accessor.type;

    #ifdef DEBUG_MODE
        bool ScalarCheck = (Type == TINYGLTF_TYPE_SCALAR && CompCount != 1);
        bool Vec2Check = (Type == TINYGLTF_TYPE_VEC2 && CompCount != 2);
        bool Vec3Check = (Type == TINYGLTF_TYPE_VEC3 && CompCount != 3);
        bool Vec4Check = (Type == TINYGLTF_TYPE_VEC4 && CompCount != 4);

        if(ScalarCheck || Vec2Check || Vec3Check || Vec4Check)
        {
            throw std::runtime_error("Comp count does does not match reported component count of the requested component type");
        }
    #endif

    OutArr.clear();
    OutArr.resize(Accessor.count*CompCount);

    for(uint32_t i = 0; i < Accessor.count; i++)
    {
        for(uint32_t x = 0; x < CompCount; x++)
        {
            uint32_t Idx = (i*CompCount)+x;
            float* Comp = (float*)pData;
            Comp += Idx;
            OutArr[Idx] = *Comp;
        }
    }

    return true;
}

bool ExtractIdx(tinygltf::Model& Model, tinygltf::Primitive& Prim, std::vector<uint32_t>& Indices)
{
    if(Prim.indices < 0) return false;

    const tinygltf::Accessor& Accessor = Model.accessors[Prim.indices];
    const tinygltf::BufferView& BuffView = Model.bufferViews[Accessor.bufferView];
    const tinygltf::Buffer& Buffer = Model.buffers[BuffView.buffer];

    uint32_t IdxCount = static_cast<uint32_t>(Accessor.count);

    const void* pData = &(Buffer.data[BuffView.byteOffset + Accessor.byteOffset]);

    Indices.resize(IdxCount);
    
    uint16_t tmp[IdxCount];

    if(Accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
    {
        const uint16_t* Dat = reinterpret_cast<const uint16_t*>(pData);

        for(uint32_t i = 0; i < IdxCount; i++)
        {
            Indices[i] = (Dat[i]);
        }
    }
    else
    {
        const uint32_t* Dat = reinterpret_cast<const uint32_t*>(pData);

        for(uint32_t i = 0; i < IdxCount; i++)
        {
            Indices[i] = Dat[i];
        }
    }

    return true;
}

Camera::Camera() : WvpBuffer("Camera WVP Buffer")
{
    CreateBuffer(WvpBuffer, (sizeof(glm::mat4)*3)+(sizeof(glm::vec4)*6), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    Allocate(WvpBuffer, true);

    Map(&WvpBuffer);

    MoveSpeed = 0.01f;

    CamMat = glm::mat4(1.f);

    Position = glm::vec3(0.f, 0.f, 5.f);
    Rotation = glm::vec3(0.f);

    pWvp = (glm::mat4*)WvpBuffer.pData;
    pWvp[0] = glm::mat4(1.f);
    pWvp[2] = glm::perspective(45.f, 16.f/9.f, 0.f, 9999.f);
    pWvp[2][1][1] *= -1.f;
}

void Camera::Update()
{
    CamMat = glm::translate(glm::mat4(1.f), Position);

    CamMat = glm::rotate(CamMat, glm::radians(Rotation.x*-1.f), glm::vec3(0.f, 1.f, 0.f)); // rotate along the y-axis (up)
    CamMat = glm::rotate(CamMat, glm::radians(Rotation.y), glm::vec3(1.f, 0.f, 0.f)); // rotate along the x-axis (right)

    pWvp[1] = glm::inverse(CamMat);
}

void Camera::Move()
{
    glm::vec3 Move(0.f);

    if(Input::GetInputMap()->Forward)
    {
        // Move += glm::vec3(0.f, 0.f, -1.f);
        Move -= glm::vec3(CamMat[2][0], CamMat[2][1], CamMat[2][2]);
    }
    if(Input::GetInputMap()->Back)
    {
        // Move -= glm::vec3(0.f, 0.f, -1.f);
        Move += glm::vec3(CamMat[2][0], CamMat[2][1], CamMat[2][2]);
    }
    if(Input::GetInputMap()->Right)
    {
        // Move += glm::vec3(1.f, 0.f, 0.f);
        Move += glm::vec3(CamMat[0][0], CamMat[0][1], CamMat[0][2]);
    }
    if(Input::GetInputMap()->Left)
    {
        // Move -= glm::vec3(1.f, 0.f, 0.f);
        Move -= glm::vec3(CamMat[0][0], CamMat[0][1], CamMat[0][2]);
    }

    if(glm::length(Move) == 0.f)
    {
        return;
    }

    Move = glm::normalize(Move);

    Position += Move*MoveSpeed;

    return;
}

void Camera::Rotate()
{
    glm::vec2 MouseDelta = glm::vec2(Input::GetInputMap()->MouseX, Input::GetInputMap()->MouseY) - PrevMouse;

    /*
    if(glm::length(MouseDelta) == 0.f)
    {
        return;
    }
    */

    PrevMouse.x = Input::GetInputMap()->MouseX;
    PrevMouse.y = Input::GetInputMap()->MouseY;

    Rotation.x += MouseDelta.x/100.f;

    Rotation.y -= MouseDelta.y/100.f;
    Rotation.y = glm::clamp(Rotation.y, -90.f, 90.f);

    return;
}

void Camera::GenPlanes()
{
    for(uint32_t i = 0; i < 3; i++)
    {
        for(uint32_t x = 0; x < 2; x++)
        {
            float sign = x ? 1.f : -1.f;

            for(uint32_t k = 0; k < 4; k++)
            {
                glm::mat4 View = glm::inverse(CamMat);
                Planes[2*i+x][k] = View[k][3] + sign * View[k][i];
            }
        }
    }

    for(glm::vec4& pl : Planes)
    {
        pl /= static_cast<float>(glm::length(glm::vec3(pl)));
    }
}

void PipeStage::Update()
{
    // Perform per-tick operations.

    for(uint32_t i = 0; i < Meshes.size(); i++)
    {
        Meshes[i]->Update();
    }
}

void PipeStage::UpdateDraws(Resources::CommandBuffer* pCmdBuffer, VkPipelineLayout ComputeLayout)
{
    for(uint32_t i = 0; i < Meshes.size(); i++)
    {
        Meshes[i]->GenDraws(*pCmdBuffer, ComputeLayout);
    }
}

void PipeStage::Draw(Resources::CommandBuffer* pCmdBuffer)
{
    vkCmdBindPipeline(*pCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *Pipe);

    for(uint32_t i = 0; i < Meshes.size(); i++)
    {
        Meshes[i]->DrawInstances(*pCmdBuffer, Pipe->PipeLayout);
    }
}

void PassStage::Update()
{
    // perform per-tick operations.

    for(uint32_t i = 0; i < PipeStages.size(); i++)
    {
        PipeStages[i]->Update();        
    }
}

void FrameBufferChain::Bake(RenderPass* pPass, VkCommandBuffer* pCmdBuffer)
{
    VkResult Err;

    Context* pCtx = GetContext();
    FBCount = GetWindow()->SwapchainAttachments.size();

    FrameBuffers.resize(FBCount);

    for(uint32_t i = 0; i < FBCount; i++)
    {
        FrameBuffers[i].AddBuffer(GetWindow()->SwapchainAttachments[i], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        for(uint32_t x = 0; x < AttachmentDescs.size(); x++)
        {
            VkImageLayout dstLayout = AttachmentDescs[x].initialLayout;
            AttachmentDescs[x].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            FrameBuffers[i].AddBuffer(AttachmentDescs[x], dstLayout);
            AttachmentDescs[x].initialLayout = dstLayout;
        }

        FrameBuffers[i].Bake(pPass->rPass, pCmdBuffer);
    }

    return;
}

SceneRenderer::SceneRenderer() : StaticSceneBuffer("Static Scene Buffer"), DynamicSceneBuffer("StaticSceneBuffer")
{
    VkResult Err;

    InitWrapperFW();

    SceneCam = new Camera();

    SceneProfile.Topo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    SceneProfile.MsaaSamples = VK_SAMPLE_COUNT_1_BIT;

    SceneProfile.RenderSize = {1280, 720, 1};
    SceneProfile.RenderOffset.extent = {1280, 720};
    SceneProfile.RenderOffset.offset = {0, 0};

    SceneProfile.bStencilTesting = true;
    SceneProfile.bDepthTesting = true;
    SceneProfile.DepthRange = {0.f, 1.f};
    SceneProfile.DepthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    SceneProfile.AddBinding(0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX);
    SceneProfile.AddAttribute(0, VK_FORMAT_R32G32B32_SFLOAT, 0, offsetof(Vertex, Position));   // Position
    SceneProfile.AddAttribute(0, VK_FORMAT_R32G32B32_SFLOAT, 1, offsetof(Vertex, Normal));   // Normal
    SceneProfile.AddAttribute(0, VK_FORMAT_R32G32_SFLOAT, 2, offsetof(Vertex, UV));    // UV

    // SceneProfile.AddBinding(1, sizeof(uint32_t), VK_VERTEX_INPUT_RATE_INSTANCE);
    // SceneProfile.AddAttribute(1, VK_FORMAT_R32_UINT, 3, 0); // Artificial Inst Idx

    Context* pCtx = GetContext();
    GraphicsHeap.Bake(CommandType::eCmdGraphics);
    ComputeHeap.Bake(CommandType::eCmdCompute);
    TransferHeap.Bake(CommandType::eCmdTransfer);

    VkDescriptorSetLayoutBinding MeshPassLayoutBinding{};
    MeshPassLayoutBinding.binding = 0;
    MeshPassLayoutBinding.descriptorCount = 1;
    MeshPassLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    MeshPassLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT;

    /*
    VkDescriptorSetLayoutBinding InstanceArrBinding{};
    MeshPassLayoutBinding.binding = 1;
    MeshPassLayoutBinding.descriptorCount = 1;
    MeshPassLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    MeshPassLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    */

    Instanced::pMeshPassLayout = new Resources::DescriptorLayout();
    Instanced::pMeshPassLayout->AddBinding(MeshPassLayoutBinding);
    // Instanced::pMeshPassLayout->AddBinding(InstanceArrBinding);
    DescriptorHeaps[*Instanced::pMeshPassLayout].Bake(Instanced::pMeshPassLayout, 1000);

    if((Err = CreateBuffer(StaticSceneBuffer, sizeof(glm::mat4)*MAX_STATIC_SCENE_SIZE, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)) != VK_SUCCESS) throw std::runtime_error("Failed to create static scene buffer.");
    Allocate(StaticSceneBuffer, false);

    Err = CreateBuffer(DynamicSceneBuffer, sizeof(glm::mat4)*MAX_DYNAMIC_SCENE_SIZE, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    Allocate(DynamicSceneBuffer, true);
    Map(&DynamicSceneBuffer);

    pSceneDescriptorLayout = new Resources::DescriptorLayout();

    VkDescriptorSetLayoutBinding SceneBindings[3] = {};
    SceneBindings[0].binding = 0;
    SceneBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    SceneBindings[0].descriptorCount = 1;
    SceneBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT;

    SceneBindings[1].binding = 1;
    SceneBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    SceneBindings[1].descriptorCount = 1;
    SceneBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT;

    SceneBindings[2].binding = 2;
    SceneBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    SceneBindings[2].descriptorCount = 1;
    SceneBindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT;

    /*
    SceneBindings[3].binding = 3;
    SceneBindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    SceneBindings[3].descriptorCount = 1;
    SceneBindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    */

    pSceneDescriptorLayout->AddBinding(SceneBindings[0]);
    pSceneDescriptorLayout->AddBinding(SceneBindings[1]);
    pSceneDescriptorLayout->AddBinding(SceneBindings[2]);
    //pSceneDescriptorLayout->AddBinding(SceneBindings[3]);

    DescriptorHeaps[*pSceneDescriptorLayout].Bake(pSceneDescriptorLayout, 2);
    pSceneDescriptorSet = DescriptorHeaps[*pSceneDescriptorLayout].CreateSet();

    Resources::DescUpdate SceneUpdates[3] = {};

    SceneUpdates[0].DescType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    SceneUpdates[0].DescCount = 1;
    SceneUpdates[0].Binding = 0;
    SceneUpdates[0].DescIndex = 0;

    SceneUpdates[0].pBuff = &SceneCam->WvpBuffer;
    SceneUpdates[0].Range = (sizeof(glm::mat4)*3) + (sizeof(glm::vec4)*6);
    SceneUpdates[0].Offset = 0;

    SceneUpdates[1].DescType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    SceneUpdates[1].Binding = 1;
    SceneUpdates[1].DescCount = 1;
    SceneUpdates[1].DescIndex = 0;

    SceneUpdates[1].pBuff = &StaticSceneBuffer;
    SceneUpdates[1].Range = StaticSceneBuffer.Alloc.Size;
    SceneUpdates[1].Offset = 0;

    SceneUpdates[2].DescType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    SceneUpdates[2].Binding = 2;
    SceneUpdates[2].DescCount = 1;
    SceneUpdates[2].DescIndex = 0;

    SceneUpdates[2].pBuff = &DynamicSceneBuffer;
    SceneUpdates[2].Range = DynamicSceneBuffer.Alloc.Size;
    SceneUpdates[2].Offset = 0;

    pSceneDescriptorSet->Update(SceneUpdates, 3);

    DrawPipeline.AddDescriptor(pSceneDescriptorLayout);
    DrawPipeline.AddDescriptor(Instanced::pMeshPassLayout);

    DrawPipeline.Bake("Draw.spv");
}

SceneRenderer::~SceneRenderer()
{
    DescriptorHeaps.clear();

    delete pSceneDescriptorLayout;
    delete pSceneDescriptorSet;

    delete Instanced::pMeshPassLayout;

    CloseWrapperFW();
}

Pipeline* SceneRenderer::CreatePipeline(std::string PipeName, uint32_t SubpassIdx, const char* VtxPath, const char* FragPath, uint32_t BlendAttCount, VkPipelineColorBlendAttachmentState* BlendAttachments, uint32_t DescriptorCount, Resources::DescriptorLayout* pDescriptors, PipelineProfile* pProfile)
{
    if(PipeStages.count(PipeName) == 0)
    {
        PipeStages[PipeName] = new PipeStage(); // push the new pipe stage into the pipestages vector
        Pipeline* Pipe = new Pipeline(); // create a temporary pipeline

        // Add the scene descriptor set.
        Pipe->AddDescriptor(pSceneDescriptorLayout);
        Pipe->AddDescriptor(Instanced::pMeshPassLayout);

        // Add the passed descriptors
        for(uint32_t i = 0; i < DescriptorCount; i++)
        {
            Pipe->AddDescriptor(&pDescriptors[i]);
        }

        // Add the blending attachments (for color/depth output blending)
        for(uint32_t i = 0; i < BlendAttCount; i++)
        {
            Pipe->AddAttachmentBlending(BlendAttachments[i]);
        }

        // Set the pipeline profile to scene default
        Pipe->SetProfile(SceneProfile);

        // bake the pipeline in the renderpass/subpass with the specified shaders.
        Pipe->Bake(&ScenePass, SubpassIdx, VtxPath, FragPath);

        // push set the new pipeline stage's pipeline to the temporary we just built.
        PipeStages[PipeName]->Pipe = Pipe;

        int32_t i = 0;

        // if the pass stages array does not have a subpass at index SubpassIdx, We push a new null pass until we hit SubpassIdx, then we push the new pipelinestage into the specified pass
        if((i = PassStages.size() - (SubpassIdx+1)) < 0)
        {
            #ifdef DEBUG_MODE
                std::cout << "CreatePipeline() : The specified subpass index exceeds registered subpass array bounds. Creating needed stud passes\n";
            #endif

            while(i != 0)
            {
                PassStages.push_back({});
                i++;
            }
        }

        PassStages[SubpassIdx].PipeStages.push_back(PipeStages[PipeName]);
        return Pipe;
    }
    else
    {
        std::cout << "Tried to create \"" << PipeName << "\" Pipeline, but a pipeline of the same name already exists.\n";
        return PipeStages[PipeName]->Pipe;
    }

    return nullptr;
}

void SceneRenderer::AddMesh(pbrMesh* pMesh, std::string PipeName)
{
    pMesh->pMeshPassSet = DescriptorHeaps[*Instanced::pMeshPassLayout].CreateSet();
    PipeStages[PipeName]->Meshes.push_back(pMesh);
}

Drawable* SceneRenderer::CreateDrawable(pbrMesh* Mesh, bool bDynamic)
{
    Drawable* pRet;

    if(bDynamic)
    {
        pRet = new Drawable(&DynamicSceneBuffer);
        pRet->pMesh = Mesh;
        pRet->ObjIdx = DynamicIter;
        DynamicIter++;
        pRet->bStatic = false;
    }
    else
    {
        pRet = new Drawable(&StaticSceneBuffer);
        pRet->pMesh = Mesh;
        pRet->ObjIdx = StaticIter;
        StaticIter++;
        pRet->bStatic = true;
    }

    Mesh->AddInstance(pRet->ObjIdx);

    return pRet;
}

void SceneRenderer::AddPass(Subpass* pSubpass)
{
    ScenePass.AddPass(*pSubpass);
    return;
}

void SceneRenderer::Bake()
{
    ScenePass.Bake();

    SceneSync.DrawGenSem = CreateVulkanSemaphore();
    SceneSync.pFrameFence = CreateFence();

    pCmdComputeBuffer = ComputeHeap.CreateBuffer();

    pCmdOpsBuffer = GraphicsHeap.CreateBuffer();
    pCmdOpsBuffer->Start();

        FrameChain.Bake(&ScenePass, *pCmdOpsBuffer);

        uint32_t FrameCount = FrameChain.FrameBuffers.size();

        for(uint32_t i = 0; i < GetWindow()->SwapchainImages.size(); i++)
        {
          RenderSemaphores.push_back(CreateVulkanSemaphore());
        }

        VkImageMemoryBarrier SwapchainBarriers[FrameCount];

        for(uint32_t i = 0; i < FrameCount; i++)
        {
            SwapchainBarriers[i] = {};

            SwapchainBarriers[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            SwapchainBarriers[i].image = GetWindow()->SwapchainImages[i];
            SwapchainBarriers[i].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            SwapchainBarriers[i].newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            SwapchainBarriers[i].srcAccessMask = VK_ACCESS_NONE;
            SwapchainBarriers[i].dstAccessMask = VK_ACCESS_NONE;

            SwapchainBarriers[i].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            SwapchainBarriers[i].subresourceRange.baseArrayLayer = 0;
            SwapchainBarriers[i].subresourceRange.baseMipLevel = 0;
            SwapchainBarriers[i].subresourceRange.layerCount = 1;
            SwapchainBarriers[i].subresourceRange.levelCount = 1; // Transition the swapchain images into color attachments so they can be used as output image.
        }

        vkCmdPipelineBarrier(*pCmdOpsBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, FrameCount, SwapchainBarriers);

    pCmdOpsBuffer->Stop();

    GraphicsHeap.Submit(pCmdOpsBuffer);
}

void SceneRenderer::Update()
{
    for(uint32_t i = 0; i < PassStages.size(); i++)
    {
        PassStages[i].Update();
    }
}

void SceneRenderer::Render()
{
    if(pCmdRenderBuffer == nullptr)
    {
        pCmdRenderBuffer = GraphicsHeap.CreateBuffer();
    }

    TransferAgent* pTransfer = GetTransferAgent();
    Context* pCtx = GetContext();

    pCmdComputeBuffer->cmdFence->Wait(); // wait for previous frame to render

    uint32_t FrameIdx = GetWindow()->GetNextFrame(SceneSync.pFrameFence);

    SceneCam->Rotate();
    SceneCam->Move();
    SceneCam->Update();

    /* Update descriptor Information (i.e. instance lists and transforms) */
    for(PassStage& pStage : PassStages)
    {
        for(PipeStage* pPipeStage : pStage.PipeStages)
        {
            for(pbrMesh* pMesh : pPipeStage->Meshes)
            {
                pMesh->Update();
            }
        }
    }

    GetTransferAgent()->Flush();

    /**** TEMP : move sync to front of function ****/
    // pCmdRenderBuffer->cmdFence->Wait(); // wait for previous frame to render

    pCmdComputeBuffer->Reset();
    pCmdComputeBuffer->Start();

        vkCmdBindDescriptorSets(*pCmdComputeBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, DrawPipeline.PipeLayout, 0, 1, &pSceneDescriptorSet->DescSet, 0, nullptr);
        vkCmdBindPipeline(*pCmdComputeBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, DrawPipeline);

       // pTransfer->AwaitFlush();

        for(uint32_t i = 0; i < PassStages.size(); i++)
        {
            for(uint32_t x = 0; x < PassStages[i].PipeStages.size(); x++)
            {
                PassStages[i].PipeStages[x]->UpdateDraws(pCmdComputeBuffer, DrawPipeline.PipeLayout);
            }
        }
 
    pCmdComputeBuffer->Stop();

    ComputeHeap.Submit(pCmdComputeBuffer, 1, &SceneSync.DrawGenSem);

    pCmdRenderBuffer->Reset();
    pCmdRenderBuffer->Start();

        ScenePass.Begin(*pCmdRenderBuffer, FrameChain.FrameBuffers[FrameIdx]);
            for(uint32_t i = 0; i < PassStages.size(); i++)
            {
                vkCmdBindDescriptorSets(*pCmdRenderBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, PassStages[i].PipeStages[0]->Pipe->PipeLayout, 0, 1, &pSceneDescriptorSet->DescSet, 0, nullptr);

                for(uint32_t x = 0; x < PassStages[i].PipeStages.size(); x++)
                {
                    PassStages[i].PipeStages[x]->Draw(pCmdRenderBuffer);
                }

                if(i+1 != PassStages.size())
                {
                    vkCmdNextSubpass(*pCmdRenderBuffer, VK_SUBPASS_CONTENTS_INLINE);
                }
            }
        ScenePass.End(*pCmdRenderBuffer);

    pCmdRenderBuffer->Stop();

    SceneSync.pFrameFence->Wait(); // wait for image acquisition
    GraphicsHeap.Submit(pCmdRenderBuffer, 1, &RenderSemaphores[FrameIdx], 1, &SceneSync.DrawGenSem, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    GetWindow()->PresentFrame(FrameIdx, &RenderSemaphores[FrameIdx]);

    pCmdComputeBuffer->cmdFence->Wait();
    pCmdRenderBuffer->cmdFence->Wait(); // TEMP.
}

pbrMesh** AssetManager::CreateMesh(std::string Path, std::string PipeName, uint32_t& MeshCount)
{
    // initiate stack variables
    std::vector<pbrMesh*> Ret = {};
 
    tinygltf::Model tmpModel;

    std::string Err, Warn;

    // load the mesh file
    // if(!MeshLoader.LoadBinaryFromFile(&tmpModel, &Err, &Warn, Path))
    if(!MeshLoader.LoadBinaryFromFile(&tmpModel, &Err, &Warn, Path))
    {
        throw std::runtime_error("Failed to load mesh file " + Path);
    }

    if(tmpModel.scenes.size() == 0)
    {
        std::cout << "Tried to create drawable mesh, but couldn't due to an empty scene.";
        return nullptr;
    }

    // process the File from the MeshScene variable provided from tinygltf.
    tinygltf::Scene& MeshScene = tmpModel.scenes[(tmpModel.defaultScene > -1) ? tmpModel.defaultScene : 0];

    for(int NodeIdx : MeshScene.nodes)
    {
        // skip the node if there is no mesh
        if(tmpModel.nodes[NodeIdx].mesh < 0) continue;

        tinygltf::Mesh& Mesh = tmpModel.meshes[tmpModel.nodes[NodeIdx].mesh];
        
        if(Mesh.primitives.size() == 0)
        {
            continue;
        }

        pbrMesh* pTmp = new pbrMesh();
        Ret.push_back(pTmp);
        Ret.back()->Name = Mesh.name;

        // process the mesh primitives' vertices.
        for(tinygltf::Primitive& Prim : Mesh.primitives)
        {
            std::vector<float> tmpPos;
            std::vector<float> tmpNorm;
            std::vector<float> tmpUV;

            if(!ExtractVtxComp(tmpModel, Prim, "POSITION", 3, tmpPos))
            {
              throw std::runtime_error("Failed to extract position data from mesh\n");
            }

            if(!ExtractVtxComp(tmpModel, Prim, "NORMAL", 3, tmpNorm))
            {
              throw std::runtime_error("Failed to extract normal data from mesh\n");
            }

            if(!ExtractVtxComp(tmpModel, Prim, "TEXCOORD_0", 2, tmpUV))
            {
              throw std::runtime_error("Failed to extract UV data from mesh\n");
            }

            Ret.back()->pVertices = new Vertex[tmpPos.size()/3];
            Ret.back()->VertCount = tmpPos.size()/3;

            for(uint32_t i = 0; i < tmpPos.size()/3; i++)
            {
                uint32_t Vec3Idx = i*3; // 3, 6, 9, 12
                uint32_t Vec2Idx = i*2; // 2, 4, 6, 8

                Ret.back()->pVertices[i].Position.x = tmpPos[ Vec3Idx ];
                Ret.back()->pVertices[i].Position.y = tmpPos[ Vec3Idx+1 ];
                Ret.back()->pVertices[i].Position.z = tmpPos[ Vec3Idx+2 ];

                Ret.back()->pVertices[i].Normal.x = tmpNorm[ Vec3Idx ];
                Ret.back()->pVertices[i].Normal.y = tmpNorm[ Vec3Idx+1 ];
                Ret.back()->pVertices[i].Normal.z = tmpNorm[ Vec3Idx+2 ];

                Ret.back()->pVertices[i].UV.x = tmpUV[ Vec2Idx ];
                Ret.back()->pVertices[i].UV.y = tmpUV[ Vec2Idx+1 ];
           }

           #ifdef DEBUG_MODE
                Ret.back()->Vertices.resize(tmpPos.size()/3);
                
                for(uint32_t i = 0; i < tmpPos.size()/3; i++)
                {
                    uint32_t Vec3Idx = i*3; // 3, 6, 9, 12
                    uint32_t Vec2Idx = i*2; // 2, 4, 6, 8

                    Ret.back()->Vertices[i].Position.x = tmpPos[ Vec3Idx ];
                    Ret.back()->Vertices[i].Position.y = tmpPos[ Vec3Idx+1 ];
                    Ret.back()->Vertices[i].Position.z = tmpPos[ Vec3Idx+2 ];
                        
                    Ret.back()->Vertices[i].Position.x = tmpNorm[ Vec3Idx ];
                    Ret.back()->Vertices[i].Position.y = tmpNorm[ Vec3Idx+1 ];
                    Ret.back()->Vertices[i].Position.z = tmpNorm[ Vec3Idx+2 ];
                    
                    Ret.back()->Vertices[i].UV.x = tmpUV[ Vec2Idx ];
                    Ret.back()->Vertices[i].UV.y = tmpUV[ Vec2Idx+1 ];
                }
            #endif
 

            std::vector<uint32_t> tmpIdx;

            if(!ExtractIdx(tmpModel, Prim, tmpIdx))
            {
              throw std::runtime_error("Failed to extract index data from mesh\n");
            }

            Ret.back()->pIndices = new uint32_t[tmpIdx.size()];
            Ret.back()->IndexCount = tmpIdx.size();

            for(uint32_t i = 0; i < tmpIdx.size(); i++)
            {
                Ret.back()->pIndices[i] = tmpIdx[i];
            }
 
            #ifdef DEBUG_MODE
                Ret.back()->Indices.resize(tmpIdx.size());

                for(uint32_t i = 0; i < tmpIdx.size(); i++)
                {
                    Ret.back()->Indices[i] = tmpIdx[i];
                }
            #endif
        }

        // create the mesh buffer (for vertices and indices)
        CreateBuffer(Ret.back()->MeshBuffer, (Ret.back()->VertCount * sizeof(Vertex)) + (Ret.back()->IndexCount * sizeof(uint32_t)), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

        #ifdef DEBUG_MODE
            Allocate(Ret.back()->MeshBuffer, true);
            Map(&Ret.back()->MeshBuffer);

            GetTransferAgent()->Transfer(Ret.back()->Vertices.data(), Ret.back()->Vertices.size()*sizeof(Vertex), &Ret.back()->MeshBuffer, 0);
            GetTransferAgent()->Transfer(Ret.back()->Indices.data(), Ret.back()->Indices.size()*sizeof(uint32_t), &Ret.back()->MeshBuffer, Ret.back()->Vertices.size()*sizeof(Vertex));
        #else
            Allocate(Ret.back()->MeshBuffer, false);

            GetTransferAgent()->Transfer(Ret.back()->pVertices, Ret.back()->VertCount*sizeof(Vertex), &Ret.back()->MeshBuffer, 0);
            GetTransferAgent()->Transfer(Ret.back()->pIndices, Ret.back()->IndexCount*sizeof(uint32_t), &Ret.back()->MeshBuffer, Ret.back()->VertCount*sizeof(Vertex));
        #endif

        pRenderer->AddMesh(Ret.back(), PipeName);

        // Store the byte offset of the indices (needed during render.)
        Ret.back()->IndexOffset = Ret.back()->VertCount*sizeof(Vertex);

        Ret.back()->Bake();
    }
    
    pbrMesh** pRet = new pbrMesh*[Ret.size()];

    for(uint32_t i = 0; i < Ret.size(); i++) { pRet[i] = Ret[i]; }

    MeshCount = Ret.size();
    return pRet;
}
