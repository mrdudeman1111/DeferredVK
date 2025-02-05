CMAKE_MINIMUM_REQUIRED(VERSION 3.18.1)

if(NOT (EXISTS "${CMAKE_CURRENT_BINARY_DIR}/SDL"))
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/SDL)
endif()

if(NOT (EXISTS "$CMAKE_CURRENT_BINARY_DIR}/VulkanMemoryAllocator"))
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/VulkanMemoryAllocator)
endif()
