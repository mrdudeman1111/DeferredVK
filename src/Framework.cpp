#include "Framework.hpp"

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

Window* gWindow; // The Framework's global context.

Context* gContext;

TransferAgent* gTransferAgent;

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

private:

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

public:
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

    // attach the cursor to the window
    SDL_SetWindowRelativeMouseMode(gWindow->sdlWindow, true);

    // retrieve required extensions for SDL
    uint32_t ExtCount = 0;
    const char* const* SdlExt = SDL_Vulkan_GetInstanceExtensions(&ExtCount);

    // validation layers to enable for the vulkan instance
    std::vector<const char*> Layers = { "VK_LAYER_KHRONOS_validation"/*, "VK_LAYER_LUNARG_crash_diagnostic"*/};  // TODO use a runtime flag to set a define which will set the vulkan layers and extensions to run.

    // instance creation info
    VkInstanceCreateInfo InstCI{};
    InstCI.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    InstCI.enabledLayerCount = Layers.size();
    InstCI.ppEnabledLayerNames = Layers.data();
    InstCI.enabledExtensionCount = ExtCount;
    InstCI.ppEnabledExtensionNames = SdlExt;

    // create instance
    if((Err = vkCreateInstance(&InstCI, nullptr, &gContext->Instance)) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create instance");
    }

    // retrieve available physical devices
    uint32_t DeviceCount = 0;
    vkEnumeratePhysicalDevices(gContext->Instance, &DeviceCount, nullptr);
    std::vector<VkPhysicalDevice> PhysDevices(DeviceCount);
    vkEnumeratePhysicalDevices(gContext->Instance, &DeviceCount, PhysDevices.data());

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

    vkGetPhysicalDeviceFeatures(gContext->PhysDevice, &gContext->PhysDeviceFeatures);

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

        // Device Creation info
        VkDeviceCreateInfo DevCI{};
        DevCI.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        DevCI.enabledExtensionCount = DevExt.size();
        DevCI.ppEnabledExtensionNames = DevExt.data();
        DevCI.queueCreateInfoCount = bTransferFamilyFound ? 3 : 2;
        DevCI.pQueueCreateInfos = QueueCI;

        // Create device
        if((Err = vkCreateDevice(gContext->PhysDevice, &DevCI, nullptr, &gContext->Device)) != VK_SUCCESS)
        {
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
        SwapCI.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;

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
            
            memcpy(((uint8_t*)dstBuff->pData)+dstOffset, srcData, srcSize);

            return;
        }

        VkBufferCopy tmp;
        tmp.srcOffset = TransitTail;
        tmp.dstOffset = dstOffset;
        tmp.size = srcSize;

        BufferCopies.push_back(tmp);

        TransferBuffers.push_back(dstBuff);

        uint8_t* pDst = (uint8_t*)pTransitBuffer->pData;
        memcpy(&pDst[TransitTail], srcData, srcSize);

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

void Allocate(Resources::Buffer& Buffer, bool bVisible)
{
    VkResult Err;
    
    Buffer.Alloc.bHostVisible = bVisible;

    VkMemoryRequirements MemReq;

    vkGetBufferMemoryRequirements(gContext->Device, Buffer, &MemReq);

    Buffer.pData = nullptr;

    bool bBound = false;

    if(bVisible)
    {
        for(uint32_t i = 0; i < gApplicationMemory->HostHeaps.size(); i++)
        {
            size_t MemLoc = gApplicationMemory->HostHeaps[i].AllocTail + (gApplicationMemory->HostHeaps[i].AllocTail % MemReq.alignment); // align the alloc tail to the alignment requirements of our buffer.
            size_t Size = gApplicationMemory->HostHeaps[i].Size - MemLoc;

            if(Size >= MemReq.size)
            {
                Buffer.Alloc.Offset = MemLoc;
                Buffer.Alloc.Size = MemReq.size;
                Buffer.Alloc.pMemory = &gApplicationMemory->HostHeaps[i].Memory;

                Err = vkBindBufferMemory(gContext->Device, Buffer, gApplicationMemory->HostHeaps[i].Memory, MemLoc);
                bBound = true;
                
                gApplicationMemory->HostHeaps[i].AllocTail += MemReq.size + (gApplicationMemory->HostHeaps[i].AllocTail % MemReq.alignment);
            }
        }
        if(!bBound)
        {
            gApplicationMemory->HostHeaps.push_back({});
            uint32_t EndIdx = gApplicationMemory->HostHeaps.size()-1;

            if(MemReq.size > PAGE_SIZE)
            {
                VkMemoryAllocateInfo AllocInf{};
                AllocInf.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                AllocInf.memoryTypeIndex = gContext->Host;
                AllocInf.allocationSize = MemReq.size;

                if(vkAllocateMemory(gContext->Device, &AllocInf, nullptr, &gApplicationMemory->HostHeaps[EndIdx].Memory) != VK_SUCCESS)
                {
                    throw std::runtime_error("Failed to allocate memory, ran out of space");
                }

                gApplicationMemory->HostHeaps[EndIdx].Size = MemReq.size;
                gApplicationMemory->HostHeaps[EndIdx].AllocTail = 0;
                gApplicationMemory->HostHeaps[EndIdx].Available = 0;
                
                Buffer.Alloc.Offset = 0;
                Buffer.Alloc.Size = MemReq.size;
                Buffer.Alloc.pMemory = &gApplicationMemory->HostHeaps[EndIdx].Memory;

                Err = vkBindBufferMemory(gContext->Device, Buffer, gApplicationMemory->HostHeaps[EndIdx].Memory, 0);

                gApplicationMemory->HostHeaps[EndIdx].AllocTail += MemReq.size + (gApplicationMemory->HostHeaps[EndIdx].AllocTail % MemReq.alignment);
            }
            else
            {
                VkMemoryAllocateInfo AllocInf{};
                AllocInf.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                AllocInf.memoryTypeIndex = gContext->Host;
                AllocInf.allocationSize = PAGE_SIZE;

                if(vkAllocateMemory(gContext->Device, &AllocInf, nullptr, &gApplicationMemory->HostHeaps[EndIdx].Memory))
                {
                    throw std::runtime_error("Failed to allocate a host heap for an allocation");
                }

                gApplicationMemory->HostHeaps[EndIdx].Size = PAGE_SIZE;
                gApplicationMemory->HostHeaps[EndIdx].AllocTail = 0;
                gApplicationMemory->HostHeaps[EndIdx].Available = MemReq.size - AllocInf.allocationSize;
                
                Buffer.Alloc.Offset = 0;
                Buffer.Alloc.Size = MemReq.size;
                Buffer.Alloc.pMemory = &gApplicationMemory->HostHeaps[EndIdx].Memory;

                Err = vkBindBufferMemory(gContext->Device, Buffer, gApplicationMemory->HostHeaps[EndIdx].Memory, 0);

                gApplicationMemory->HostHeaps[EndIdx].AllocTail += MemReq.size + (gApplicationMemory->HostHeaps[EndIdx].AllocTail % MemReq.alignment);
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
                Buffer.Alloc.Offset = MemLoc;
                Buffer.Alloc.Size = MemReq.size;
                Buffer.Alloc.pMemory = &gApplicationMemory->LocalHeaps[i].Memory;

                Err = vkBindBufferMemory(gContext->Device, Buffer, gApplicationMemory->LocalHeaps[i].Memory, MemLoc);
                bBound = true;

                gApplicationMemory->LocalHeaps[i].AllocTail += MemReq.size + (gApplicationMemory->LocalHeaps[i].AllocTail % MemReq.alignment);
            }
        }
        if(!bBound)
        {
            gApplicationMemory->LocalHeaps.push_back({});
            uint32_t EndIdx = gApplicationMemory->LocalHeaps.size()-1;

            if(MemReq.size > PAGE_SIZE)
            {
                VkMemoryAllocateInfo AllocInf{};
                AllocInf.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                AllocInf.memoryTypeIndex = gContext->Local;
                AllocInf.allocationSize = MemReq.size;

                if(vkAllocateMemory(gContext->Device, &AllocInf, nullptr, &gApplicationMemory->LocalHeaps[EndIdx].Memory) != VK_SUCCESS)
                {
                    throw std::runtime_error("Failed to allocate memory, ran out of space");
                }

                gApplicationMemory->LocalHeaps[EndIdx].Size = MemReq.size;
                gApplicationMemory->LocalHeaps[EndIdx].AllocTail = 0;
                gApplicationMemory->LocalHeaps[EndIdx].Available = 0;
                
                Buffer.Alloc.Offset = 0;
                Buffer.Alloc.Size = MemReq.size;
                Buffer.Alloc.pMemory = &gApplicationMemory->LocalHeaps[EndIdx].Memory;

                Err = vkBindBufferMemory(gContext->Device, Buffer, gApplicationMemory->LocalHeaps[EndIdx].Memory, 0);
                
                gApplicationMemory->LocalHeaps[EndIdx].AllocTail += MemReq.size + (gApplicationMemory->LocalHeaps[EndIdx].AllocTail % MemReq.alignment);
            }
            else
            {
                VkMemoryAllocateInfo AllocInf{};
                AllocInf.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                AllocInf.memoryTypeIndex = gContext->Local;
                AllocInf.allocationSize = PAGE_SIZE;

                if(vkAllocateMemory(gContext->Device, &AllocInf, nullptr, &gApplicationMemory->LocalHeaps[EndIdx].Memory))
                {
                    throw std::runtime_error("Failed to allocate a host heap for an allocation");
                }

                gApplicationMemory->LocalHeaps[EndIdx].Size = PAGE_SIZE;
                gApplicationMemory->LocalHeaps[EndIdx].AllocTail = 0;
                gApplicationMemory->LocalHeaps[EndIdx].Available = AllocInf.allocationSize - MemReq.size;
                
                Buffer.Alloc.Offset = 0;
                Buffer.Alloc.Size = MemReq.size;
                Buffer.Alloc.pMemory = &gApplicationMemory->LocalHeaps[EndIdx].Memory;

                Err = vkBindBufferMemory(gContext->Device, Buffer, gApplicationMemory->LocalHeaps[EndIdx].Memory, 0);
                
                gApplicationMemory->LocalHeaps[EndIdx].AllocTail += MemReq.size + (gApplicationMemory->LocalHeaps[EndIdx].AllocTail % MemReq.alignment);
            }
        }
    }

    if(Err != VK_SUCCESS) throw std::runtime_error("Failed to bind a buffer.");

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
                Image.Alloc.Offset = MemLoc;
                Image.Alloc.Size = MemReq.size;
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
                Image.Alloc.Offset = MemLoc;
                Image.Alloc.Size = MemReq.size;
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
    if(pBuffer->pData != nullptr)
    {
        std::cout << "Just tried to map an already mapped buffer.";
    }

    for(uint32_t i = 0; i < gApplicationMemory->HostHeaps.size(); i++)
    {
        if(&(gApplicationMemory->HostHeaps[i].Memory) == pBuffer->Alloc.pMemory) // P.S. We're comparing the ADDRESS of the handle rather than the actual value of the handle, since you can't compare vulkan handles (accurately)
        {
            if(gApplicationMemory->HostHeaps[i].pMapped != nullptr)
            {
                pBuffer->pData = ((uint8_t*)gApplicationMemory->HostHeaps[i].pMapped)+pBuffer->Alloc.Offset;
            }
            else
            {
                vkMapMemory(gContext->Device, gApplicationMemory->HostHeaps[i].Memory, 0, gApplicationMemory->HostHeaps[i].Size, 0, &gApplicationMemory->HostHeaps[i].pMapped);
                pBuffer->pData = ((uint8_t*)gApplicationMemory->HostHeaps[i].pMapped)+pBuffer->Alloc.Offset;
            }
        }
    }

    return;
}

void Unmap(Resources::Buffer* pBuffer)
{
    vkUnmapMemory(gContext->Device, *pBuffer->Alloc.pMemory);
}
