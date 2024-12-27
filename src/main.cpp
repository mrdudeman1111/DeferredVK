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

    CloseWrapperFW();

    std::cout << "Good run\n";
    return 0;
}
