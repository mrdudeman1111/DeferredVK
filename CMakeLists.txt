CMAKE_MINIMUM_REQUIRED(VERSION 3.18.1)
project("Proj")
set(CMAKE_CXX_STANDARD 17)

find_package(Vulkan REQUIRED)

if(WIN32)
    add_subdirectory(${CMAKE_SOURCE_DIR}/ThirdParty)
    list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_BINARY_DIR}/ThirdParty/SDL")
endif()

find_package(OpenImageIO CONFIG REQUIRED)
find_package(SDL3 REQUIRED)

include_directories(Vulkan::Headers)
include_directories(${OpenImageIO_INCLUDE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/include)

file(GLOB SceneSrc
    ${CMAKE_SOURCE_DIR}/src/*.cpp ${CMAKE_SOURCE_DIR}/include/*.hpp)

add_executable(VkScene ${SceneSrc})

if(WIN32)
    target_link_directories(VkScene PUBLIC ${CMAKE_BINARY_DIR}/ThirdParty/SDL)
endif()

target_link_libraries(VkScene Vulkan::Vulkan SDL3::SDL3-shared SDL3::Headers OpenImageIO::OpenImageIO)

add_subdirectory(${CMAKE_SOURCE_DIR}/Shaders)

target_compile_definitions(VkScene PRIVATE shader_path="${CMAKE_BINARY_DIR}/Shaders/" PUBLIC $<$<CONFIG:Debug>:DEBUG_MODE> $<$<CONFIG:Release>:RELEASE_MODE> PUBLIC working_directory="${CMAKE_BINARY_DIR}/")
