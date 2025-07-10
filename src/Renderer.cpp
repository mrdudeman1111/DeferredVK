#include "Renderer.hpp"

#include <iostream>
#include <algorithm>

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

    tinygltf::Accessor& Accessor = Model.accessors[Iter->second]; // the component can be found in the accessor, so change compcount to an output variable later.
    tinygltf::BufferView& BuffView = Model.bufferViews[Accessor.bufferView];
    tinygltf::Buffer& Buff = Model.buffers[BuffView.buffer];
    uint8_t* pData = Buff.data.data() + BuffView.byteOffset + Accessor.byteOffset;
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
    OutArr.resize(Accessor.count * CompCount);

    for(uint32_t i = 0; i < Accessor.count/*-1*/; i++)
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

    tinygltf::Accessor& Accessor = Model.accessors[Prim.indices];
    tinygltf::BufferView& BuffView = Model.bufferViews[Accessor.bufferView];
    tinygltf::Buffer& Buffer = Model.buffers[BuffView.buffer];

    uint16_t* pData = (uint16_t*)(Buffer.data.data() + BuffView.byteOffset + Accessor.byteOffset);

    Indices.resize(Accessor.count);

    if(Accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
    {
        for(uint32_t i = 0; i < Accessor.count; i++)
        {
            Indices[i] = (uint32_t) (*(pData+i));
        }
    }
    else
    {
        for(uint32_t i = 0; i < Accessor.count; i++)
        {
            Indices[i] = *(((uint32_t*)pData)+i);
        }
    }

    return true;
}

void Instanced::Update()
{
    TransferAgent* pAgent = GetTransferAgent();
    uint32_t Zero = 0;
    pAgent->Transfer(&Zero, sizeof(uint32_t), &MeshPassBuffer, offsetof(VkDrawIndexedIndirectCommand, instanceCount));

    if(bInstanceDataDirty)
    {
        uint32_t InstCount = Instances.size();
        size_t Offset = sizeof(VkDrawIndexedIndirectCommand)+sizeof(uint32_t);
        pAgent->Transfer(&InstCount, sizeof(InstCount), &MeshPassBuffer, Offset);
        bInstanceDataDirty = false;
    }
}

pbrMesh::pbrMesh() : MeshBuffer("Mesh Buffer")
{
    if(MeshLayout.Layout == VK_NULL_HANDLE)
    {
        VkDescriptorSetLayoutBinding SceneTransforms;
            SceneTransforms.binding = 1;
            SceneTransforms.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            SceneTransforms.descriptorCount = 1;
            SceneTransforms.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        MeshLayout.AddBinding(SceneTransforms);

        VkDescriptorSetLayoutBinding AlbedoImg;
            AlbedoImg.binding = 2;
            AlbedoImg.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            AlbedoImg.descriptorCount = 1;
            AlbedoImg.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        MeshLayout.AddBinding(AlbedoImg);

        VkDescriptorSetLayoutBinding NormalImg;
            NormalImg.binding = 3;
            NormalImg.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            NormalImg.descriptorCount = 1;
            NormalImg.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        MeshLayout.AddBinding(NormalImg);
    }
}

void pbrMesh::Bake()
{
    CreateBuffer(MeshPassBuffer, sizeof(VkDrawIndexedIndirectCommand)+(sizeof(uint32_t)*(MAX_RENDERABLE_INSTANCES+2)), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
    Allocate(MeshPassBuffer, false);

    VkDrawIndexedIndirectCommand tmp;
    tmp.firstIndex = 0;
    tmp.firstInstance = 0;
    tmp.indexCount = IndexCount;
    tmp.instanceCount = 0;
    tmp.vertexOffset = 0;

    GetTransferAgent()->Transfer(&tmp, sizeof(tmp), &MeshPassBuffer, 0);

    Resources::DescUpdate MeshPassUpdate{};
    MeshPassUpdate.Binding = 0;
    MeshPassUpdate.DescType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    MeshPassUpdate.DescCount = 1;
    MeshPassUpdate.DescIndex = 0;

    MeshPassUpdate.pBuff = &MeshPassBuffer;
    MeshPassUpdate.Range = MeshPassBuffer.Alloc.Size;
    MeshPassUpdate.Offset = 0;

    pMeshPassSet->Update(&MeshPassUpdate, 1);
}

void pbrMesh::AddInstance(uint32_t InstIdx)
{
    Instances.push_back(InstIdx);
    bInstanceDataDirty = true;
    Update();
}

void pbrMesh::GenDraws(VkCommandBuffer* pCmdBuff, VkPipelineLayout Layout)
{
    vkCmdBindDescriptorSets(*pCmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, Layout, 1, 1, &pMeshPassSet->DescSet, 0, nullptr);
    vkCmdDispatch(*pCmdBuff, 1+(Instances.size()-1)/64, 1, 1);
}

void pbrMesh::DrawInstances(VkCommandBuffer* pCmdBuff, VkPipelineLayout Layout)
{
    /*
        vkCmdBindDescriptorSets(cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, Layout, MeshDescriptorIdx, 1, &MeshSet.DescSet, 0, nullptr);
    */
    VkDeviceSize Offset = 0;
    vkCmdBindVertexBuffers(*pCmdBuff, 0, 1, MeshBuffer, &Offset);
    vkCmdBindIndexBuffer(*pCmdBuff, MeshBuffer, IndexOffset, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexedIndirect(*pCmdBuff, MeshPassBuffer, 0, 1, sizeof(VkDrawIndexedIndirectCommand));

    return;
}

void Drawable::SetTransform(glm::vec3 Position, glm::vec3 Rotation, glm::vec3 Scale)
{
    glm::mat4 tmp = glm::mat4(1.f);

    tmp = glm::scale(tmp, Scale);

    // Apply Yaw, pitch, then roll.
    tmp = glm::rotate(tmp, Rotation.y, glm::vec3(0.f, 1.f, 0.f));
    tmp = glm::rotate(tmp, Rotation.x, glm::vec3(1.f, 0.f, 0.f));
    tmp = glm::rotate(tmp, Rotation.z, glm::vec3(0.f, 0.f, 1.f));

    tmp = glm::translate(tmp, Position);

    Transform = tmp;
}

void Drawable::Translate(glm::vec3 Translation)
{
    Transform = glm::translate(Transform, Translation);
}

void Drawable::Rotate(glm::vec3 Rotation)
{
    Transform = glm::rotate(Transform, Rotation.y, glm::vec3(0.f, 1.f, 0.f));
    Transform = glm::rotate(Transform, Rotation.x, glm::vec3(1.f, 0.f, 0.f));
    Transform = glm::rotate(Transform, Rotation.z, glm::vec3(0.f, 0.f, -1.f));
}

void Drawable:: Scale(glm::vec3 Scale)
{
    Transform = glm::scale(Transform, Scale);
}

void Drawable::UpdateTransform()
{
    if(bStatic)
    {
        GetTransferAgent()->Transfer(&Transform, sizeof(glm::mat4), pSceneBuffer, ObjIdx*sizeof(glm::mat4));
    }
    else
    {
        memcpy(((glm::mat4*)pSceneBuffer->pData)+ObjIdx, &Transform, sizeof(Transform));
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
    SceneProfile.AddAttribute(0, VK_FORMAT_R32G32B32_SFLOAT, 0, 0);                   // Position
    SceneProfile.AddAttribute(0, VK_FORMAT_R32G32B32_SFLOAT, 1, sizeof(glm::vec3));   // Normal
    SceneProfile.AddAttribute(0, VK_FORMAT_R32G32_SFLOAT, 2, sizeof(glm::vec3)*2);    // UV

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
    MeshPassLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    Instanced::pMeshPassLayout = new Resources::DescriptorLayout();
    Instanced::pMeshPassLayout->AddBinding(MeshPassLayoutBinding);
    DescriptorHeaps[Instanced::pMeshPassLayout->Layout].Bake(Instanced::pMeshPassLayout, 1000);

    if((Err = CreateBuffer(StaticSceneBuffer, sizeof(glm::mat4)*10000, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)) != VK_SUCCESS) throw std::runtime_error("Failed to create static scene buffer.");
    Allocate(StaticSceneBuffer, false);

    Err = CreateBuffer(DynamicSceneBuffer, sizeof(glm::mat4)*5000, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
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

    DescriptorHeaps[pSceneDescriptorLayout->Layout].Bake(pSceneDescriptorLayout, 2);
    pSceneDescriptorSet = DescriptorHeaps[pSceneDescriptorLayout->Layout].CreateSet();

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
        PipeStages[PipeName] = new PipeStage();
        Pipeline* Pipe = new Pipeline();

        Pipe->AddDescriptor(pSceneDescriptorLayout);

        for(uint32_t i = 0; i < DescriptorCount; i++)
        {
            Pipe->AddDescriptor(&pDescriptors[i]);
        }

        for(uint32_t i = 0; i < BlendAttCount; i++)
        {
            Pipe->AddAttachmentBlending(BlendAttachments[i]);
        }

        Pipe->SetProfile(SceneProfile);

        Pipe->Bake(&ScenePass, SubpassIdx, VtxPath, FragPath);

        PipeStages[PipeName]->Pipe = Pipe;

        uint32_t i = 0;

        if((i = (PassStages.size() - SubpassIdx+1)) > 0)
        {
            while(i != 0)
            {
                PassStages.push_back({});
                i--;
            }
        }

        PassStages[SubpassIdx].PipeStages.push_back(PipeStages[PipeName]);
        return Pipe;
    }
    else
    {
        std::cout << "Tried to create the " << PipeName << ", but a pipeline of the same name already exists.\n";
        return PipeStages[PipeName]->Pipe;
    }

    return nullptr;
}

void SceneRenderer::AddMesh(pbrMesh* pMesh, std::string PipeName)
{
    pMesh->pMeshPassSet = DescriptorHeaps[pMesh->pMeshPassLayout->Layout].CreateSet();
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

    SceneSync.RenderSem = CreateVulkanSemaphore();
    SceneSync.DrawGenSem = CreateVulkanSemaphore();
    SceneSync.pFrameFence = CreateFence();

    pCmdComputeBuffer = ComputeHeap.CreateBuffer();

    pCmdOpsBuffer = GraphicsHeap.CreateBuffer();
    pCmdOpsBuffer->Start();

        FrameChain.Bake(&ScenePass, *pCmdOpsBuffer);

        uint32_t FrameCount = FrameChain.FrameBuffers.size();
        VkImageMemoryBarrier SwapchainBarriers[FrameCount] = {};

        for(uint32_t i = 0; i < FrameCount; i++)
        {
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

void SceneRenderer::Render()
{
    if(pCmdRenderBuffer == nullptr)
    {
        pCmdRenderBuffer = GraphicsHeap.CreateBuffer();
    }

    TransferAgent* pTransfer = GetTransferAgent();
    Context* pCtx = GetContext();

    uint32_t FrameIdx = GetWindow()->GetNextFrame(SceneSync.pFrameFence);

    pCmdRenderBuffer->cmdFence->Wait(); // wait for previous frame to render

    SceneCam->Rotate();
    SceneCam->Move();

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
    GraphicsHeap.Submit(pCmdRenderBuffer, 1, &SceneSync.RenderSem, 1, &SceneSync.DrawGenSem, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    GetWindow()->PresentFrame(FrameIdx, &SceneSync.RenderSem);

    pCmdComputeBuffer->cmdFence->Wait();
}

pbrMesh* AssetManager::CreateMesh(std::string Path, std::string PipeName)
{
    // initiate stack variables
    pbrMesh* Ret = new pbrMesh();
 
    tinygltf::Model tmpModel;

    std::string Err, Warn;

    // load the mesh file
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

        // process the mesh primitives' vertices.
        for(tinygltf::Primitive& Prim : Mesh.primitives)
        {
            std::vector<float> tmpPos;
            std::vector<float> tmpNorm;
            std::vector<float> tmpUV;

            ExtractVtxComp(tmpModel, Prim, "POSITION", 3, tmpPos);
            ExtractVtxComp(tmpModel, Prim, "NORMAL", 3, tmpNorm);
            ExtractVtxComp(tmpModel, Prim, "TEXCOORD_0", 2, tmpUV);

            Ret->pVertices = new Vertex[tmpPos.size()/3];
            Ret->VertCount = tmpPos.size()/3;

            for(uint32_t i = 0; i < tmpPos.size()/3; i++)
            {
                uint32_t Vec3Idx = i*3;
                uint32_t Vec2Idx = i*2;

                Ret->pVertices[i].Position.x = tmpPos[ Vec3Idx ];
                Ret->pVertices[i].Position.y = tmpPos[ Vec3Idx+1 ];
                Ret->pVertices[i].Position.z = tmpPos[ Vec3Idx+2 ];

                Ret->pVertices[i].Normal.x = tmpNorm[ Vec3Idx ];
                Ret->pVertices[i].Normal.y = tmpNorm[ Vec3Idx+1 ];
                Ret->pVertices[i].Normal.z = tmpNorm[ Vec3Idx+2 ];

                Ret->pVertices[i].UV.x = tmpUV[ Vec2Idx ];
                Ret->pVertices[i].UV.y = tmpUV[ Vec2Idx+1 ];
            }

            std::vector<uint32_t> tmpIdx;

            ExtractIdx(tmpModel, Prim, tmpIdx);

            Ret->pIndices = new uint32_t[tmpIdx.size()];
            Ret->IndexCount = tmpIdx.size();

            for(uint32_t i = 0; i < tmpIdx.size(); i++)
            {
                Ret->pIndices[i] = tmpIdx[i];
            }
        }
    }

    // create the mesh buffer (for vertices and indices)
    CreateBuffer(Ret->MeshBuffer, (Ret->VertCount * sizeof(Vertex))+ (Ret->IndexCount * sizeof(uint32_t)), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    Allocate(Ret->MeshBuffer, false);

    GetTransferAgent()->Transfer((void*)Ret->pVertices, Ret->VertCount*sizeof(Vertex), &Ret->MeshBuffer, 0);
    GetTransferAgent()->Transfer((void*)Ret->pIndices, Ret->IndexCount, &Ret->MeshBuffer, Ret->VertCount*sizeof(Vertex));

    pRenderer->AddMesh(Ret, PipeName);

    // Store the byte offset of the indices (needed during render.)
    Ret->IndexOffset = Ret->VertCount*sizeof(Vertex);

    Ret->Bake();

    return Ret;
}
