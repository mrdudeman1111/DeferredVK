CMAKE_MINIMUM_REQUIRED(VERSION 3.18.1)
project("Proj")

find_package(Vulkan REQUIRED)

#add_subdirectory(${CMAKE_SOURCE_DIR}/ThirdParty)

#list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_BINARY_DIR}/ThirdParty/SDL")
find_package(SDL3 REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/include)

file(GLOB SceneSrc
    ${CMAKE_SOURCE_DIR}/src/*.cpp)

add_executable(VkScene ${SceneSrc})

target_link_directories(VkScene PUBLIC ${CMAKE_BINARY_DIR}/ThirdParty/SDL)

target_link_libraries(VkScene Vulkan::Vulkan SDL3::SDL3-shared SDL3::Headers)

add_subdirectory(${CMAKE_SOURCE_DIR}/Shaders)

target_compile_definitions(VkScene PRIVATE shader_path="${CMAKE_BINARY_DIR}/Shaders/" PUBLIC $<$<CONFIG:Debug>:DEBUG_MODE> $<$<CONFIG:Release>:RELEASE_MODE>)
