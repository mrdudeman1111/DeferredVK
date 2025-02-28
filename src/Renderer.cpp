#include "Renderer.hpp"

#include <iostream>

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tgltf/tiny_gltf.h"

tinygltf::TinyGLTF MeshLoader;

Resources::DescriptorLayout pbrMesh::MeshLayout = {};

bool ExtractVtxComp(tinygltf::Model& Model, tinygltf::Primitive& Prim, std::string CompName, int CompCount, std::vector<float>& OutArr)
{
    auto Iter = Prim.attributes.find(CompName);

    if(Iter == Prim.attributes.end()) return false;

    tinygltf::Accessor& Accessor = Model.accessors[Iter->second];
    tinygltf::BufferView& BuffView = Model.bufferViews[Accessor.bufferView];
    tinygltf::Buffer& Buff = Model.buffers[BuffView.buffer];
    uint8_t* pData = Buff.data.data() + BuffView.byteOffset + Accessor.byteOffset;

    OutArr.resize(Accessor.count * CompCount);

    for(uint32_t i = 0; i < Accessor.count; i++)
    {
        memcpy(&OutArr[i*CompCount], pData + i, sizeof(float));
    }

    return true;
}

bool ExtractIdx(tinygltf::Model& Model, tinygltf::Primitive& Prim, std::vector<uint32_t>& Indices)
{
    if(Prim.indices < 0) return false;

    tinygltf::Accessor& Accessor = Model.accessors[Prim.indices];
    tinygltf::BufferView& BuffView = Model.bufferViews[Accessor.bufferView];
    tinygltf::Buffer& Buffer = Model.buffers[BuffView.buffer];

    uint8_t* pData = Buffer.data.data() + BuffView.byteOffset + Accessor.byteOffset;

    Indices.resize(Accessor.count);

    if(Accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
    {
        for(uint32_t i = 0; i < Accessor.count; i++)
        {
            Indices[i] = (uint32_t) (*(uint16_t*)pData[i]);
        }
    }
    else
    {
        for(uint32_t i = 0; i < Accessor.count; i++)
        {
            Indices[i] = (uint32_t) (*(uint16_t*)pData[i]);
        }
    }

    return true;
}

pbrMesh::pbrMesh()
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

void pbrMesh::Draw(VkCommandBuffer& cmdBuff, VkPipelineLayout Layout, uint32_t MeshDescriptorIdx)
{
    vkCmdBindDescriptorSets(cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, Layout, MeshDescriptorIdx, 1, &MeshDescriptor.DescSet, 0, nullptr);

    vkCmdBindVertexBuffers(cmdBuff, 0, 1, &MeshBuffer.Buff, 0);
    vkCmdBindIndexBuffer(cmdBuff, MeshBuffer.Buff, IndexOffset, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(cmdBuff, Indices.size(), 1, 0, 0, 0);

    return;
}

void PipeStage::Draw(Resources::CommandBuffer* pCmdBuffer)
{
    vkCmdBindPipeline(*pCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *Pipe);

    for(uint32_t i = 0; i < Drawables.size(); i++)
    {
        Drawables[i]->Draw(*pCmdBuffer);
    }
}

void FrameBufferChain::Bake(RenderPass* pPass)
{
    VkResult Err;

    Context* pCtx = GetContext();
    FBCount = GetWindow()->SwapchainAttachments.size();

    for(uint32_t i = 0; i < FBCount; i++)
    {
        FrameBuffers[i].AddBuffer(GetWindow()->SwapchainAttachments[i]);

        for(uint32_t x = 0; x < AttachmentDescs.size(); x++)
        {
            FrameBuffers[i].AddBuffer(AttachmentDescs[x]);
        }

        FrameBuffers[i].Bake(pPass->rPass);
    }

    return;
}

SceneRenderer::SceneRenderer()
{
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

    SceneProfile.AddBinding(0, sizeof(float)*8, VK_VERTEX_INPUT_RATE_VERTEX);
    SceneProfile.AddAttribute(0, VK_FORMAT_R32G32B32_SFLOAT, 0, 0);                   // Position
    SceneProfile.AddAttribute(0, VK_FORMAT_R32G32B32_SFLOAT, 1, sizeof(glm::vec3));   // Normal
    SceneProfile.AddAttribute(0, VK_FORMAT_R32G32_SFLOAT, 2, sizeof(glm::vec3)*2);    // UV

    DescriptorHeaps[SceneCam->WvpLayout.Layout].SetLayout(&SceneCam->WvpLayout);
    DescriptorHeaps[SceneCam->WvpLayout.Layout].Bake(10); // max camera count of 10

    SceneCam->pWvpUniform = DescriptorHeaps[SceneCam->WvpLayout.Layout].CreateSet();

    Resources::DescUpdate UpdateInfo{};
    UpdateInfo.DescType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    UpdateInfo.Binding = 0;
    UpdateInfo.DescCount = 1;
    UpdateInfo.DescIndex = 0;

    UpdateInfo.pBuff = &SceneCam->WvpBuffer;
    UpdateInfo.Range = sizeof(glm::mat4)*3;
    UpdateInfo.Offset = 0;

    SceneCam->pWvpUniform->Update(UpdateInfo);
}

Pipeline* SceneRenderer::CreatePipeline(std::string PipeName, uint32_t SubpassIdx, const char* VtxPath, const char* FragPath, uint32_t BlendAttCount, VkPipelineColorBlendAttachmentState* BlendAttachments, uint32_t DescriptorCount, Resources::DescriptorLayout* pDescriptors, PipelineProfile* pProfile)
{
    if(PipeStages[PipeName]->Pipe == nullptr)
    {
        Pipeline* Ret = new Pipeline();

        Ret->AddDescriptor(&SceneCam->WvpLayout);

        for(uint32_t i = 0; i < DescriptorCount; i++)
        {
            Ret->AddDescriptor(&pDescriptors[i]);
        }

        for(uint32_t i = 0; i < BlendAttCount; i++)
        {
            Ret->AddAttachmentBlending(BlendAttachments[i]);
        }

        Ret->SetProfile(SceneProfile);

        Ret->Bake(&ScenePass, SubpassIdx, VtxPath, FragPath);

        PipeStages[PipeName]->Pipe = Ret;

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

        return Ret;
    }
    else
    {
        std::cout << "Tried to create the " << PipeName << ", but a pipeline of the same name already exists.\n";
        return PipeStages[PipeName]->Pipe;
    }

    return nullptr;
}

pbrMesh* SceneRenderer::CreateDrawable(std::string Path, std::string PipeName)
{
    pbrMesh* Ret = new pbrMesh();
 
    tinygltf::Model tmpModel;

    std::string Err, Warn;

    if(MeshLoader.LoadASCIIFromFile(&tmpModel, &Err, &Warn, Path))
    {
        throw std::runtime_error("Failed to load mesh file " + Path);
    }

    if(tmpModel.scenes.empty())
    {
        std::cout << "Tried to create drawable mesh, but couldn't due to an empty scene.";
        return nullptr;
    }

    tinygltf::Scene& MeshScene = tmpModel.scenes[(tmpModel.defaultScene > -1) ? tmpModel.defaultScene : 0];
    for(int NodeIdx : MeshScene.nodes)
    {
        // skip the node if there is no mesh
        if(tmpModel.nodes[NodeIdx].mesh < 0) continue;

        tinygltf::Mesh& Mesh = tmpModel.meshes[tmpModel.nodes[NodeIdx].mesh];

        for(tinygltf::Primitive& Prim : Mesh.primitives)
        {
            std::vector<float> tmpVec;

            ExtractVtxComp(tmpModel, Prim, "POSITION", 3, tmpVec);

            Ret->Vertices.resize(tmpVec.size());
            for(uint32_t i = 0; i < tmpVec.size()/3; i++)
            {
                Ret->Vertices[i].Position.x = tmpVec[i];
                Ret->Vertices[i].Position.y = tmpVec[i+1];
                Ret->Vertices[i].Position.z = tmpVec[i+2];
            }

            ExtractVtxComp(tmpModel, Prim, "NORMAL", 3, tmpVec);

            for(uint32_t i = 0; i < tmpVec.size()/3; i++)
            {
                Ret->Vertices[i].Normal.x = tmpVec[i];
                Ret->Vertices[i].Normal.y = tmpVec[i+1];
                Ret->Vertices[i].Normal.z = tmpVec[i+2];
            }

            ExtractVtxComp(tmpModel, Prim, "TEXCOORD_0", 2, tmpVec);
            
            for(uint32_t i = 0; i < tmpVec.size()/2; i++)
            {
                Ret->Vertices[i].UV.x = tmpVec[i];
                Ret->Vertices[i].UV.y = tmpVec[i+1];
            }
        }
    }

    CreateBuffer(Ret->MeshBuffer, (Ret->Vertices.size() * sizeof(Vertex))+ (Ret->Indices.size() * sizeof(uint32_t)), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    Allocate(Ret->MeshBuffer, false);

    if(pTransit == nullptr)
    {
        CreateBuffer(TransitBuffer, 4194304, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        Allocate(TransitBuffer, true);
        Map(&TransitBuffer);
    }

    memcpy(TransitBuffer.pData, Ret->Vertices.data(), Ret->Vertices.size()*sizeof(Vertex));
    memcpy(((char*)TransitBuffer.pData)+(Ret->Vertices.size()*sizeof(Vertex)), Ret->Indices.data(), Ret->Indices.size()*sizeof(uint32_t));

    Ret->IndexOffset = Ret->Vertices.size()*sizeof(Vertex);

    Resources::CommandBuffer* cmdTransfer = TransferHeap.CreateBuffer();

    cmdTransfer->Start();

        VkBufferCopy CopyInf{};
        CopyInf.srcOffset = 0;
        CopyInf.dstOffset = 0;
        CopyInf.size = (Ret->Vertices.size() * sizeof(Vertex)) + (Ret->Indices.size() * sizeof(uint32_t));

        vkCmdCopyBuffer(*cmdTransfer, TransitBuffer.Buff, Ret->MeshBuffer.Buff, 1, &CopyInf);

    cmdTransfer->Stop();

    TransferHeap.Submit(cmdTransfer, &cmdTransfer->Fence);

    PipeStages[PipeName]->Drawables.push_back(Ret);

    return Ret;
}

void SceneRenderer::AddPass(Subpass* pSubpass)
{
    ScenePass.AddPass(*pSubpass);
    return;
}

void SceneRenderer::Bake()
{
    ScenePass.Bake();
    FrameChain.Bake(&ScenePass);

    SceneSync.FrameSem = CreateSemaphore();
    SceneSync.RenderSem = CreateSemaphore();
}

void SceneRenderer::Render()
{
    if(pRenderBuffer == nullptr)
    {
        pRenderBuffer = GraphicsHeap.CreateBuffer();
    }

    uint32_t FrameIdx = GetWindow()->GetNextFrame(SceneSync.FrameSem);

    ScenePass.Begin(*pRenderBuffer, FrameChain.FrameBuffers[FrameIdx]);
        for(uint32_t i = 0; i < PassStages.size(); i++)
        {
            for(uint32_t x = 0; x < PassStages[i].PipeStages.size(); x++)
            {
                vkCmdBindDescriptorSets(*pRenderBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, PassStages[i].PipeStages[x]->Pipe->PipeLayout, 0, 1, &SceneCam->pWvpUniform->DescSet, 0, nullptr);
                PassStages[i].PipeStages[x]->Draw(pRenderBuffer);
            }

            vkCmdNextSubpass(*pRenderBuffer, VK_SUBPASS_CONTENTS_INLINE);
        }
    ScenePass.End(*pRenderBuffer);
}
