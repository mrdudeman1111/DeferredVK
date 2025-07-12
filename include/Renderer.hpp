#pragma once

#include "Framework.hpp"
#include "Input.hpp"

#include "glm/gtc/matrix_transform.hpp"

#include <iostream>
#include <unordered_map>
#include <stdexcept>

#define MAX_RENDERABLE_INSTANCES 600

// seperate drawables and meshes.

/*! Vertex structure containing all needed attributes */
struct Vertex
{
    glm::vec3 Position; //! > Position of the vertex in 3D-space
    glm::vec3 Normal; //! > Facing normal of the vertex.
    glm::vec2 UV; //! > UV coordinates of the vertex, for mapping to textures.
};

/*! Texture structure containing everything needed to use an image as a mesh texture (or 2D texture) */
struct Texture
{
    Resources::Image* Img = nullptr;
};

// todo: implement convex hulls as culling shapes.
struct CullingBox
{
    glm::vec3 Vertices[4];
};

struct pbrMeshDescriptors
{
    VkDrawIndirectCommand cmdIndirectDraw;
    uint32_t MeshBoundIdx;
    uint32_t InstCount;
    std::vector<uint32_t> MeshInstances;
};

class Cullable
{
    CullingBox CullBound;
};

class Instanced
{
public:
    Instanced() : MeshPassBuffer("Instanced Mesh Buffer") {}

    static Resources::DescriptorLayout* pMeshPassLayout;

    virtual void GenDraws(VkCommandBuffer* pCmdBuffer, VkPipelineLayout Layout) = 0;
    virtual void DrawInstances(VkCommandBuffer* pCmdBuffer, VkPipelineLayout Layout) = 0;
    virtual void AddInstance(uint32_t InstanceIndex) = 0;
    void Update();

    Resources::DescriptorSet* pMeshPassSet;

protected:
    bool bInstanceDataDirty;

    std::vector<uint32_t> Instances;
    Resources::Buffer MeshPassBuffer;
};

/*! Contains physical description of a mesh */
class pbrMesh : public Instanced
{
public:
    Resources::DescriptorLayout MeshLayout;
    Resources::DescriptorSet MeshSet;
    CullingBox CullBound;

    pbrMesh();
    ~pbrMesh()
    {
        delete[] pVertices;
        pVertices = nullptr;
        delete[] pIndices;
        pIndices = nullptr;
    }

    void Bake();

    Vertex* pVertices;
    uint32_t VertCount;

    uint32_t* pIndices;
    uint32_t IndexCount;

    /* Inherited from instance */
        /*! \brief Generates the draw commands (and performs occlusion and frustum culling) for the instances*/
        void GenDraws(VkCommandBuffer* pCmdBuffer, VkPipelineLayout Layout);

        /*! \brief Renders all visible instances of this mesh. */
        void DrawInstances(VkCommandBuffer* pCmdBuff, VkPipelineLayout Layout);

        void AddInstance(uint32_t InstanceIndex);

    /*! \brief Byte offset of the index array in the mesh buffer. */
    uint32_t IndexOffset;
    Resources::Buffer MeshBuffer;

private:

    Texture Albedo = {};
    Texture Normal = {};
};

/*! Contains information needed for mesh instance to be rendered. */
class Drawable
{
public:
    Drawable(Resources::Buffer* SceneBuffer) : pSceneBuffer(SceneBuffer) {};

    void SetTransform(glm::vec3 Position = {0.f, 0.f, 0.f}, glm::vec3 Rotation = {0.f, 0.f, 0.f}, glm::vec3 Scale = {1.f, 1.f, 1.f});

    void Translate(glm::vec3 Translation);

    void Rotate(glm::vec3 Rotation);

    void Scale(glm::vec3 Scale);

    void UpdateTransform();

    bool bStatic;
    uint32_t ObjIdx;

    Resources::Buffer* pSceneBuffer;

    glm::mat4 Transform;

    pbrMesh* pMesh;
};

//! Pipeline stage in a renderpass.
/*!
 Contains all drawables assigned to the wrapped pipeline. Also contains the index of the subpass this pipeline was built against.
*/
struct PipeStage
{
    // Call when 
    void UpdateDraws(Resources::CommandBuffer* pCmdBuffer, VkPipelineLayout ComputeLayout);
    void Draw(Resources::CommandBuffer* pCmdBuffer);

    Pipeline* Pipe = nullptr;
    uint32_t PassIdx;
    std::vector<pbrMesh*> Meshes;
};

//! Subpass stages.
/*!
 Contains all associated Pipeline stages.
*/
struct PassStage
{
    std::vector<PipeStage*> PipeStages;
};

//! Camera structure.
class Camera
{
public:
    Camera() : WvpBuffer("Camera WVP Buffer")
    {
        CreateBuffer(WvpBuffer, (sizeof(glm::mat4)*3)+(sizeof(glm::vec4)*6), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

        Allocate(WvpBuffer, true);

        Map(&WvpBuffer);

        MoveSpeed = 0.5f;

        CamMat = glm::mat4(1.f);

        Position = glm::vec3(0.f, 0.f, 5.f);
        Rotation = glm::vec3(0.f);

        pWvp = (glm::mat4*)WvpBuffer.pData;
        pWvp[0] = glm::mat4(1.f);
        pWvp[2] = glm::perspective(45.f, 16.f/9.f, 0.f, 9999.f);
        pWvp[2][1][1] *= -1.f;
    }

    void Update()
    {
        CamMat = glm::rotate(glm::mat4(1.f), Rotation.y, glm::vec3(0.f, 1.f, 0.f)); // rotate along the y-axis (up)
        CamMat = glm::rotate(CamMat, Rotation.x, glm::vec3(1.f, 0.f, 0.f)); // rotate along the x-axis (right)

        CamMat = glm::translate(CamMat, Position);

        pWvp[0] = glm::inverse(CamMat);
    }

    void Move()
    {
        glm::vec3 Move(0.f);

        if(Input::GetInputMap()->Forward)
        {
            Move += glm::vec3(CamMat[0][2], CamMat[1][2], CamMat[2][2]);
        }
        if(Input::GetInputMap()->Back)
        {
            Move -= glm::vec3(CamMat[0][2], CamMat[1][2], CamMat[2][2]);
        }
        if(Input::GetInputMap()->Right)
        {
            Move += glm::vec3(CamMat[0][0], CamMat[1][0], CamMat[2][0]);
        }
        if(Input::GetInputMap()->Left)
        {
            Move -= glm::vec3(CamMat[0][0], CamMat[1][0], CamMat[2][0]);
        }

        if(glm::length(Move) == 0.f)
        {
            return;
        }

        Move = glm::normalize(Move);

        Position += Move*MoveSpeed;

        return;
    }

    void Rotate()
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

        Rotation.x += MouseDelta.x/40.f;
        Rotation.y += MouseDelta.y/40.f;

        return;
    }

    void GenPlanes()
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

    Resources::Buffer WvpBuffer;

    glm::mat4 CamMat;

private:
    glm::vec3 Position, Rotation;
    glm::mat4* pWvp;
    glm::vec4 Planes[6];
    float MoveSpeed = 0.5f;
    glm::vec2 PrevMouse;
};

/*! Abstracts framebuffer creation by creating all images, views, and framebuffers from basic descriptions of the attachments provided by the user. */
class FrameBufferChain
{
public:
    void Bake(RenderPass* pPass, VkCommandBuffer* pCmdBuffer);
    inline void AddAtt(VkImageCreateInfo AttDesc) { AttachmentDescs.push_back(AttDesc); }

    std::vector<Resources::FrameBuffer> FrameBuffers;

private:
    std::vector<VkImageCreateInfo> AttachmentDescs;

    uint32_t FBCount;
};

/*! Contains heaps(arrays of allocator memory wrappers), pipelines, renderpasses, and drawables and renders them to the screen */
class SceneRenderer
{
public:
    SceneRenderer();
    ~SceneRenderer();

    Pipeline* CreatePipeline(std::string PipeName, uint32_t SubpassIdx, const char* VtxPath, const char* FragPath, uint32_t BlendAttCount = 0, VkPipelineColorBlendAttachmentState* BlendAttachments = nullptr, uint32_t DescriptorCount = 0, Resources::DescriptorLayout* pDescriptors = nullptr, PipelineProfile* pProfile = nullptr);

    void AddMesh(pbrMesh* Mesh, std::string PipeName);
    Drawable* CreateDrawable(pbrMesh* pMesh, bool bDynamic);

    inline void AddFrameBufferAttachment(VkFormat Format, VkImageLayout AttachmentLayout, VkImageUsageFlagBits Usage)
    {
        VkImageCreateInfo AttachentImg{};
        AttachentImg.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        AttachentImg.extent = { GetWindow()->Resolution.width, GetWindow()->Resolution.height, 1 };
        AttachentImg.arrayLayers = 1;
        AttachentImg.imageType = VK_IMAGE_TYPE_2D;
        AttachentImg.initialLayout = AttachmentLayout; // this is changed to undefined when we bake the frambufferchain, it is saved, then transfered back to this layout with a memory barrier.
        AttachentImg.mipLevels = 1;
        AttachentImg.samples = VK_SAMPLE_COUNT_1_BIT;
        AttachentImg.tiling = VK_IMAGE_TILING_OPTIMAL;
        AttachentImg.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        AttachentImg.format = Format;
        AttachentImg.usage = Usage;

        FrameChain.AddAtt(AttachentImg);
    }

    inline void AddRenderAttachment(VkFormat Format, VkImageLayout InitLay, VkImageLayout FinLay, VkAttachmentStoreOp StoreOp, VkAttachmentLoadOp LoadOp, VkAttachmentStoreOp StencilStoreOp, VkAttachmentLoadOp StencilLoadOp, VkClearValue* ClearValue, VkSampleCountFlagBits Samples)
    {
        if(LoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR && ClearValue == nullptr)
        {
            throw std::runtime_error("Scene Renderer : Tried to add a renderpass attachment with no clear value even though LoadOp is set to CLEAR");
        }

        ScenePass.AddAttachmentDesc(Format, InitLay, FinLay, StoreOp, LoadOp, StencilStoreOp, StencilLoadOp, ClearValue, Samples);
    }

    void AddPass(Subpass* pSubpass);

    void Bake();

    void Render();

    /* Descriptors */
        std::unordered_map<VkDescriptorSetLayout, Allocators::DescriptorPool> DescriptorHeaps; //! > map assigns each type of descriptorsetlayout in the scene a corresponding descriptor pool

    /* Command Heaps */
        Allocators::CommandPool GraphicsHeap; //! > Graphics command buffer allocator
        Allocators::CommandPool ComputeHeap; //! > Compute command buffer allocator
        Allocators::CommandPool TransferHeap; //! > Transfer command buffer allocator

    /* Scene Render Information */
        PipelineProfile SceneProfile; //! > Scene-wide pipeline profile.
        Camera* SceneCam; //! > Scene camera structure.

    Resources::DescriptorSet SceneSet;

private:
    /* Scene Pass */
        struct
        {
            VkSemaphore RenderSem;
            VkSemaphore DrawGenSem;
            Resources::Fence* pFrameFence;
        } SceneSync;

        RenderPass ScenePass; //! > Scene wide renderpass. Passes can be added to the scene through AddPass().

    /* FrameBuffers */
        FrameBufferChain FrameChain; //! > Scene framebuffer chain. Attachments can be added through AddFrameBufferAttachment()


    /* Scene Descriptor contains scene info

        binding 0:

         Contain information about the camera, such as the WVP matrices and the frustum planes for frustum culling.

            layout(std140, set = 0, binding = 0) uniform Camerastructure
            {
                mat4 View;
                mat4 Proj;
                mat4 ProjView;
                vec4 Planes[6];
            } Camera;

        binding 1:

         Contains static mesh instances in the scene

            layout(std140, set = 0, binding = 1) buffer StaticSceneStruct
            {
                mat4 Transforms[];
                uint IDs[];
            } StaticSceneBuffer;

        binding 2:

         Contains dynamic mesh instances in the scene

            layout(std140, set = 0, binding = 1) buffer DynamicSceneStruct
            {
                mat4 Transforms[];
                uint IDs[];
            } DynamicSceneBuffer;

        binding 3:

         Contains mesh culling volumes (OBBs). Should be transformed by mesh transforms to get correct position/orientation/scale

            layout(std140, set = 0, binding = 3) buffer readonly globInstBounds
            {
                struct CullBounds
                {
                    float Width;
                    float Height;
                } MeshCullBounds[];
            } Bounds;
    */

    Resources::DescriptorLayout* pSceneDescriptorLayout;
    Resources::DescriptorSet* pSceneDescriptorSet;

    Resources::Buffer StaticSceneBuffer; //! > Static scene object positions (10000).
    uint32_t StaticIter = 0; // Index Iterator

    Resources::Buffer DynamicSceneBuffer; //! > Dynamic scene object positions (5000).
    uint32_t DynamicIter = 0; // Index Iterator

    std::unordered_map<std::string, PipeStage*> PipeStages; //! > Pipeline stages (Pipeline+Drawables) mapped to string names

    /***************** TODO ******************/
    //  Change pass stages "PipeStages" variable to an array of names that can be used to index into renderer's "PipeStages" map.
    /*****************************************/
    std::vector<PassStage> PassStages; //! > Pipeline stages sorted by subpass.

    ComputePipeline DrawPipeline;

    Resources::CommandBuffer* pCmdOpsBuffer = nullptr; //! > General purpose spare command buffer.
    Resources::CommandBuffer* pCmdComputeBuffer = nullptr; //! > Compute render buffer (mostly used for command generation).
    Resources::CommandBuffer* pCmdRenderBuffer = nullptr; //! > Render buffer.
};

class AssetManager
{
    public:
        /*! \brief Loads a mesh from a mesh file (.glb or .gltf)
        *
        *   Uses tinygltf to load a gltf file and registers the mesh with the specified pipeline.
        */
        pbrMesh* CreateMesh(std::string Path, std::string PipeName);

        SceneRenderer* pRenderer;
};
