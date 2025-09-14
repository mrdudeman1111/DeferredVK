#pragma once

#include "Input.hpp"
#include "Mesh.hpp"

#include <iostream>
#include <unordered_map>
#include <stdexcept>

#define MAX_STATIC_SCENE_SIZE 10000
#define MAX_DYNAMIC_SCENE_SIZE 5000

typedef uint32_t PointLight;

//! Pipeline stage in a renderpass.
/*!
 Contains all drawables assigned to the wrapped pipeline. Also contains the index of the subpass this pipeline was built against.
*/
struct PipeStage
{
    /*! \brief Call to update all owned meshes. (called every frame to reset draw information, this function must be called before UpdateDraws())
    */
    void Update();

    /*! \brief Call to regenerate draw lists (called every frame so that culling can be performed).
     
        Exclusively for internal use by the renderer. The compute layout should only point to the 
     
        @param pCmdBuffer A pointer to the Resources::CommandBuffer object to hold this command.
        @param ComputeLayout The pipeline layout of the compute pipeline to use.
    */
    void UpdateDraws(Resources::CommandBuffer* pCmdBuffer, VkPipelineLayout ComputeLayout);

    // Call to Draw all owned meshes with this pipeline.
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
    void Update();

    std::vector<PipeStage*> PipeStages;
};

//! Camera structure.
class Camera
{
public:
    Camera();

    void Update();

    void Move();

    void Rotate();

    void GenPlanes();

    Resources::Buffer WvpBuffer;

    glm::mat4 CamMat;

private:
    glm::vec3 Position, Rotation;
    glm::mat4* pWvp;
    glm::vec4 Planes[6];
    float MoveSpeed;
    glm::vec2 PrevMouse;
};

/*! Helper structure for The renderer. Abstracts framebuffer creation by creating all images, views, and framebuffers from basic descriptions of the attachments provided by the user. */
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

    /*! \brief Create a graphics pipeline with the specified configuration information
     *
     *  Creates a graphics pipeline named (PipeName) with (VtxPath) as the vertex shader and (FragPath) as the fragment shader. The pipeline uses the framebuffer attachments specified in subpass[(SubpassIdx)] of the scene render pass. The output of this graphics pipeline to each attachment in the framebuffer are added/mixed/written according to the corresponding (BlendAttachments[i] where 'i' is the attachment index). There should always be one more BlendAttachment than calls to AddFrameBufferAttachment, since there is an implicit frame buffer attachment, that being the swapchain image. The pipeline also uses (DescriptorCount) descriptors specified in the (pDescriptors) array. So any descriptor with the same Layout in (pDescriptors) at index i can be bound at location i during a draw using this pipeline. Scene wide information like rasterization method, multisample count, resolution and more are all provided through (pProfile).
     *
     * @param PipeName The name of this pipeline.
     * @param SubpassIdx The index of the subpass (in the Scene render pass) that this pipeline will operate in.
     * @param VtxPath The path to the vertex shader to use for this pipeline.
     * @param FragPath The path to the fragment shader to use for this pipeline.
     * @param BlendAttCount The number of elements in the BlendAttachments array.
     * @param BlendAttachment The Array of rules to use for blending the output of this pipeline to each framebuffer attachment
     * @param DescriptorCount The number of elements in the pDescriptors array.
     * @param pDescriptors An Array of DescriptorLayouts specifying the layout of each descriptor this pipeline takes.
     * @param pProfile The pipeline profile to use for this pipeline (stuff like resolution, sample count, etc.)
     */
    Pipeline* CreatePipeline(std::string PipeName, uint32_t SubpassIdx, const char* VtxPath, const char* FragPath, uint32_t BlendAttCount = 0, VkPipelineColorBlendAttachmentState* BlendAttachments = nullptr, uint32_t DescriptorCount = 0, Resources::DescriptorLayout* pDescriptors = nullptr, PipelineProfile* pProfile = nullptr);

    void AddMesh(pbrMesh* Mesh, std::string PipeName);
    Drawable* CreateDrawable(pbrMesh* pMesh, bool bDynamic);
    
    PointLight CreatePointLight(glm::vec3 Pos, glm::vec3 LightCol);

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
    
    void Update();

    void Render();

    /* Descriptors */
        std::unordered_map<VkDescriptorSetLayout, Allocators::DescriptorPool> DescriptorHeaps; //! > map assigns each type of descriptorsetlayout in the scene a corresponding descriptor pool

    /* Command Heaps/Allocators */
        Allocators::CommandPool GraphicsHeap; //! > Graphics command buffer allocator
        Allocators::CommandPool ComputeHeap; //! > Compute command buffer allocator
        Allocators::CommandPool TransferHeap; //! > Transfer command buffer allocator

    /* Scene Render Information */
        PipelineProfile SceneProfile; //! > Scene-wide pipeline profile.
        Camera* SceneCam; //! > Scene camera structure.

private:
    /* Scene Objects */
        std::vector<PointLight> SceneLights;
    
    /* Scene Pass */
        struct
        {
            VkSemaphore DrawGenSem;
            Resources::Fence* pFrameFence;
        } SceneSync; //! > Contains the semaphore and fence used to synchronise draw call generation.

        std::vector<VkSemaphore> RenderSemaphores; //! > Contains all the semaphores needed for rendering. The size of the vector is equal to the number of framebuffers

        RenderPass ScenePass; //! > Scene wide renderpass. Passes can be added to the scene through AddPass().

    /* FrameBuffers */
        FrameBufferChain FrameChain; //! > Scene framebuffer chain. Attachments can be added through AddFrameBufferAttachment()

    /* Scene Descriptor layout can be found in Draw.comp */
        Resources::DescriptorLayout* pSceneDescriptorLayout;
        Resources::DescriptorSet* pSceneDescriptorSet;

        /* SSBO for static scene objects */
            Resources::Buffer StaticSceneBuffer; //! > Static scene object positions (10000).
            uint32_t StaticIter = 0; //! > Index Iterator

        /* SSBO for dynamic scene objects */
            Resources::Buffer DynamicSceneBuffer; //! > Dynamic scene object positions (5000).
            uint32_t DynamicIter = 0; //! > Index Iterator
    
        /* SSBO for scene lights */
            Resources::Buffer SceneLightBuffer; //! > Contains all the lights in the current scene.
            uint32_t LightIter = 0; //! > Index Iterator

    /* Drawing/Rendering */
        std::unordered_map<std::string, PipeStage*> PipeStages; //! > Pipeline stages (Pipeline+Drawables) mapped to string names (the names of the pipeline.)

        /***************** TODO ******************/
        //  Change pass stages "PipeStages" variable to an array of names that can be used to index into renderer's "PipeStages" map.
        /*****************************************/
        std::vector<PassStage> PassStages; //! > Pipeline stages sorted by subpass.

        ComputePipeline DrawPipeline; //! > The pipeline used by the renderer to perform culling and generate indirect draw commands

    /* Command buffers */
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
        pbrMesh** CreateMesh(std::string Path, std::string PipeName, uint32_t& MeshCount);
    
        /*! \brief Loads an image from a file.
        *
        *   Uses OpenImageIO to open and read a file from the filesystem into GPU memory.
        *
        *   @param ImageFile The path to the image file to load.
        *   @return The image as an Resources::Image object.
        */
        Resources::Image* LoadImage(std::string ImageFile, VkSampleCountFlagBits SampleCount = VK_SAMPLE_COUNT_1_BIT);

        SceneRenderer* pRenderer;
};
