cmake_minimum_required(VERSION 3.20)
project(ScarlettNativeLinuxVulkan LANGUAGES CXX)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "Linux ELF target only")
endif()
find_package(Vulkan REQUIRED)
add_executable(linux_vulkan_native_probe main.cpp)
target_compile_features(linux_vulkan_native_probe PRIVATE cxx_std_17)
target_compile_options(linux_vulkan_native_probe PRIVATE -Wall -Wextra -Werror)
target_link_libraries(linux_vulkan_native_probe PRIVATE Vulkan::Vulkan)
enable_testing()
add_test(NAME native_linux_vulkan_device_queue COMMAND linux_vulkan_native_probe)
set_tests_properties(native_linux_vulkan_device_queue PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 20)

# Compile an original compute shader to SPIR-V, then execute it via a real
# Vulkan command buffer and verify all shader-written output bytes on the CPU.
find_program(GLSLANG_VALIDATOR NAMES glslangValidator REQUIRED)
set(VK_COMPUTE_SPIRV "${CMAKE_CURRENT_BINARY_DIR}/scarlett_readback.comp.spv")
add_custom_command(
  OUTPUT "${VK_COMPUTE_SPIRV}"
  COMMAND "${GLSLANG_VALIDATOR}" -V --target-env vulkan1.0 -o "${VK_COMPUTE_SPIRV}"
    "${CMAKE_CURRENT_SOURCE_DIR}/readback.comp"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/readback.comp"
  VERBATIM)
add_custom_target(linux_vulkan_compute_spirv ALL DEPENDS "${VK_COMPUTE_SPIRV}")
add_executable(linux_vulkan_compute_readback compute_readback.cpp)
target_compile_features(linux_vulkan_compute_readback PRIVATE cxx_std_17)
target_compile_options(linux_vulkan_compute_readback PRIVATE -Wall -Wextra -Werror)
target_link_libraries(linux_vulkan_compute_readback PRIVATE Vulkan::Vulkan)
add_dependencies(linux_vulkan_compute_readback linux_vulkan_compute_spirv)
add_test(NAME native_linux_vulkan_compute_shader_readback
  COMMAND linux_vulkan_compute_readback "${VK_COMPUTE_SPIRV}")
set_tests_properties(native_linux_vulkan_compute_shader_readback PROPERTIES
  SKIP_RETURN_CODE 77 TIMEOUT 35)

# Genuine original graphics pipeline draw, image transfer and exact CPU pixel checks.
foreach(stage vert frag)
  set(SCARLETT_RASTER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/raster.${stage}")
  set(SCARLETT_RASTER_SPV "${CMAKE_CURRENT_BINARY_DIR}/scarlett_raster.${stage}.spv")
  add_custom_command(OUTPUT "${SCARLETT_RASTER_SPV}"
    COMMAND "${GLSLANG_VALIDATOR}" -V --target-env vulkan1.0
      -o "${SCARLETT_RASTER_SPV}" "${SCARLETT_RASTER_SRC}"
    DEPENDS "${SCARLETT_RASTER_SRC}"
    VERBATIM)
  list(APPEND SCARLETT_RASTER_SHADERS "${SCARLETT_RASTER_SPV}")
endforeach()
add_custom_target(linux_vulkan_raster_shaders ALL DEPENDS ${SCARLETT_RASTER_SHADERS})
add_executable(linux_vulkan_raster_readback raster_readback.cpp)
target_compile_features(linux_vulkan_raster_readback PRIVATE cxx_std_17)
target_compile_options(linux_vulkan_raster_readback PRIVATE -Wall -Wextra -Werror)
target_link_libraries(linux_vulkan_raster_readback PRIVATE Vulkan::Vulkan)
add_dependencies(linux_vulkan_raster_readback linux_vulkan_raster_shaders)
add_test(NAME native_linux_vulkan_graphics_raster_pixel_readback
  COMMAND linux_vulkan_raster_readback
    "${CMAKE_CURRENT_BINARY_DIR}/scarlett_raster.vert.spv"
    "${CMAKE_CURRENT_BINARY_DIR}/scarlett_raster.frag.spv")
set_tests_properties(native_linux_vulkan_graphics_raster_pixel_readback PROPERTIES
  SKIP_RETURN_CODE 77 TIMEOUT 35)
