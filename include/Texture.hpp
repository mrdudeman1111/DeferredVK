#pragma once

#include "Wrappers.hpp"

namespace Texture
{
    class Texture2D
    {
    public:
        Texture2D() : pTexImg(nullptr), Sampler(VK_NULL_HANDLE) {};

        void SetImg(Resources::Image* pImg);

        VkDescriptorImageInfo GetDescriptorImg();

    private:
        Resources::Image* pTexImg;
        VkSampler Sampler;
    };
}
