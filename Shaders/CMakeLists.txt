CMAKE_MINIMUM_REQUIRED(VERSION 3.18.1)

file(GLOB SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.glsl ${CMAKE_CURRENT_SOURCE_DIR}/*.vert ${CMAKE_CURRENT_SOURCE_DIR}/*.frag ${CMAKE_CURRENT_SOURCE_DIR}/*.comp)

foreach(SHAD IN LISTS SHADERS)
    get_filename_component(FILENAME ${SHAD} NAME_WE)
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${FILENAME}.spv
        COMMAND ${Vulkan_GLSLC_EXECUTABLE} ${SHAD} -o ${CMAKE_CURRENT_BINARY_DIR}/${FILENAME}.spv
        DEPENDS ${SHAD}
        COMMENT "Compiling ${FILENAME} shader")
    list(APPEND COMPILED_SHADERS ${CMAKE_CURRENT_BINARY_DIR}/${FILENAME}.spv)
endforeach()

#add_compile_definitions(shader_path ${CMAKE_CURRENT_BINARY_DIR})

add_custom_target(Shaders ALL DEPENDS ${COMPILED_SHADERS})
