# update current positions
set(PROJECT_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/)
set(PROJECT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/)
set(EXTERNAL_LIB_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}../../ExternalLib/)
set(SHADER_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../Resources/Shaders/)

include(cmake/filters/SetShaderFilters.cmake)
include(cmake/filters/SetProjectFilters.cmake)
include(cmake/SetSourceGroups.cmake)
include(cmake/filters/SetExternalLibsFilters.cmake)

file(
  GLOB_RECURSE
  VULKANRENDERER_SOURCES
  "*.hpp"
  "*.cpp")

if(RUST_FEATURES)
  corrosion_import_crate(MANIFEST_PATH rusty_code/Cargo.toml)
  corrosion_add_cxxbridge(rusty_bridge CRATE rusty_code MANIFEST_PATH rusty_code FILES lib.rs)
endif()

# add the executable
add_executable(
  ${PROJECT_NAME}
  ${RASTER_SHADER_FILTER}
  ${RAYTRACING_SHADER_FILTER}
  ${COMMON_SHADER_FILTER}
  ${POST_SHADER_FILTER}
  ${BRDF_SHADER_FILTER}
  ${PBR_SHADER_FILTER}
  ${PATH_TRACING_SHADER_FILTER}
  ${VULKANRENDERER_SOURCES}
  # this is great; no CPPCHECK,CLANG_TIDY here
  $<TARGET_OBJECTS:IMGUI>)

set(ShaderIncludes
    -I
    ${SHADER_SRC_DIR}common/
    -I
    ${SHADER_SRC_DIR}pbr/
    -I
    ${SHADER_SRC_DIR}pbr/brdf/
    -I
    ${SHADER_SRC_DIR}hostDevice/
    -I
    ${CMAKE_CURRENT_SOURCE_DIR}/renderer/
    -I
    ${CMAKE_CURRENT_SOURCE_DIR}/renderer/pushConstants/
    -I
    ${CMAKE_CURRENT_SOURCE_DIR}/scene/)

# Join the paths into a single space-separated string
string(
  REPLACE ";"
          " "
          ShaderIncludesString
          "${ShaderIncludes}")

message("The shader include string is the following: ${ShaderIncludesString}")

if(RUST_FEATURES)
target_compile_definitions(
  ${PROJECT_NAME}
  PRIVATE USE_RUST=1)
else()
target_compile_definitions(
  ${PROJECT_NAME}
  PRIVATE USE_RUST=0)
endif()

if(NOT MSVC)
  target_compile_definitions(
    ${PROJECT_NAME}
    PRIVATE RELATIVE_RESOURCE_PATH="/../Resources/"
            RELATIVE_INCLUDE_PATH="/../Src/GraphicsEngineVulkan/"
            RELATIVE_IMGUI_FONTS_PATH="/../ExternalLib/IMGUI/misc/fonts/"
            ShaderIncludesString="${ShaderIncludesString}")
else()
  target_compile_definitions(
    ${PROJECT_NAME}
    PRIVATE RELATIVE_RESOURCE_PATH="/../../Resources/"
            RELATIVE_INCLUDE_PATH="/../../Src/GraphicsEngineVulkan/"
            RELATIVE_IMGUI_FONTS_PATH="/../../ExternalLib/IMGUI/misc/fonts/"
            ShaderIncludesString="${ShaderIncludesString}")
endif()

configure_file(VulkanRendererConfig.hpp.in "${CMAKE_CURRENT_SOURCE_DIR}/renderer/VulkanRendererConfig.hpp")

target_include_directories(
  ${PROJECT_NAME}
  PUBLIC ${PROJECT_INCLUDE_DIR}renderer
         ${PROJECT_INCLUDE_DIR}renderer/pushConstants
         ${PROJECT_INCLUDE_DIR}renderer/accelerationStructures
         ${PROJECT_INCLUDE_DIR}vulkan_base
         ${PROJECT_INCLUDE_DIR}gui
         ${PROJECT_INCLUDE_DIR}common
         ${PROJECT_INCLUDE_DIR}scene
         ${PROJECT_INCLUDE_DIR}window
         ${PROJECT_INCLUDE_DIR}memory
         ${PROJECT_INCLUDE_DIR}util
         ${PROJECT_INCLUDE_DIR}app
         ${SHADER_SRC_DIR}hostDevice
         ${PROJECT_INCLUDE_DIR})

target_include_directories(${PROJECT_NAME} PRIVATE ${Vulkan_INCLUDE_DIRS})

target_link_libraries(
  ${PROJECT_NAME}
  PUBLIC ${CMAKE_DL_LIBS}
         Threads::Threads
         Vulkan::Vulkan
         glfw
         imgui
         stb
         glm
         tinyobjloader
         vma
         ktx
         # enable compiler warnings
         myproject_warnings
         # enable sanitizers
         myproject_options
  PRIVATE GSL spdlog::spdlog nlohmann_json::nlohmann_json rusty_bridge)

if(RUST_FEATURES)
target_link_libraries(
  ${PROJECT_NAME}
  PRIVATE rusty_bridge)
endif()

# compile shaders as part of my cmake routine right away :)
include(cmake/CompileShadersToSPV.cmake)

foreach(Shader ${POST_SHADER_FILTER})
  add_shader(${PROJECT_NAME} ${Shader})
endforeach()

foreach(Shader ${RAYTRACING_SHADER_FILTER})
  add_shader(${PROJECT_NAME} ${Shader})
endforeach()

foreach(Shader ${RASTER_SHADER_FILTER})
  add_shader(${PROJECT_NAME} ${Shader})
endforeach()

foreach(Shader ${PATH_TRACING_SHADER_FILTER})
  add_shader(${PROJECT_NAME} ${Shader})
endforeach()
