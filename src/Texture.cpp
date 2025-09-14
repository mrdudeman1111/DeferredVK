#include "Texture.hpp"
#include "Framework.hpp"

#include "OpenImageIO/imageio.h"

using namespace OIIO;

namespace Texture
{
    void Texture2D::SetImg(Resources::Image* pImg)
    {
        VkResult Err = VK_SUCCESS;

        pTexImg = pImg;

        if(Sampler == VK_NULL_HANDLE)
        {
            vkDestroySampler(GetContext()->Device, Sampler, nullptr);
            
            VkSamplerCreateInfo SamplerCI{};
            SamplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            SamplerCI.minFilter = VK_FILTER_NEAREST;
            SamplerCI.magFilter = VK_FILTER_NEAREST;
            SamplerCI.minLod = -1.f;
            SamplerCI.maxLod = VK_LOD_CLAMP_NONE;
            SamplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            SamplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
            SamplerCI.anisotropyEnable = VK_TRUE;
            SamplerCI.compareEnable = VK_FALSE;
            
            SamplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            SamplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            SamplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            
            if((Err = vkCreateSampler(GetContext()->Device, &SamplerCI, nullptr, &Sampler)) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create sampler for texture");
            }
        }
    }

    VkDescriptorImageInfo Texture2D::GetDescriptorImg()
    {
        VkDescriptorImageInfo Ret{};
        Ret.imageView = pTexImg->View;
        Ret.imageLayout = pTexImg->Layout;
        Ret.sampler = Sampler;

        return Ret;
    }
}
