#include "Framework.hpp"

#include <stdexcept>

Window* gWindow; // The Framework's global context.

Context* gContext;

TransferAgent* gTransferAgent;

/*! \brief All the application's memory */
struct Memory
{
private:
    struct MemoryHeap
    {
        size_t Size;
        size_t Available;

        uint32_t AllocTail = 0;

        VkDeviceMemory Memory;
    };

public:
    std::vector<MemoryHeap> HostHeaps; // host visible memory heaps
    std::vector<MemoryHeap> LocalHeaps; // device local memory heaps
} ApplicationMemory;


bool InitWrapperFW(uint32_t Width, uint32_t Height)
{
    VkResult Err;

    // initiate globals
    gContext = new Context();
    gWindow = new Window();

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
    std::vector<const char*> Layers = { "VK_LAYER_KHRONOS_validation" };

    // instance creation info
    VkInstanceCreateInfo InstCI{};
    InstCI.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    InstCI.enabledLayerCount = 1;
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

    for(uint32_t i = 0; i < FamilyCount; i++)
    {
        if(Families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            GraphicsFamily = i;
        }
        else if(Families[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
        {
            ComputeFamily = i;
        }
    }

    // create render surface from window using SDL
    SDL_Vulkan_CreateSurface(gWindow->sdlWindow, gContext->Instance, nullptr, &gWindow->Surface);

    /* Device creation*/

        // Graphics/Compute Queue creation info
        VkDeviceQueueCreateInfo QueueCI[2] = {};

        QueueCI[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        QueueCI[0].queueCount = 1;
        QueueCI[0].queueFamilyIndex = GraphicsFamily;

        QueueCI[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        QueueCI[1].queueCount = 1;
        QueueCI[1].queueFamilyIndex = ComputeFamily;

        // Device extensions to enable
        std::vector<const char*> DevExt = { "VK_KHR_swapchain" };

        // Device Creation info
        VkDeviceCreateInfo DevCI{};
        DevCI.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        DevCI.enabledExtensionCount = DevExt.size();
        DevCI.ppEnabledExtensionNames = DevExt.data();
        DevCI.queueCreateInfoCount = 2;
        DevCI.pQueueCreateInfos = QueueCI;

        // Create device
        if((Err = vkCreateDevice(gContext->PhysDevice, &DevCI, nullptr, &gContext->Device)) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create vulkan device for global context");
        }

        // retrieve device queues created during device creation
        vkGetDeviceQueue(gContext->Device, GraphicsFamily, 0, &gContext->GraphicsQueue);
        vkGetDeviceQueue(gContext->Device, ComputeFamily, 0, &gContext->ComputeQueue);

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
        SwapCI.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;

        SwapCI.imageArrayLayers = 1;
        SwapCI.imageFormat = gWindow->SurfFormat.format;
        SwapCI.imageColorSpace = gWindow->SurfFormat.colorSpace;
        SwapCI.imageExtent = gWindow->Resolution;
        SwapCI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        SwapCI.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        SwapCI.minImageCount = 2;

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
    pTransferAgentPool->Bake(false);

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

    vkDestroySwapchainKHR(gContext->Device, gWindow->Swapchain, nullptr);
    vkDestroyDevice(gContext->Device, nullptr);
    vkDestroyInstance(gContext->Instance, nullptr);

    delete gTransferAgent;
    delete gContext;
    delete gWindow;
}

    void TransferAgent::Transfer(void* srcData, size_t srcSize, Resources::Buffer* dstBuff, size_t dstOffset)
    {
        VkBufferCopy tmp;
        tmp.srcOffset = TransitTail;
        tmp.dstOffset = dstOffset;
        tmp.size = srcSize;

        BufferCopies.push_back(tmp);

        TransferBuffers.push_back(dstBuff);

        memcpy(((uint8_t*)pTransitBuffer->pData)+TransitTail, srcData, srcSize);

        TransitTail += srcSize;
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
        pWaitThread = new std::thread([this] { FlushImpl(); });
    }

    void TransferAgent::AwaitFlush()
    {
        if(bFlushing)
        {
            pWaitThread->join();
        }

        return;
    }

    void TransferAgent::FlushImpl()
    {
        if(pWaitThread != nullptr)
        {
            // implicitly wait on flush
            AwaitFlush();
        }

        if(pCmdBuff == nullptr)
        {
            pCmdBuff = cmdAllocator->CreateBuffer();
        }

        pCmdBuff->Start();

            for(uint32_t i = 0; i < BufferCopies.size(); i++)
            {
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

        cmdAllocator->Submit(pCmdBuff, pTransferFence);

        bFlushing = true;
        pTransferFence->Wait();
        TransitTail = 0;
        bFlushing = false;
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

    vkCreateFence(GetContext()->Device, &FenceCI, nullptr, pRet->GetFence());

    return pRet;
}

void Allocate(Resources::Buffer& Buffer, bool bVisible)
{
    VkResult Err;

    VkMemoryRequirements MemReq;

    vkGetBufferMemoryRequirements(gContext->Device, Buffer, &MemReq);

    Buffer.pData = nullptr;

    bool bBound = false;

    if(bVisible)
    {
        for(uint32_t i = 0; i < ApplicationMemory.HostHeaps.size(); i++)
        {
            size_t MemLoc = ApplicationMemory.HostHeaps[i].AllocTail + (ApplicationMemory.HostHeaps[i].AllocTail % MemReq.alignment); // align the alloc tail to the alignment requirements of our buffer.
            size_t Size = ApplicationMemory.HostHeaps[i].Size - MemLoc;

            if(Size >= MemReq.size)
            {
                Buffer.Alloc.Offset = MemLoc;
                Buffer.Alloc.Size = MemReq.size;
                Buffer.Alloc.pMemory = &ApplicationMemory.HostHeaps[i].Memory;

                Err = vkBindBufferMemory(gContext->Device, Buffer, ApplicationMemory.HostHeaps[i].Memory, MemLoc);
                bBound = true;
            }
        }
        if(!bBound)
        {
            ApplicationMemory.HostHeaps.push_back({});
            uint32_t EndIdx = ApplicationMemory.HostHeaps.size()-1;

            if(MemReq.size > PAGE_SIZE)
            {
                VkMemoryAllocateInfo AllocInf{};
                AllocInf.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                AllocInf.memoryTypeIndex = gContext->Host;
                AllocInf.allocationSize = MemReq.size;

                if(vkAllocateMemory(gContext->Device, &AllocInf, nullptr, &ApplicationMemory.HostHeaps[EndIdx].Memory) != VK_SUCCESS)
                {
                    throw std::runtime_error("Failed to allocate memory, ran out of space");
                }

                ApplicationMemory.HostHeaps[EndIdx].Size = MemReq.size;
                ApplicationMemory.HostHeaps[EndIdx].AllocTail = 0;
                ApplicationMemory.HostHeaps[EndIdx].Available = 0;
                
                Buffer.Alloc.Offset = 0;
                Buffer.Alloc.Size = MemReq.size;
                Buffer.Alloc.pMemory = &ApplicationMemory.HostHeaps[EndIdx].Memory;

                Err = vkBindBufferMemory(gContext->Device, Buffer, ApplicationMemory.HostHeaps[EndIdx].Memory, 0);
            }
            else
            {
                VkMemoryAllocateInfo AllocInf{};
                AllocInf.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                AllocInf.memoryTypeIndex = gContext->Host;
                AllocInf.allocationSize = PAGE_SIZE;

                if(vkAllocateMemory(gContext->Device, &AllocInf, nullptr, &ApplicationMemory.HostHeaps[EndIdx].Memory))
                {
                    throw std::runtime_error("Failed to allocate a host heap for an allocation");
                }

                ApplicationMemory.HostHeaps[EndIdx].Size = PAGE_SIZE;
                ApplicationMemory.HostHeaps[EndIdx].AllocTail = 0;
                ApplicationMemory.HostHeaps[EndIdx].Available = MemReq.size - AllocInf.allocationSize;
                
                Buffer.Alloc.Offset = 0;
                Buffer.Alloc.Size = MemReq.size;
                Buffer.Alloc.pMemory = &ApplicationMemory.HostHeaps[EndIdx].Memory;

                Err = vkBindBufferMemory(gContext->Device, Buffer, ApplicationMemory.HostHeaps[EndIdx].Memory, 0);
            }
        }
    }
    else
    {
        for(uint32_t i = 0; i < ApplicationMemory.LocalHeaps.size(); i++)
        {
            size_t MemLoc = ApplicationMemory.LocalHeaps[i].AllocTail + (ApplicationMemory.LocalHeaps[i].AllocTail % MemReq.alignment); // align the alloc tail to the alignment requirements of our buffer.
            size_t Size = ApplicationMemory.LocalHeaps[i].Size - MemLoc;

            if(Size >= MemReq.size)
            {
                Buffer.Alloc.Offset = MemLoc;
                Buffer.Alloc.Size = MemReq.size;
                Buffer.Alloc.pMemory = &ApplicationMemory.LocalHeaps[i].Memory;

                Err = vkBindBufferMemory(gContext->Device, Buffer, ApplicationMemory.LocalHeaps[i].Memory, MemLoc);
                bBound = true;
            }
        }
        if(!bBound)
        {
            ApplicationMemory.LocalHeaps.push_back({});
            uint32_t EndIdx = ApplicationMemory.LocalHeaps.size()-1;

            if(MemReq.size > PAGE_SIZE)
            {
                VkMemoryAllocateInfo AllocInf{};
                AllocInf.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                AllocInf.memoryTypeIndex = gContext->Local;
                AllocInf.allocationSize = MemReq.size;

                if(vkAllocateMemory(gContext->Device, &AllocInf, nullptr, &ApplicationMemory.LocalHeaps[EndIdx].Memory) != VK_SUCCESS)
                {
                    throw std::runtime_error("Failed to allocate memory, ran out of space");
                }

                ApplicationMemory.LocalHeaps[EndIdx].Size = MemReq.size;
                ApplicationMemory.LocalHeaps[EndIdx].AllocTail = 0;
                ApplicationMemory.LocalHeaps[EndIdx].Available = 0;
                
                Buffer.Alloc.Offset = 0;
                Buffer.Alloc.Size = MemReq.size;
                Buffer.Alloc.pMemory = &ApplicationMemory.HostHeaps[EndIdx].Memory;

                Err = vkBindBufferMemory(gContext->Device, Buffer, ApplicationMemory.LocalHeaps[EndIdx].Memory, 0);
            }
            else
            {
                VkMemoryAllocateInfo AllocInf{};
                AllocInf.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                AllocInf.memoryTypeIndex = gContext->Local;
                AllocInf.allocationSize = PAGE_SIZE;

                if(vkAllocateMemory(gContext->Device, &AllocInf, nullptr, &ApplicationMemory.LocalHeaps[EndIdx].Memory))
                {
                    throw std::runtime_error("Failed to allocate a host heap for an allocation");
                }

                ApplicationMemory.LocalHeaps[EndIdx].Size = PAGE_SIZE;
                ApplicationMemory.LocalHeaps[EndIdx].AllocTail = 0;
                ApplicationMemory.LocalHeaps[EndIdx].Available = MemReq.size - AllocInf.allocationSize;
                
                Buffer.Alloc.Offset = 0;
                Buffer.Alloc.Size = MemReq.size;
                Buffer.Alloc.pMemory = &ApplicationMemory.HostHeaps[EndIdx].Memory;

                Err = vkBindBufferMemory(gContext->Device, Buffer, ApplicationMemory.LocalHeaps[EndIdx].Memory, 0);
            }
        }
    }

    if(Err != VK_SUCCESS) throw std::runtime_error("Failed to bind a buffer.");

    return;
}

void Allocate(Resources::Image& Image, bool bVisible)
{
    VkMemoryRequirements MemReq;

    vkGetImageMemoryRequirements(gContext->Device, Image.Img, &MemReq);

    if(bVisible)
    {
        for(uint32_t i = 0; i < ApplicationMemory.HostHeaps.size(); i++)
        {
            size_t MemLoc = ApplicationMemory.HostHeaps[i].AllocTail + (ApplicationMemory.HostHeaps[i].AllocTail % MemReq.alignment); // align the alloc tail to the alignment requirements of our buffer.
            size_t Size = ApplicationMemory.HostHeaps[i].Size - MemLoc;

            if(Size >= MemReq.size)
            {
                Image.Alloc.Offset = MemLoc;
                Image.Alloc.Size = MemReq.size;
                Image.Alloc.pMemory = &ApplicationMemory.HostHeaps[i].Memory;

                vkBindImageMemory(gContext->Device, Image.Img, ApplicationMemory.HostHeaps[i].Memory, MemLoc);
            }
        }
    }
    else
    {
        for(uint32_t i = 0; i < ApplicationMemory.LocalHeaps.size(); i++)
        {
            size_t MemLoc = ApplicationMemory.LocalHeaps[i].AllocTail + (ApplicationMemory.LocalHeaps[i].AllocTail % MemReq.alignment); // align the alloc tail to the alignment requirements of our buffer.
            size_t Size = ApplicationMemory.LocalHeaps[i].Size - MemLoc;

            if(Size >= MemReq.size)
            {
                Image.Alloc.Offset = MemLoc;
                Image.Alloc.Size = MemReq.size;
                Image.Alloc.pMemory = &ApplicationMemory.LocalHeaps[i].Memory;

                vkBindImageMemory(gContext->Device, Image.Img, ApplicationMemory.LocalHeaps[i].Memory, MemLoc);
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
    vkMapMemory(gContext->Device, *pBuffer->Alloc.pMemory, pBuffer->Alloc.Offset, pBuffer->Alloc.Size, 0, &pBuffer->pData);
}

void Unmap(Resources::Buffer* pBuffer)
{
    vkUnmapMemory(gContext->Device, *pBuffer->Alloc.pMemory);
}
