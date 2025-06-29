#pragma once

#include "Wrappers.hpp"
#include <thread>

#define PAGE_SIZE 16777216

bool InitWrapperFW(uint32_t Width = 1280, uint32_t Height = 720);
void CloseWrapperFW();

Context* GetContext();

Window* GetWindow();

/* creation functions */
    /*! \brief Create a semaphore */
    VkSemaphore CreateVulkanSemaphore();

    /*! \brief Create a Fence */
    Resources::Fence* CreateFence(std::string Name = "Fence");

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

class TransferAgent
{
    public:
        TransferAgent(Allocators::CommandPool* pCmdPool) : cmdAllocator{pCmdPool}
        {
            BufferCopies = {};
            TransferBuffers = {};

            BuffImageCopies = {};
            TransferBuffImages = {};
            BufferImageLayouts = {};

            ImageCopies = {};
            TransferImages= {};
            ImageLayouts = {};

            TransitTail = false;

            bFlushing = false;

            pTransitBuffer = new Resources::Buffer();

            if(CreateBuffer(*pTransitBuffer, 16000000, VK_BUFFER_USAGE_TRANSFER_SRC_BIT) != VK_SUCCESS)
            {
                std::cout << "Failed to create transit buffer.";
            }

            pCmdBuff = cmdAllocator->CreateBuffer();

            Allocate(*pTransitBuffer, true);

            Map(pTransitBuffer);

            pTransferFence = CreateFence("Trasfer Fence");
        }
        ~TransferAgent()
        {
            delete pCmdBuff;
            delete cmdAllocator;
        }

        void Transfer(void* srcData, size_t srcSize, Resources::Buffer* dstBuff, size_t dstOffset = 0);
        void Transfer(void* srcData, size_t srcSize, Resources::Image* dstImg, VkExtent3D Extent, VkOffset3D ImgOffset, VkImageSubresourceLayers SubResource, VkImageLayout Layout);
        void Transfer(Resources::Image* srcImg, Resources::Image*dstImg, VkImageLayout srcLayout, VkImageLayout dstLayout, VkOffset3D srcOffset, VkOffset3D dstOffset, VkImageSubresourceLayers srcSubResource, VkImageSubresourceLayers dstSubResource, VkExtent3D Size);

        /*! \brief Submit all transfer commands */
        void Flush();
        void AwaitFlush();

        Resources::Fence* pTransferFence;

    private:

        void FlushImpl();
        bool bFlushing;

        Resources::Buffer* pTransitBuffer;
        size_t TransitTail;

        Allocators::CommandPool* cmdAllocator;
        Resources::CommandBuffer* pCmdBuff;
        std::thread* pWaitThread;

        std::vector<VkBufferCopy> BufferCopies;
        std::vector<Resources::Buffer*> TransferBuffers;

        std::vector<VkBufferImageCopy> BuffImageCopies;
        std::vector<Resources::Image*> TransferBuffImages;
        std::vector<VkImageLayout> BufferImageLayouts;

        std::vector<VkImageCopy> ImageCopies;
        std::vector<std::pair<Resources::Image*, Resources::Image*>> TransferImages;
        std::vector<std::pair<VkImageLayout, VkImageLayout>> ImageLayouts;
};

TransferAgent* GetTransferAgent();
