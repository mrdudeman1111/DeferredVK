#include "Framework.hpp"

#include <stdexcept>

Wrappers::Context* gContext; // The Framework's global context.
Wrappers::Window* gWindow;

bool InitWrapperFW()
{
    VkResult Err;

    gContext = new Wrappers::Context();
    gWindow = new Wrappers::Window();

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Vulkan_LoadLibrary(nullptr);
    gWindow->sdlWindow = SDL_CreateWindow("Framework Renderer", 1280, 720, SDL_WINDOW_VULKAN);
    gWindow->Resolution = {1280, 720};

    uint32_t ExtCount = 0;
    const char* const* SdlExt = SDL_Vulkan_GetInstanceExtensions(&ExtCount);

    std::vector<const char*> Layers = { "VK_LAYER_KHRONOS_validation" };

    VkInstanceCreateInfo InstCI{};
    InstCI.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    InstCI.enabledLayerCount = 1;
    InstCI.ppEnabledLayerNames = Layers.data();
    InstCI.enabledExtensionCount = ExtCount;
    InstCI.ppEnabledExtensionNames = SdlExt;

    if((Err = vkCreateInstance(&InstCI, nullptr, &gContext->Instance)) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create instance");
    }

    uint32_t DeviceCount = 0;
    vkEnumeratePhysicalDevices(gContext->Instance, &DeviceCount, nullptr);
    std::vector<VkPhysicalDevice> PhysDevices(DeviceCount);
    vkEnumeratePhysicalDevices(gContext->Instance, &DeviceCount, PhysDevices.data());

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

    VkPhysicalDeviceMemoryProperties MemProps;
    vkGetPhysicalDeviceMemoryProperties(gContext->PhysDevice, &MemProps);

    for(uint32_t i = 0; i < MemProps.memoryTypeCount; i++)
    {
        if(MemProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        {
            gContext->Local = i;
        }
        else if(MemProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
        {
            gContext->Host = i;
        }
    }

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

    SDL_Vulkan_CreateSurface(gWindow->sdlWindow, gContext->Instance, nullptr, &gWindow->Surface);

    VkDeviceQueueCreateInfo QueueCI[2] = {};

    QueueCI[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    QueueCI[0].queueCount = 1;
    QueueCI[0].queueFamilyIndex = GraphicsFamily;

    QueueCI[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    QueueCI[1].queueCount = 1;
    QueueCI[1].queueFamilyIndex = ComputeFamily;

    std::vector<const char*> DevExt = { "VK_KHR_swapchain" };

    VkDeviceCreateInfo DevCI{};
    DevCI.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    DevCI.enabledExtensionCount = DevExt.size();
    DevCI.ppEnabledExtensionNames = DevExt.data();
    DevCI.queueCreateInfoCount = 2;
    DevCI.pQueueCreateInfos = QueueCI;

    if((Err = vkCreateDevice(gContext->PhysDevice, &DevCI, nullptr, &gContext->Device)) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create vulkan device for global context");
    }

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

    VkSwapchainCreateInfoKHR SwapCI{};
    SwapCI.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    SwapCI.clipped = VK_FALSE;
    SwapCI.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    SwapCI.imageArrayLayers = 1;
    SwapCI.imageColorSpace = gWindow->SurfFormat.colorSpace;
    SwapCI.imageExtent = gWindow->Resolution;
    SwapCI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    SwapCI.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    SwapCI.minImageCount = 2;
    SwapCI.surface = gWindow->Surface;
    SwapCI.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    SwapCI.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;

    if((Err = vkCreateSwapchainKHR(gContext->Device, &SwapCI, nullptr, &gWindow->Swapchain)) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create framework swapchain");
    }

    uint32_t SwpImgCount;
    vkGetSwapchainImagesKHR(gContext->Device, gWindow->Swapchain, &SwpImgCount, nullptr);
    gWindow->SwpImages.resize(SwpImgCount);
    vkGetSwapchainImagesKHR(gContext->Device, gWindow->Swapchain, &SwpImgCount, gWindow->SwpImages.data());

    vkGetDeviceQueue(gContext->Device, GraphicsFamily, 0, &gContext->GraphicsQueue);
    vkGetDeviceQueue(gContext->Device, ComputeFamily, 0, &gContext->ComputeQueue);

    return true;
}

void CloseWrapperFW()
{
    delete gContext;
    delete gWindow;
}

Wrappers::Context* GetContext()
{
    return gContext;
}

Wrappers::Window* GetWindow()
{
    return gWindow;
}
