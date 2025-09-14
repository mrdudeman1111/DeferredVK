#pragma once

#include "Framework.hpp"
#include "Texture.hpp"

#include "glm/gtc/matrix_transform.hpp"

// if this is changed, change the define with the same name in Shaders/Draw.comp
#define MAX_RENDERABLE_INSTANCES 600

// seperate drawables and meshes.

class pbrMaterial
{
public:
    static Resources::DescriptorLayout MatLayout;
    
    pbrMaterial();

    Texture::Texture2D Albedo;
    Texture::Texture2D Metallic;
    Texture::Texture2D Roughness;
    Texture::Texture2D Normal;
};

/*! Vertex structure containing all needed attributes */
struct Vertex
{
public:
    Vertex() {};

    alignas(16) glm::vec3 Position; //! > Position of the vertex in 3D-space
    alignas(16) glm::vec3 Normal; //! > Facing normal of the vertex.
    alignas(8) glm::vec2 UV; //! > UV coordinates of the vertex, for mapping to textures.
};

/*
struct pbrMeshDescriptors
{
    VkDrawIndirectCommand cmdIndirectDraw;
    uint32_t MeshBoundIdx;
    uint32_t InstCount;
    std::vector<uint32_t> MeshInstances;
};
*/

// todo: implement convex hulls as culling shapes.
struct CullingBox
{
    glm::vec3 Vertices[4];
};

class Cullable
{
    CullingBox CullBound;
};

class Instanced
{
public:
    Instanced();
    ~Instanced();

    static Resources::DescriptorLayout* pMeshPassLayout; // This should never be changed, this is the layout for a buffer that handles proper indexing and instancing with the renderer. (i.e. It should not change stride, size, or offsets).

    virtual void GenDraws(VkCommandBuffer* pCmdBuffer, VkPipelineLayout Layout) = 0;
    virtual void DrawInstances(VkCommandBuffer* pCmdBuffer, VkPipelineLayout Layout) = 0;
    virtual void AddInstance(uint32_t InstanceIndex) = 0;
    void Update();

    Resources::DescriptorSet* pMeshPassSet; //! > The Mesh pass Descriptor. This is provided by the renderer through the AddMesh() method. P.S. the descriptor layout used to create this descriptor set is the static pMeshPassLayout variable. Also contains a list of all instances, this is used to map raw indices to scene indices (instance 0,1,2 is mapped to an object index in the scene like 24,58,91)

protected:
    bool bInstanceDataDirty; //! > Dirty flag. Raised when a new instance is attached to the mesh.

    std::vector<uint32_t> Instances; //! > List of managed instances. They are represented here as indices in the scene buffer.
    Resources::Buffer MeshPassBuffer; //! > The Mesh pass buffer. This buffer contains (in this order) a VkDrawIndexedIndirectCommand, two uint32(s) a mesh bound index for frustum culling and an instance count indicating the number of instances managed by the mesh, and finally an array of MAX_RENDERABLE_INSTANCES uint32(s) representing the instances (as indices in the Scene buffer) to render. And finally, a list of all instance indices, this is used to map render indices to scene indices (instance 0,1,2 is mapped to an object index in the scene like 24,58,91) In debug builds, this will exist on host visible memory.
};

class Mesh : public Instanced
{
public:
    CullingBox CullBound;
    
    Mesh() : Instanced(), MeshBuffer("Mesh Buffer") {};
    ~Mesh()
    {
#ifdef DEBUG_MODE
        std::cout << "Destroying pbrMesh\n";
#endif
        
        delete[] pVertices;
        pVertices = nullptr;
        VertCount = 0;
        delete[] pIndices;
        pIndices = nullptr;
        IndexCount = 0;
    }
    
    uint32_t VertCount; //! The number of elements in (and the size of) the pVertices array.
    Vertex* pVertices; //! A dynamic array (exists on the heap, as in the CPU side, not the GPU side) Containing the Vertices. Any changes to this array should be followed at some point by a call to Mesh::Update();
    
    uint32_t IndexCount;
    uint32_t* pIndices;

    uint32_t IndexOffset; //! The offset in bytes of the Indices in the mesh buffer, used during draw calls.

    Resources::Buffer MeshBuffer; //! Contains all the mesh's vertices and indices on the GPU for use during rendering.

    std::string Name;
};

/*! Contains physical description of a mesh */
class pbrMesh : public Mesh
{
public:
    CullingBox CullBound;

    pbrMesh();
    ~pbrMesh()
    {
        #ifdef DEBUG_MODE
            std::cout << "Destroying a pbrMesh named \"" << Name << "\"\n";
        #endif
    }

    void Bake();

    /* Inherited from instance */
        /*! \brief Generates the draw commands (and performs occlusion and frustum culling) for the instances*/
        void GenDraws(VkCommandBuffer* pCmdBuffer, VkPipelineLayout Layout);

        /*! \brief Renders all visible instances of this mesh. */
        void DrawInstances(VkCommandBuffer* pCmdBuff, VkPipelineLayout Layout);

        void AddInstance(uint32_t InstanceIndex);

private:

    pbrMaterial* pMat;
};

/*! Contains information needed for a mesh instance to be rendered. */
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

