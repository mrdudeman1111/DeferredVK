#pragma once

#include "Wrappers.hpp"

#define PAGE_SIZE 16777216

bool InitWrapperFW();
void CloseWrapperFW();

Wrappers::Context* GetContext();

Wrappers::Window* GetWindow();

/* creation functions */

    void Allocate(Resources::Image& Buffer);
    void Allocate(Resources::Buffer& Image);

    VkResult CreateBuffer(VkBuffer& Buffer, size_t Size, VkBufferUsageFlags Usage);
    VkResult CreateImage(VkImage& Image, VkFormat Format, VkExtent2D Size, VkImageUsageFlags Usage, VkSampleCountFlagBits SampleCount = VK_SAMPLE_COUNT_1_BIT);

    VkResult CreateView(VkImageView& View, VkImage& Image, VkFormat Format);

    // virtual void CreateTexture(Resources::Image& Texture, VkFormat Format, VkExtent2D Size) = 0;

    void* Map(Wrappers::Resources::Allocation* pAllocation);
    void Unmap(Wrappers::Resources::Allocation* pAllocation);
