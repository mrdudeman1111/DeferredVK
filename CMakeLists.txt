CMAKE_MINIMUM_REQUIRED(VERSION 3.18.1)
project("Proj")

include_directories($ENV{VULKAN_SDK}/Include)
link_directories($ENV{VULKAN_SDK}/Lib)

include_directories(${CMAKE_SOURCE_DIR}/include)

link_directories(${CMAKE_BINARY_DIR})

file(GLOB SceneSrc
    ${CMAKE_SOURCE_DIR}/src/*.cpp)

add_executable(VkScene ${SceneSrc})

target_link_libraries(VkScene vulkan-1 SDL3)
