#include "Mesh.hpp"
#include <vulkan/vulkan_core.h>

Instanced::Instanced() : MeshPassBuffer("Instanced Mesh Buffer")
{
}

Instanced::~Instanced()
{
    delete pMeshPassSet;
    pMeshPassSet = nullptr;
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
        pAgent->Transfer(&InstCount, sizeof(InstCount), &MeshPassBuffer, Offset); // update the instance count for the mesh draw parameters

        // update the instance list on the GPU side
        size_t InstanceListOffset =
        sizeof(VkDrawIndexedIndirectCommand) + // Draw command
        sizeof(uint32_t)*2;                    // Mesh Bounds and Instance Count
        pAgent->Transfer(Instances.data(), Instances.size()*sizeof(uint32_t), &MeshPassBuffer, InstanceListOffset);

        bInstanceDataDirty = false;
    }

    #ifdef DEBUG_MODE
        std::cout << "Indirect DrawCall as (Indirect*):\n";
            VkDrawIndexedIndirectCommand* pTmp = (VkDrawIndexedIndirectCommand*)MeshPassBuffer.pData;

            std::cout << "  indexCount " << ((uint32_t*)MeshPassBuffer.pData)[0] << '\n';
            std::cout << "  instanceCount " << ((uint32_t*)MeshPassBuffer.pData)[1] << '\n';
            std::cout << "  firstIndex " << ((uint32_t*)MeshPassBuffer.pData)[2] << '\n';
            std::cout << "  vertexOffset " << ((int32_t*)MeshPassBuffer.pData)[3] << '\n';
            std::cout << "  firstInstance " << ((uint32_t*)MeshPassBuffer.pData)[4] << '\n';
    #endif

    return;
}

pbrMesh::pbrMesh() : Instanced(), MeshBuffer("Mesh Buffer")
{
    if((VkDescriptorSetLayout)MeshLayout == VK_NULL_HANDLE)
    {
        VkDescriptorSetLayoutBinding AlbedoImg;
            AlbedoImg.binding = 1;
            AlbedoImg.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            AlbedoImg.descriptorCount = 1;
            AlbedoImg.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        MeshLayout.AddBinding(AlbedoImg);

        VkDescriptorSetLayoutBinding NormalImg;
            NormalImg.binding = 2;
            NormalImg.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            NormalImg.descriptorCount = 1;
            NormalImg.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        MeshLayout.AddBinding(NormalImg);
    }
}

void pbrMesh::Bake()
{
    size_t BuffSize = sizeof(VkDrawIndexedIndirectCommand)+(sizeof(uint32_t)*(MAX_RENDERABLE_INSTANCES+2)*2);
    CreateBuffer(MeshPassBuffer, BuffSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);

    #ifdef DEBUG_MODE
        Allocate(MeshPassBuffer, true);
        Map(&MeshPassBuffer);
    #else
        Allocate(MeshPassBuffer, false);
    #endif

    Resources::DescUpdate MeshPassUpdate{};
    MeshPassUpdate.Binding = 0;
    MeshPassUpdate.DescType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    MeshPassUpdate.DescCount = 1;
    MeshPassUpdate.DescIndex = 0;

    MeshPassUpdate.pBuff = &MeshPassBuffer;
    MeshPassUpdate.Range = BuffSize;
    MeshPassUpdate.Offset = 0;

    pMeshPassSet->Update(&MeshPassUpdate, 1);

    VkDrawIndexedIndirectCommand tmp{};
    tmp.indexCount = IndexCount;
    tmp.instanceCount = 0;
    tmp.firstIndex = 0;
    tmp.vertexOffset = 0;
    tmp.firstInstance = 0;
    
    GetTransferAgent()->Transfer(&tmp, sizeof(tmp), &MeshPassBuffer, 0);
}

void pbrMesh::AddInstance(uint32_t InstIdx)
{
    Instances.push_back(InstIdx);
    bInstanceDataDirty = true;
}

void pbrMesh::GenDraws(VkCommandBuffer* pCmdBuff, VkPipelineLayout Layout)
{
    vkCmdBindDescriptorSets(*pCmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, Layout, 1, 1, &pMeshPassSet->DescSet, 0, nullptr);
    vkCmdDispatch(*pCmdBuff, 1+((Instances.size()-1)/64), 1, 1);
}

void pbrMesh::DrawInstances(VkCommandBuffer* pCmdBuff, VkPipelineLayout Layout)
{
    vkCmdBindDescriptorSets(*pCmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, Layout, 1, 1, &pMeshPassSet->DescSet, 0, nullptr);
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

