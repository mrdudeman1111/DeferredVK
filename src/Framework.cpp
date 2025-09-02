#include "Framework.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

Window* gWindow; // The Framework's global window state.

Context* gContext; // The framework's global context.

TransferAgent* gTransferAgent;

/*! \brief A heap of application memory.*/
struct MemoryHeap
{
    MemoryHeap() : AllocTail(0), Memory(VK_NULL_HANDLE), pMapped(nullptr) {}
    ~MemoryHeap() {}

    void Destroy() { vkFreeMemory(gContext->Device, Memory, nullptr); }

    size_t Size; // the size of the memory heap
    size_t Available; // the amount of available memory in bytes

    uint32_t AllocTail; // The allocation "tail" (First byte of unallocated memory)

    VkDeviceMemory Memory; // the memory heap's memory handle
    void* pMapped; // pointer to mapped host visible memory
};

/*! \brief All the application's memory */
struct Memory
{
public:
    Memory()
    {
        HostHeaps = {};
        LocalHeaps = {};
    }

    ~Memory()
    {
        for(auto Mem : HostHeaps)
        {
            Mem.Destroy();
        }
        
        HostHeaps.clear();

        for(auto Mem : LocalHeaps)
        {
            Mem.Destroy();
        }
        
        LocalHeaps.clear();
    }
    
    std::vector<MemoryHeap> HostHeaps; // host visible memory heaps
    std::vector<MemoryHeap> LocalHeaps; // device local memory heaps
}* gApplicationMemory;

bool InitWrapperFW(uint32_t Width, uint32_t Height)
{
    VkResult Err;

    // initiate globals
    gContext = new Context();
    gWindow = new Window();
    gApplicationMemory = new Memory();

    // initiate SDL for window creation and input management.
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Vulkan_LoadLibrary(nullptr);
    gWindow->sdlWindow = SDL_CreateWindow("Framework Renderer", Width, Height, SDL_WINDOW_VULKAN);
    gWindow->Resolution = {Width, Height};

    // attach the cursor to the windo
    SDL_SetWindowRelativeMouseMode(gWindow->sdlWindow, true);

    // retrieve required extensions for SDL
    std::vector<const char*> InstExts;
    
    uint32_t ExtCount = 0;
    const char* const* SdlExt = SDL_Vulkan_GetInstanceExtensions(&ExtCount);
    
    for(uint32_t i = 0; i < ExtCount; i++)
    {
        InstExts.push_back(SdlExt[i]);
    }
    
    #ifndef RENDERDOC
        InstExts.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    #endif

    #ifdef __APPLE__
        InstExts.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    #endif

    // validation layers to enable for the vulkan instance
    std::vector<const char*> Layers = { "VK_LAYER_KHRONOS_validation"/*, "VK_LAYER_LUNARG_crash_diagnostic"*/};  // TODO use a runtime flag to set a define which will set the vulkan layers and extensions to run.

    // instance creation info
    VkInstanceCreateInfo InstCI{};
    InstCI.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    #ifdef __APPLE__
        InstCI.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    #endif
    InstCI.enabledLayerCount = (uint32_t)Layers.size();
    InstCI.ppEnabledLayerNames = Layers.data();
    InstCI.enabledExtensionCount = (uint32_t)InstExts.size();
    InstCI.ppEnabledExtensionNames = InstExts.data();

    // create instance
    if((Err = vkCreateInstance(&InstCI, nullptr, &gContext->Instance)) != VK_SUCCESS)
    {
        printf("Error : %i\n", Err);
        throw std::runtime_error("Failed to create instance\n");
    }

    // retrieve available physical devices
    uint32_t DeviceCount = 0;
    vkEnumeratePhysicalDevices(gContext->Instance, &DeviceCount, nullptr);
    std::vector<VkPhysicalDevice> PhysDevices(DeviceCount);
    vkEnumeratePhysicalDevices(gContext->Instance, &DeviceCount, PhysDevices.data());
    
    gContext->PhysDevice = VK_NULL_HANDLE;

    // find and use the first discrete GPU available
    for(uint32_t i = 0; i < DeviceCount; i++)
    {
        VkPhysicalDeviceProperties DevProps;
        vkGetPhysicalDeviceProperties(PhysDevices[i], &DevProps);

        if(DevProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            gContext->PhysDevice = PhysDevices[i];
            break;
        }
    }

    if(gContext->PhysDevice == VK_NULL_HANDLE) { gContext->PhysDevice = PhysDevices[0]; }

    #ifdef RENDERDOC
        vkGetPhysicalDeviceFeatures(gContext->PhysDevice, &gContext->PhysDeviceFeatures);
    #else
        gContext->PhysDeviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

        vkGetPhysicalDeviceFeatures2(gContext->PhysDevice, &gContext->PhysDeviceFeatures);
    #endif

    /* retrieve and store local and host memory in globals */
    VkPhysicalDeviceMemoryProperties MemProps;
    vkGetPhysicalDeviceMemoryProperties(gContext->PhysDevice, &MemProps);

    gContext->Local = UINT32_MAX;
    gContext->Host = UINT32_MAX;

    for(uint32_t i = 0; i < MemProps.memoryTypeCount; i++)
    {
        if(MemProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT && gContext->Local == UINT32_MAX)
        {
            gContext->Local = i;
        }
        else if(MemProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT && gContext->Host == UINT32_MAX)
        {
            gContext->Host = i;
        }
    }

    /* retrieve and store queue family information */
    uint32_t FamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(gContext->PhysDevice, &FamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> Families(FamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(gContext->PhysDevice, &FamilyCount, Families.data());

    uint32_t GraphicsFamily = UINT32_MAX;
    uint32_t ComputeFamily = UINT32_MAX;
    uint32_t TransferFamily = UINT32_MAX;
    bool bTransferFamilyFound = false;

    for(uint32_t i = 0; i < FamilyCount; i++)
    {
        if(Families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT && GraphicsFamily == UINT32_MAX)
        {
            Families[i].queueCount--;
            GraphicsFamily = i;
        }
        else if(Families[i].queueFlags & VK_QUEUE_COMPUTE_BIT && ComputeFamily == UINT32_MAX)
        {
            Families[i].queueCount--;
            ComputeFamily = i;
        }
        else if(Families[i].queueFlags & VK_QUEUE_TRANSFER_BIT && !bTransferFamilyFound)
        {
            Families[i].queueCount--;
            TransferFamily = i;
            bTransferFamilyFound = true;
        }
    }
    
    if(GraphicsFamily == UINT32_MAX || ComputeFamily == UINT32_MAX || TransferFamily == UINT32_MAX)
    {
        for(uint32_t i = 0; i < FamilyCount; i++)
        {
            if(Families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT && Families[i].queueCount > 0 && GraphicsFamily == UINT32_MAX)
            {
                Families[i].queueCount--;
                GraphicsFamily = i;
            }
            if(Families[i].queueFlags & VK_QUEUE_COMPUTE_BIT && Families[i].queueCount > 0 && ComputeFamily == UINT32_MAX)
            {
                Families[i].queueCount--;                
                ComputeFamily = i;
            }
            if(Families[i].queueFlags & VK_QUEUE_TRANSFER_BIT && Families[i].queueCount > 0 && TransferFamily == UINT32_MAX)
            {
                Families[i].queueCount--;                
                TransferFamily = i;
            }
        }
    }

    assert(GraphicsFamily != UINT32_MAX);
    assert(ComputeFamily != UINT32_MAX);

    // create render surface from window using SDL
    SDL_Vulkan_CreateSurface(gWindow->sdlWindow, gContext->Instance, nullptr, &gWindow->Surface);

    /* Device creation*/

        // Graphics/Compute Queue creation info
        VkDeviceQueueCreateInfo QueueCI[3] = {};
        const float Prio[] = {1.f, 1.f};

        QueueCI[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        QueueCI[0].queueCount = 1;
        QueueCI[0].queueFamilyIndex = GraphicsFamily;
        QueueCI[0].pQueuePriorities = Prio;

        QueueCI[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        QueueCI[1].queueCount = bTransferFamilyFound ? 1 : 2;
        QueueCI[1].queueFamilyIndex = ComputeFamily;
        QueueCI[1].pQueuePriorities = Prio;

        if(bTransferFamilyFound)
        {
            QueueCI[2].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            QueueCI[2].queueCount = 1;
            QueueCI[2].queueFamilyIndex = TransferFamily;
            QueueCI[2].pQueuePriorities = Prio;
        }
        else
        {
            std::cout << "transfer family not available, using Compute family instead.\n";
        }

        // Device extensions to enable
        std::vector<const char*> DevExt = { "VK_KHR_swapchain" };
    
        #if __APPLE__
            DevExt.push_back("VK_KHR_portability_subset");
        #endif

        // Device Creation info
        VkDeviceCreateInfo DevCI{};
        DevCI.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        DevCI.enabledExtensionCount = (uint32_t)DevExt.size();
        DevCI.ppEnabledExtensionNames = DevExt.data();
        DevCI.queueCreateInfoCount = bTransferFamilyFound ? 3 : 2;
        DevCI.pQueueCreateInfos = QueueCI;

        // Create device
        if((Err = vkCreateDevice(gContext->PhysDevice, &DevCI, nullptr, &gContext->Device)) != VK_SUCCESS)
        {
            printf("Error : %i\n", Err);
            throw std::runtime_error("Failed to create vulkan device for global context");
        }

        // retrieve device queues created during device creation
        vkGetDeviceQueue(gContext->Device, GraphicsFamily, 0, &gContext->GraphicsQueue);
        vkGetDeviceQueue(gContext->Device, ComputeFamily, 0, &gContext->ComputeQueue);

        if(bTransferFamilyFound)
        {
            vkGetDeviceQueue(gContext->Device, TransferFamily, 0, &gContext->TransferQueue);
        }
        else
        {
            vkGetDeviceQueue(gContext->Device, ComputeFamily, 1, &gContext->TransferQueue);
        }

        gContext->GraphicsFamily = GraphicsFamily;
        gContext->ComputeFamily = ComputeFamily;
        gContext->TransferFamily = TransferFamily;

    /* Swapchain creation*/

        uint32_t FormCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(gContext->PhysDevice, gWindow->Surface, &FormCount, nullptr);
        std::vector<VkSurfaceFormatKHR> Formats(FormCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(gContext->PhysDevice, gWindow->Surface, &FormCount, Formats.data());

        for(uint32_t i = 0; i < FormCount; i++)
        {
            if(Formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                gWindow->SurfFormat = Formats[i];
                break;
            }
        }
    
        // Swapchain creation info
        VkSwapchainCreateInfoKHR SwapCI{};
        SwapCI.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        SwapCI.clipped = VK_FALSE;
        SwapCI.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        SwapCI.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;

        uint32_t PresentModesCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(gContext->PhysDevice, gWindow->Surface, &PresentModesCount, nullptr);
        std::vector<VkPresentModeKHR> PresentModes(PresentModesCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(gContext->PhysDevice, gWindow->Surface, &PresentModesCount, PresentModes.data());

        if(std::find(PresentModes.begin(), PresentModes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != PresentModes.end())
        {
            SwapCI.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        }
        else if(std::find(PresentModes.begin(), PresentModes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != PresentModes.end())
        {
            SwapCI.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
        }
        else if(std::find(PresentModes.begin(), PresentModes.end(), VK_PRESENT_MODE_FIFO_KHR) != PresentModes.end())
        {
            SwapCI.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        }
        else
        {
            SwapCI.presentMode = PresentModes[0];
        }

        SwapCI.imageArrayLayers = 1;
        SwapCI.imageFormat = gWindow->SurfFormat.format;
        SwapCI.imageColorSpace = gWindow->SurfFormat.colorSpace;
        SwapCI.imageExtent = gWindow->Resolution;
        SwapCI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        SwapCI.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        SwapCI.minImageCount = 3;

        SwapCI.surface = gWindow->Surface;


        if((Err = vkCreateSwapchainKHR(gContext->Device, &SwapCI, nullptr, &gWindow->Swapchain)) != VK_SUCCESS)
        {
            printf("Error : %i", Err);
            throw std::runtime_error("Failed to create framework swapchain");
        }

        /* Retrieve swapchain images and store them in the global window structure */
        uint32_t SwpImgCount;
        vkGetSwapchainImagesKHR(gContext->Device, gWindow->Swapchain, &SwpImgCount, nullptr);
        gWindow->SwapchainImages.resize(SwpImgCount);
        vkGetSwapchainImagesKHR(gContext->Device, gWindow->Swapchain, &SwpImgCount, gWindow->SwapchainImages.data());

        /* Iterate over retrieved swapchain images and create ImageViews for all of them. (Image views are stored in the gloal window structure as well) */
        for(uint32_t i = 0; i < gWindow->SwapchainImages.size(); i++)
        {
            VkImageView TmpView;

            VkImageViewCreateInfo ViewCI{};
            ViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            ViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
            ViewCI.format = gWindow->SurfFormat.format;
            ViewCI.image = gWindow->SwapchainImages[i];

            ViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            ViewCI.subresourceRange.levelCount = 1;
            ViewCI.subresourceRange.layerCount = 1;
            ViewCI.subresourceRange.baseArrayLayer = 0;
            ViewCI.subresourceRange.baseMipLevel = 0;

            ViewCI.components.r = VK_COMPONENT_SWIZZLE_R;
            ViewCI.components.g = VK_COMPONENT_SWIZZLE_G;
            ViewCI.components.b = VK_COMPONENT_SWIZZLE_B;
            ViewCI.components.a = VK_COMPONENT_SWIZZLE_A;

            if((Err = vkCreateImageView(gContext->Device, &ViewCI, nullptr, &TmpView)) != VK_SUCCESS)
            {
                printf("Error : %i\n", Err);
                throw std::runtime_error("Failed to create image view for swapchain images");
            }

            gWindow->SwapchainAttachments.push_back(TmpView);
        }

    Allocators::CommandPool* pTransferAgentPool = new Allocators::CommandPool();
    pTransferAgentPool->Bake(CommandType::eCmdTransfer);

    gTransferAgent = new TransferAgent(pTransferAgentPool);

    return true;
}

void CloseWrapperFW()
{
    SDL_DestroyWindowSurface(gWindow->sdlWindow);
    SDL_DestroyWindow(gWindow->sdlWindow);

    for(uint32_t i = 0; i < gWindow->SwapchainAttachments.size(); i++)
    {
        vkDestroyImageView(gContext->Device, gWindow->SwapchainAttachments[i], nullptr);
    }

    delete gTransferAgent;

    vkDestroySwapchainKHR(gContext->Device, gWindow->Swapchain, nullptr);
    vkDestroyDevice(gContext->Device, nullptr);
    vkDestroyInstance(gContext->Instance, nullptr);

    delete gApplicationMemory;
    delete gContext;
    delete gWindow;
}

    void TransferAgent::Transfer(void* srcData, size_t srcSize, Resources::Buffer* dstBuff, size_t dstOffset)
    {
        if(dstBuff->Alloc.bHostVisible)
        {
            if(dstBuff->pData == nullptr) throw std::runtime_error("tried to transfer to a host-visible buffer without calling map() on it first. (The buffer MUST be mapped prior to transfer).");
           
            uint8_t* pDst = (uint8_t*)dstBuff->pData;
            memcpy(pDst+dstOffset, srcData, srcSize);

            return;
        }

        VkBufferCopy tmp;
        tmp.srcOffset = TransitTail;
        tmp.dstOffset = dstOffset;
        tmp.size = srcSize;

        BufferCopies.push_back(tmp);

        TransferBuffers.push_back(dstBuff);

        uint8_t* pDst = (uint8_t*)pTransitBuffer->pData;
        memcpy(pDst+TransitTail, srcData, srcSize);

        TransitTail += srcSize;

        return;
    }

    void TransferAgent::Transfer(void* srcData, size_t srcSize, Resources::Image* dstImg, VkExtent3D Extent, VkOffset3D ImgOffset, VkImageSubresourceLayers SubResource, VkImageLayout Layout)
    {
        VkBufferImageCopy tmp;
        tmp.bufferOffset = TransitTail;
        tmp.imageExtent = Extent;
        tmp.imageOffset = ImgOffset;
        tmp.imageSubresource = SubResource;

        BuffImageCopies.push_back(tmp);

        TransferBuffImages.push_back(dstImg);
        BufferImageLayouts.push_back(Layout);

        memcpy(((uint8_t*)pTransitBuffer->pData)+TransitTail, srcData, srcSize);

        TransitTail += srcSize;
    }

    void TransferAgent::Transfer(Resources::Image* srcImg, Resources::Image* dstImg, VkImageLayout srcLayout, VkImageLayout dstLayout, VkOffset3D srcOffset, VkOffset3D dstOffset, VkImageSubresourceLayers srcSubResource, VkImageSubresourceLayers dstSubResource, VkExtent3D Size)
    {
        VkImageCopy tmp;
        tmp.srcOffset = srcOffset;
        tmp.dstOffset = dstOffset;
        tmp.srcSubresource = srcSubResource;
        tmp.dstSubresource = dstSubResource;
        tmp.extent = Size;

        ImageCopies.push_back(tmp);

        TransferImages.push_back(std::make_pair(srcImg, dstImg));
        ImageLayouts.push_back(std::make_pair(srcLayout, dstLayout));
    }

    void TransferAgent::Flush()
    {
        /*
        if(bFlushing)
        {
            AwaitFlush();
        }

        bFlushing = true;

        pWaitThread = new std::thread([this] { FlushImpl(); });
        */

        FlushImpl();

        return;
    }

    void TransferAgent::AwaitFlush()
    {
        if(pWaitThread->joinable())
        {
            pWaitThread->join();
        }

        delete pWaitThread;
        pWaitThread = nullptr;

        return;
    }

    void TransferAgent::FlushImpl()
    {
        if(pCmdBuff == nullptr)
        {
            pCmdBuff = cmdAllocator->CreateBuffer();
        }

        if(BufferCopies.size() != 0 || BuffImageCopies.size() != 0 || ImageCopies.size() != 0)
        {
            pCmdBuff->Start();

                for(uint32_t i = 0; i < BufferCopies.size(); i++) {
                    vkCmdCopyBuffer(*pCmdBuff, *pTransitBuffer, *TransferBuffers[i], 1, &BufferCopies[i]);
                }
                for(uint32_t i = 0; i < BuffImageCopies.size(); i++)
                {
                    vkCmdCopyBufferToImage(*pCmdBuff, *pTransitBuffer, TransferBuffImages[i]->Img, BufferImageLayouts[i], 1, &BuffImageCopies[i]);
                }
                for(uint32_t i = 0; i < ImageCopies.size(); i++)
                {
                    vkCmdCopyImage(*pCmdBuff, TransferImages[i].first->Img, ImageLayouts[i].first, TransferImages[i].second->Img, ImageLayouts[i].second, 1, &ImageCopies[i]);
                }

            pCmdBuff->Stop();

            cmdAllocator->Submit(pCmdBuff);

            pCmdBuff->cmdFence->Wait();
            pCmdBuff->Reset();

            BufferCopies.clear();
            TransferBuffers.clear();
            BuffImageCopies.clear();
            TransferBuffImages.clear();
            BufferImageLayouts.clear();
            ImageCopies.clear();
            TransferImages.clear();
            ImageLayouts.clear();
        }

        TransitTail = 0;
        bFlushing = false;
        return;
    }

Context* GetContext()
{
    return gContext;
}

Window* GetWindow()
{
    return gWindow;
}

TransferAgent* GetTransferAgent()
{
    return gTransferAgent;
}

VkSemaphore CreateVulkanSemaphore()
{
    VkSemaphore Ret;

    VkSemaphoreCreateInfo SemCI{};
    SemCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    vkCreateSemaphore(GetContext()->Device, &SemCI, nullptr, &Ret);

    return Ret;
}

Resources::Fence* CreateFence(std::string Name)
{
    Resources::Fence* pRet = new Resources::Fence(Name);

    VkFenceCreateInfo FenceCI{};
    FenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    if(vkCreateFence(GetContext()->Device, &FenceCI, nullptr, *pRet) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create fence " + Name);
    }

    return pRet;
}

VkResult AllocBuff(Resources::Buffer& Buffer, VkMemoryRequirements& MemReq, std::vector<MemoryHeap>& MemoryHeaps, uint32_t MemIdx)
{
    VkResult Ret = VK_SUCCESS;
    
    bool bBound = false;

    // bind to existing heap with enough space
        for(uint32_t i = 0; i < MemoryHeaps.size(); i++)
        {
            MemoryHeap& MemHeap = MemoryHeaps[i];
            
            size_t AlignmentOffset = MemReq.alignment - (MemHeap.AllocTail % MemReq.alignment);
            size_t MemLoc = MemHeap.AllocTail + AlignmentOffset; // align the alloc tail to the alignment requirements of our buffer.
            size_t Size = MemHeap.Size - MemLoc;

            if(Size >= MemReq.size)
            {
                Buffer.Alloc.Offset = (uint32_t)MemLoc;
                Buffer.Alloc.Size = (uint32_t)MemReq.size;
                Buffer.Alloc.pMemory = &MemHeap.Memory;

                MemHeap.AllocTail += MemReq.size + AlignmentOffset;
                MemHeap.Available -= MemReq.size + AlignmentOffset;

                Ret = vkBindBufferMemory(gContext->Device, Buffer, MemHeap.Memory, MemLoc);

                bBound = true;
                break;
            }
        }
    
    // if a large enough heap could not be found
        if(!bBound)
        {
            MemoryHeaps.push_back(MemoryHeap());
            MemoryHeap& MemHeap = MemoryHeaps[MemoryHeaps.size()-1];
            
        // bind to new custom sized heap (the required allocation is too big for a standard sized heap)
            if(MemReq.size > PAGE_SIZE)
            {
                VkMemoryAllocateInfo AllocInf{};
                AllocInf.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                AllocInf.memoryTypeIndex = MemIdx;
                AllocInf.allocationSize = MemReq.size;
                
                if(vkAllocateMemory(gContext->Device, &AllocInf, nullptr, &MemHeap.Memory) != VK_SUCCESS)
                {
                    throw std::runtime_error("Failed to allocate memory, ran out of space");
                }
                
                MemHeap.Size = MemReq.size;
                MemHeap.AllocTail = 0;
                MemHeap.Available = 0;

                Buffer.Alloc.Offset = 0;
                Buffer.Alloc.Size = (uint32_t)MemReq.size;
                Buffer.Alloc.pMemory = &MemHeap.Memory;

                Ret = vkBindBufferMemory(gContext->Device, Buffer, MemHeap.Memory, 0);
                
                MemHeap.AllocTail += MemReq.size + (MemHeap.AllocTail % MemReq.alignment);
            }
            
        // bind to new standard sized heap.
            else
            {
                VkMemoryAllocateInfo AllocInf{};
                AllocInf.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                AllocInf.memoryTypeIndex = MemIdx;
                AllocInf.allocationSize = PAGE_SIZE;
                
                if(vkAllocateMemory(gContext->Device, &AllocInf, nullptr, &MemHeap.Memory))
                {
                    throw std::runtime_error("Failed to allocate a heap for an allocation");
                }
                
                MemHeap.Size = PAGE_SIZE;
                MemHeap.AllocTail = 0;
                MemHeap.Available = AllocInf.allocationSize - MemReq.size;
                
                Buffer.Alloc.Offset = 0;
                Buffer.Alloc.Size = (uint32_t)MemReq.size;
                Buffer.Alloc.pMemory = &MemHeap.Memory;
                
                Ret = vkBindBufferMemory(gContext->Device, Buffer, MemHeap.Memory, 0);
                
                MemHeap.AllocTail += MemReq.size + (MemHeap.AllocTail % MemReq.alignment);
                MemHeap.Available = MemHeap.Size - MemHeap.AllocTail;
            }
        }

    return Ret;
}

void Allocate(Resources::Buffer& Buffer, bool bVisible)
{
    VkResult Err;
    VkMemoryRequirements MemReq;

    vkGetBufferMemoryRequirements(gContext->Device, Buffer, &MemReq);

    Buffer.pData = nullptr;
    Buffer.Alloc.bHostVisible = bVisible;

    if(bVisible)
        Err = AllocBuff(Buffer, MemReq, gApplicationMemory->HostHeaps, gContext->Host);
    else
        Err = AllocBuff(Buffer, MemReq, gApplicationMemory->LocalHeaps, gContext->Local);

    if(Err != VK_SUCCESS)
        throw std::runtime_error("Failed to bind a buffer.");

    return;
}

void Allocate(Resources::Image& Image, bool bVisible)
{
    VkMemoryRequirements MemReq;

    Image.Alloc.bHostVisible = bVisible;

    vkGetImageMemoryRequirements(gContext->Device, Image.Img, &MemReq);

    if(bVisible)
    {
        for(uint32_t i = 0; i < gApplicationMemory->HostHeaps.size(); i++)
        {
            size_t MemLoc = gApplicationMemory->HostHeaps[i].AllocTail + (gApplicationMemory->HostHeaps[i].AllocTail % MemReq.alignment); // align the alloc tail to the alignment requirements of our buffer.
            size_t Size = gApplicationMemory->HostHeaps[i].Size - MemLoc;

            if(Size >= MemReq.size)
            {
                Image.Alloc.Offset = (uint32_t)MemLoc;
                Image.Alloc.Size = (uint32_t)MemReq.size;
                Image.Alloc.pMemory = &gApplicationMemory->HostHeaps[i].Memory;

                vkBindImageMemory(gContext->Device, Image.Img, gApplicationMemory->HostHeaps[i].Memory, MemLoc);
                
                gApplicationMemory->HostHeaps[i].AllocTail += MemReq.size + (gApplicationMemory->HostHeaps[i].AllocTail % MemReq.alignment);
            }
        }
    }
    else
    {
        for(uint32_t i = 0; i < gApplicationMemory->LocalHeaps.size(); i++)
        {
            size_t MemLoc = gApplicationMemory->LocalHeaps[i].AllocTail + (gApplicationMemory->LocalHeaps[i].AllocTail % MemReq.alignment); // align the alloc tail to the alignment requirements of our buffer.
            size_t Size = gApplicationMemory->LocalHeaps[i].Size - MemLoc;

            if(Size >= MemReq.size)
            {
                Image.Alloc.Offset = (uint32_t)MemLoc;
                Image.Alloc.Size = (uint32_t)MemReq.size;
                Image.Alloc.pMemory = &gApplicationMemory->LocalHeaps[i].Memory;

                vkBindImageMemory(gContext->Device, Image.Img, gApplicationMemory->LocalHeaps[i].Memory, MemLoc);

                gApplicationMemory->LocalHeaps[i].AllocTail += MemReq.size + (gApplicationMemory->LocalHeaps[i].AllocTail % MemReq.alignment);
            }
        }
    }

    return;
}

VkResult CreateBuffer(Resources::Buffer& Buffer, size_t Size, VkBufferUsageFlags Usage)
{
    VkResult Ret;

    VkBufferCreateInfo BuffCI{};
    BuffCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    BuffCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    BuffCI.size = Size;
    BuffCI.usage = Usage;

    Ret = vkCreateBuffer(gContext->Device, &BuffCI, nullptr, Buffer);

    return Ret;
}

VkResult CreateImage(Resources::Image& Image, VkFormat Format, VkExtent2D Size, VkImageUsageFlags Usage, VkSampleCountFlagBits SampleCount)
{
    VkResult Ret;

    VkImageCreateInfo ImageCI{};
    ImageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ImageCI.usage = Usage;
    ImageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ImageCI.extent = {Size.width, Size.height, 1};
    ImageCI.format = Format;
    ImageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    ImageCI.arrayLayers = 1;
    ImageCI.imageType = VK_IMAGE_TYPE_2D;
    ImageCI.mipLevels = 0;
    ImageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ImageCI.samples = SampleCount;

    Ret = vkCreateImage(gContext->Device, &ImageCI, nullptr, &Image.Img);

    return Ret;
}

VkResult CreateView(VkImageView& View, VkImage& Image, VkFormat Format, VkImageAspectFlagBits Aspect)
{
    VkResult Ret;

    VkImageViewCreateInfo ViewCI{};
    ViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ViewCI.format = Format;
    ViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ViewCI.image = Image;
    ViewCI.components.r = VK_COMPONENT_SWIZZLE_R;
    ViewCI.components.g = VK_COMPONENT_SWIZZLE_G;
    ViewCI.components.b = VK_COMPONENT_SWIZZLE_B;
    ViewCI.components.a = VK_COMPONENT_SWIZZLE_A;
    ViewCI.subresourceRange.aspectMask = Aspect;
    ViewCI.subresourceRange.baseArrayLayer = 0;
    ViewCI.subresourceRange.baseMipLevel = 0;
    ViewCI.subresourceRange.layerCount = 1;
    ViewCI.subresourceRange.levelCount = 1;

    Ret = vkCreateImageView(gContext->Device, &ViewCI, nullptr, &View);

    return Ret;
}

void Map(Resources::Buffer* pBuffer)
{
    if(!pBuffer->Alloc.bHostVisible)
    {
        throw std::runtime_error("Failed to map buffer : The buffer is not host visible.\n");
    }
    if(pBuffer->pData != nullptr)
    {
        std::cout << "Just tried to map an already mapped buffer.";
        return;
    }

    for(uint32_t i = 0; i < gApplicationMemory->HostHeaps.size(); i++)
    {
        if(&(gApplicationMemory->HostHeaps[i].Memory) == pBuffer->Alloc.pMemory) // P.S. We're comparing the ADDRESS of the handle rather than the actual value of the handle, since you can't compare vulkan handles (accurately)
        {
            if(gApplicationMemory->HostHeaps[i].pMapped != nullptr)
            {
                pBuffer->pData = ((uint8_t*)gApplicationMemory->HostHeaps[i].pMapped)+pBuffer->Alloc.Offset;
                break;
            }
            else
            {
                vkMapMemory(gContext->Device, gApplicationMemory->HostHeaps[i].Memory, 0, gApplicationMemory->HostHeaps[i].Size, 0, &gApplicationMemory->HostHeaps[i].pMapped);
                pBuffer->pData = ((uint8_t*)gApplicationMemory->HostHeaps[i].pMapped)+pBuffer->Alloc.Offset;
                break;
            }
        }
    }
    
    if(pBuffer->pData == nullptr)
    {
        throw std::runtime_error("failed to map buffer for some reason?");
    }

    return;
}

void Unmap(Resources::Buffer* pBuffer)
{
    vkUnmapMemory(gContext->Device, *pBuffer->Alloc.pMemory);
}
