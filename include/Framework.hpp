#pragma once

#include "Wrappers.hpp"

#define PAGE_SIZE 16777216

bool InitWrapperFW();
void CloseWrapperFW();

Context* GetContext();

Window* GetWindow();


/* creation functions */
    /*! \brief Allocate a Buffer
        @param Buffer The addres of the buffer to allocate
        @param bVisible Determines whether the buffer should be allocated on host visible memory
    */
    void Allocate(Resources::Buffer& Buffer, bool bVisible = true);
    
    /*! \brief Allocate Image
        @param Buffer The addres of the buffer to allocate
        @param bVisible Determines whether the buffer should be allocated on host visible memory
    */
    void Allocate(Resources::Image& Image, bool bVisible = true);

    /*! \brief Create buffer
        @param Buffer The buffer object to output to.
        @param Size The size of the buffer object to create.
        @param Usage The buffer's usage flags
    */
    VkResult CreateBuffer(Resources::Buffer& Buffer, size_t Size, VkBufferUsageFlags Usage);

    /*! \brief Create image
        @param Image The Image object to output to.
        @param Format The format of the image.
        @param Size The resolution of the desired image.
        @param Usage How the image will be used (VkImageUsageFlagBits)
        @param SampleCount The Sample size of the image, most often used in msaa. (2x, 4x, 8x, 16x, etc)
    */
    VkResult CreateImage(Resources::Image& Image, VkFormat Format, VkExtent2D Size, VkImageUsageFlags Usage, VkSampleCountFlagBits SampleCount = VK_SAMPLE_COUNT_1_BIT);

    /*! \brief Create View
        @param View The Image view to output to.
        @param Image The image the output view wraps.
        @param Format The format of the view
        @param Aspect The subresource aspect
    */
    VkResult CreateView(VkImageView& View, VkImage& Image, VkFormat Format, VkImageAspectFlagBits Aspect = VK_IMAGE_ASPECT_COLOR_BIT);

    // virtual void CreateTexture(Resources::Image& Texture, VkFormat Format, VkExtent2D Size) = 0;

    void Map(Resources::Buffer* pBuffer);
    void Unmap(Resources::Buffer* pBuffer);
