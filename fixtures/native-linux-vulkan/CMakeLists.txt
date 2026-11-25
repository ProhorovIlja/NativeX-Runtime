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

# The partition shader has two storage buffers: CPU-generated input and GPU output.
find_package(Python3 QUIET COMPONENTS Interpreter)
find_program(SPIRV_VAL NAMES spirv-val)
if(Python3_Interpreter_FOUND AND SPIRV_VAL)
  set(partition_shader "${CMAKE_CURRENT_SOURCE_DIR}/../native_wave64_partition.comp")
  set(partition_oracle "${CMAKE_CURRENT_SOURCE_DIR}/../../tools/wave64_partition_vectors.py")
  set(partition_spv "${CMAKE_CURRENT_BINARY_DIR}/wave64_partition.spv")
  set(partition_input "${CMAKE_CURRENT_BINARY_DIR}/wave64_input.u32")
  set(partition_reference "${CMAKE_CURRENT_BINARY_DIR}/wave64_reference.u32")
  add_custom_command(
    OUTPUT "${partition_spv}"
    COMMAND "${GLSLANG_VALIDATOR}" -V --target-env vulkan1.1 -o "${partition_spv}.tmp" "${partition_shader}"
    COMMAND "${SPIRV_VAL}" --target-env vulkan1.1 "${partition_spv}.tmp"
    COMMAND "${CMAKE_COMMAND}" -E rename "${partition_spv}.tmp" "${partition_spv}"
    DEPENDS "${partition_shader}"
    VERBATIM)
  add_custom_command(
    OUTPUT "${partition_input}" "${partition_reference}"
    COMMAND "${Python3_EXECUTABLE}" "${partition_oracle}" "${partition_input}" "${partition_reference}"
    DEPENDS "${partition_oracle}"
    VERBATIM)
  add_custom_target(linux_vulkan_wave64_partition ALL
    DEPENDS "${partition_spv}" "${partition_input}" "${partition_reference}")
  add_dependencies(linux_vulkan_wave64_partition linux_vulkan_compute_readback)
  add_test(NAME native_linux_vulkan_wave64_partition
    COMMAND linux_vulkan_compute_readback "${partition_spv}" "${partition_input}" "${partition_reference}")
  set_tests_properties(native_linux_vulkan_wave64_partition PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 35)
else()
  message(STATUS "Vulkan wave64 partition test requires both Python 3 and spirv-val")
endif()

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
