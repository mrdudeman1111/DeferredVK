#include <iostream>

#include "Framework.hpp"

int main()
{
    InitWrapperFW();

    Wrappers::PipelineProfile MainProf;
    MainProf.Topo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    MainProf.MsaaSamples = VK_SAMPLE_COUNT_1_BIT;

    MainProf.RenderSize = {1280, 720};
    MainProf.RenderOffset = {0, 0};

    MainProf.bStencilTesting = true;
    MainProf.bDepthTesting = true;
    MainProf.DepthRange = {0.f, 1.f};
    MainProf.DepthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    VkImageCreateInfo DepthBuffer{};
    DepthBuffer.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    DepthBuffer.extent = { GetWindow()->Resolution.width, GetWindow()->Resolution.height, 1 };
    DepthBuffer.format = VK_FORMAT_D32_SFLOAT;
    DepthBuffer.arrayLayers = 1;
    DepthBuffer.imageType = VK_IMAGE_TYPE_2D;
    DepthBuffer.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    DepthBuffer.mipLevels = 1;
    DepthBuffer.samples = VK_SAMPLE_COUNT_1_BIT;
    DepthBuffer.tiling = VK_IMAGE_TILING_OPTIMAL;
    DepthBuffer.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    DepthBuffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    std::vector<Wrappers::Resources::FrameBuffer> FB(GetWindow()->SwpImages.size());
    for(uint32_t i = 0; i < FB.size(); i++)
    {
        FB[i].AddBuffer(DepthBuffer);
    }

    VkAttachmentReference ColorRef{};
    ColorRef.attachment = 0;
    ColorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference DepthRef{};
    DepthRef.attachment = 1;
    DepthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription ColorDesc{};
    ColorDesc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    ColorDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    ColorDesc.samples = VK_SAMPLE_COUNT_1_BIT;
    ColorDesc.format = GetWindow()->SurfFormat.format;
    ColorDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    ColorDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    ColorDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    ColorDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    VkAttachmentDescription DepthDesc{};
    DepthDesc.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    DepthDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    DepthDesc.samples = VK_SAMPLE_COUNT_1_BIT;
    DepthDesc.format = VK_FORMAT_D32_SFLOAT;
    DepthDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    DepthDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    DepthDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    DepthDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;

    FbDesc.AddBuffer(ColorDesc, ColorRef);
    FbDesc.AddBuffer(DepthDesc, DepthRef);

    CloseWrapperFW();

    std::cout << "Good run\n";
    return 0;
}
