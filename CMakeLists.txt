cmake_minimum_required(VERSION 3.20)
project(NativeXRuntime LANGUAGES CXX)
include(CTest)
add_executable(pe_intake src/pe_intake.cpp)
target_compile_features(pe_intake PRIVATE cxx_std_17)
if(MSVC)
  target_compile_options(pe_intake PRIVATE /W4 /WX /permissive-)
else()
  target_compile_options(pe_intake PRIVATE -Wall -Wextra -Werror)
endif()
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME pe32plus_readonly_fixture COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/check_pe_intake.py" "$<TARGET_FILE:pe_intake>")
# Explicitly invoked, offline source-built large PE32+ fixture: >65k AMD64
# pdata records. Ensures --compact inspects them without large JSON artifacts.
add_test(NAME pe32plus_large_pdata_compact_selftest
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/check_large_pe_compact.py"
    "$<TARGET_FILE:pe_intake>")
set_tests_properties(pe32plus_large_pdata_compact_selftest PROPERTIES TIMEOUT 25)
# Native Windows x64 import dependency planning: no guest execution/rebinding.
# The synthetic PE is read by pe_intake, then mapped only through explicit host
# export or native-adapter declarations. Missing and unsupported imports fail closed.
add_test(NAME pe32plus_native_import_binding_plan_pipeline
  COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_SOURCE_DIR}/tests/check_pe_binding_pipeline.py"
    "$<TARGET_FILE:pe_intake>")
set_tests_properties(pe32plus_native_import_binding_plan_pipeline PROPERTIES TIMEOUT 15)

# Optional real Windows x64 PE32+ export-table inspection using original C and
# .def fixtures compiled by Clang/LLVM. No DLL is loaded or executed; missing
# clang/lld-link yields an explicit CTest skip (77), never a fabricated pass.
add_test(NAME pe32plus_sourcebuilt_host_dll_exports_and_forwarders
  COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_SOURCE_DIR}/tests/check_sourcebuilt_pe_host_exports.py"
    "$<TARGET_FILE:pe_intake>")
set_tests_properties(pe32plus_sourcebuilt_host_dll_exports_and_forwarders
  PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 45)
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  add_executable(win_native_probe src/platform_probe.cpp)
  target_compile_features(win_native_probe PRIVATE cxx_std_17)
  if(MSVC)
    target_compile_options(win_native_probe PRIVATE /W4 /WX /permissive-)
  else()
    target_compile_options(win_native_probe PRIVATE -Wall -Wextra -Werror)
  endif()
  add_test(NAME native_windows_host_selftest COMMAND win_native_probe --self-test)
endif()


if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  # Source-built DLL with two exported functions; the host loads it by name
  # and by ordinal to verify native Windows x64 loader / ABI semantics.
  add_library(scarlett_abi_fixture SHARED
    fixtures/abi_library.cpp
    fixtures/abi_exports.def)
  target_compile_features(scarlett_abi_fixture PRIVATE cxx_std_17)
  add_executable(scarlett_abi_host_test fixtures/abi_host_test.cpp)
  target_compile_features(scarlett_abi_host_test PRIVATE cxx_std_17)
  if(MSVC)
    target_compile_options(scarlett_abi_host_test PRIVATE /W4 /WX /permissive-)
    target_compile_options(scarlett_abi_fixture PRIVATE /W4 /WX /permissive-)
  endif()
  add_test(NAME native_loader_named_ordinal_exports
    COMMAND scarlett_abi_host_test "$<TARGET_FILE:scarlett_abi_fixture>")
endif()

add_executable(wave64_cpu_oracle fixtures/wave64_cpu_oracle.cpp)
target_compile_features(wave64_cpu_oracle PRIVATE cxx_std_17)
if(MSVC)
  target_compile_options(wave64_cpu_oracle PRIVATE /W4 /WX /permissive-)
else()
  target_compile_options(wave64_cpu_oracle PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME wave64_cpu_oracle COMMAND wave64_cpu_oracle)

if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  add_executable(d3d12_warp_smoke fixtures/d3d12_warp_smoke.cpp)
  target_compile_features(d3d12_warp_smoke PRIVATE cxx_std_17)
  target_link_libraries(d3d12_warp_smoke PRIVATE d3d12 dxgi)
  if(MSVC)
    target_compile_options(d3d12_warp_smoke PRIVATE /W4 /WX /permissive-)
  endif()
  add_test(NAME d3d12_warp_queue_and_fence COMMAND d3d12_warp_smoke)
  set_tests_properties(d3d12_warp_queue_and_fence PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 15)
endif()

if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  add_executable(d3d12_warp_compute fixtures/d3d12_warp_compute.cpp)
  target_compile_features(d3d12_warp_compute PRIVATE cxx_std_17)
  target_link_libraries(d3d12_warp_compute PRIVATE d3d12 dxgi d3dcompiler)
  if(MSVC)
    target_compile_options(d3d12_warp_compute PRIVATE /W4 /WX /permissive-)
  endif()
  add_test(NAME d3d12_warp_compute_readback COMMAND d3d12_warp_compute)
  set_tests_properties(d3d12_warp_compute_readback PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 20)
endif()

if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  add_executable(win_memory_alias fixtures/win_memory_alias.cpp)
  target_compile_features(win_memory_alias PRIVATE cxx_std_17)
  if(MSVC)
    target_compile_options(win_memory_alias PRIVATE /W4 /WX /permissive-)
  endif()
  add_test(NAME native_memory_reserve_alias COMMAND win_memory_alias)
  set_tests_properties(native_memory_reserve_alias PROPERTIES TIMEOUT 15)
endif()

if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  add_test(NAME pe32plus_compiled_dll_exports
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/check_compiled_pe_exports.py"
    "$<TARGET_FILE:pe_intake>" "$<TARGET_FILE:scarlett_abi_fixture>")
endif()

add_executable(physical_page_cpu_model fixtures/physical_page_cpu_model.cpp)
target_compile_features(physical_page_cpu_model PRIVATE cxx_std_17)
if(MSVC)
  target_compile_options(physical_page_cpu_model PRIVATE /W4 /WX /permissive-)
else()
  target_compile_options(physical_page_cpu_model PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME physical_page_cpu_reference COMMAND physical_page_cpu_model)

if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  if(DEFINED ENV{SCARLETT_DXC_PATH} AND EXISTS "$ENV{SCARLETT_DXC_PATH}")
    set(SCARLETT_DXC "$ENV{SCARLETT_DXC_PATH}")
  else()
    find_program(SCARLETT_DXC NAMES dxc.exe dxc)
  endif()
  if(SCARLETT_DXC)
    set(SM6_SHADER "${CMAKE_CURRENT_BINARY_DIR}/wave_sm6.dxil")
    add_custom_command(OUTPUT "${SM6_SHADER}"
      COMMAND "${SCARLETT_DXC}" -T cs_6_0 -E main -O2 -Fo "${SM6_SHADER}"
        "${CMAKE_SOURCE_DIR}/fixtures/wave_lanes_sm6.hlsl"
      DEPENDS "${CMAKE_SOURCE_DIR}/fixtures/wave_lanes_sm6.hlsl"
      VERBATIM)
    add_custom_target(scarlett_sm6_shader ALL DEPENDS "${SM6_SHADER}")
    add_executable(d3d12_wave_sm6 fixtures/d3d12_wave_sm6.cpp)
    target_compile_features(d3d12_wave_sm6 PRIVATE cxx_std_17)
    target_link_libraries(d3d12_wave_sm6 PRIVATE d3d12 dxgi)
    add_dependencies(d3d12_wave_sm6 scarlett_sm6_shader)
    if(MSVC)
      target_compile_options(d3d12_wave_sm6 PRIVATE /W4 /WX /permissive-)
    endif()
    add_test(NAME d3d12_sm6_warp_wave_differential COMMAND d3d12_wave_sm6 "${SM6_SHADER}" warp)
    add_test(NAME d3d12_sm6_hardware_wave_differential COMMAND d3d12_wave_sm6 "${SM6_SHADER}" hardware)
    set_tests_properties(d3d12_sm6_warp_wave_differential d3d12_sm6_hardware_wave_differential
      PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 25)
  else()
    message(STATUS "Windows SDK DXC unavailable: optional SM6 GPU wave test omitted")
  endif()
endif()

if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  add_executable(msvc_struct_return_abi fixtures/msvc_struct_return_abi.cpp)
  target_compile_features(msvc_struct_return_abi PRIVATE cxx_std_17)
  if(MSVC)
    target_compile_options(msvc_struct_return_abi PRIVATE /W4 /WX /permissive-)
  endif()
  add_test(NAME native_x64_struct_return_vtable COMMAND msvc_struct_return_abi)
  set_tests_properties(native_x64_struct_return_vtable PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 15)
endif()

add_executable(command_record_cpu_model fixtures/command_record_cpu_model.cpp)
target_compile_features(command_record_cpu_model PRIVATE cxx_std_17)
if(MSVC)
  target_compile_options(command_record_cpu_model PRIVATE /W4 /WX /permissive-)
else()
  target_compile_options(command_record_cpu_model PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME synthetic_command_record_contract COMMAND command_record_cpu_model)

# Pure Windows/Linux feature negotiation; no external technology DLLs included.
add_library(pc_feature_policy STATIC src/pc_feature_policy.cpp)
target_include_directories(pc_feature_policy PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_compile_features(pc_feature_policy PUBLIC cxx_std_17)
add_executable(pc_feature_policy_tests fixtures/pc_feature_policy_tests.cpp)
target_link_libraries(pc_feature_policy_tests PRIVATE pc_feature_policy)
add_executable(native_pc_host_inventory src/native_pc_host_inventory.cpp)
target_compile_features(native_pc_host_inventory PRIVATE cxx_std_17)
if(CMAKE_DL_LIBS)
  target_link_libraries(native_pc_host_inventory PRIVATE "${CMAKE_DL_LIBS}")
endif()
foreach(target pc_feature_policy pc_feature_policy_tests native_pc_host_inventory)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
  endif()
endforeach()
add_test(NAME native_pc_feature_policy_scenarios COMMAND pc_feature_policy_tests)
add_test(NAME native_pc_host_inventory_selftest COMMAND native_pc_host_inventory --self-test)
add_test(NAME native_pc_host_inventory_smoke COMMAND native_pc_host_inventory)

# Original source-only 1080p native 60 FPS *goal* and adjustable quality
# settings policy; no game hooks, frame-generation provider or game assets.
add_library(native_1080p60_profile STATIC src/native_1080p60_profile.cpp)
target_link_libraries(native_1080p60_profile PUBLIC pc_feature_policy)
target_include_directories(native_1080p60_profile PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_compile_features(native_1080p60_profile PUBLIC cxx_std_17)
add_executable(native_1080p60_profile_tests fixtures/native_1080p60_profile_tests.cpp)
target_link_libraries(native_1080p60_profile_tests PRIVATE native_1080p60_profile)
foreach(target native_1080p60_profile native_1080p60_profile_tests)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
  endif()
endforeach()
add_test(NAME native_pc_1080p60_no_fg_quality_policy COMMAND native_1080p60_profile_tests)

# Native host x86-64 ISA checks. The baseline library never executes advanced
# SIMD instructions; AVX2 eligibility requires CPUID and OS XCR0 YMM state.
add_library(native_cpu_dispatch STATIC src/native_cpu_dispatch.cpp)
target_include_directories(native_cpu_dispatch PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_compile_features(native_cpu_dispatch PUBLIC cxx_std_17)
add_executable(native_cpu_dispatch_tests fixtures/native_cpu_dispatch_tests.cpp)
target_link_libraries(native_cpu_dispatch_tests PRIVATE native_cpu_dispatch)
foreach(target native_cpu_dispatch native_cpu_dispatch_tests)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
  endif()
endforeach()
add_test(NAME native_pc_x64_cpu_isa_safe_dispatch COMMAND native_cpu_dispatch_tests)

# Real source-built native Windows/Linux portable asset I/O fallback.
find_package(Threads REQUIRED)
add_library(native_asset_stream STATIC src/native_asset_stream.cpp)
target_include_directories(native_asset_stream PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_compile_features(native_asset_stream PUBLIC cxx_std_17)
target_link_libraries(native_asset_stream PUBLIC Threads::Threads)
add_executable(native_asset_stream_tests fixtures/native_asset_stream_tests.cpp)
target_link_libraries(native_asset_stream_tests PRIVATE native_asset_stream)
foreach(target native_asset_stream native_asset_stream_tests)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
  endif()
endforeach()
add_test(NAME native_asset_stream_exact_reads_and_errors COMMAND native_asset_stream_tests)
set_tests_properties(native_asset_stream_exact_reads_and_errors PROPERTIES TIMEOUT 20)
add_executable(native_asset_stream_budget_tests fixtures/native_asset_stream_budget_tests.cpp)
target_link_libraries(native_asset_stream_budget_tests PRIVATE native_asset_stream)
if(MSVC)
  target_compile_options(native_asset_stream_budget_tests PRIVATE /W4 /WX /permissive-)
else()
  target_compile_options(native_asset_stream_budget_tests PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME native_asset_stream_byte_budget COMMAND native_asset_stream_budget_tests)
set_tests_properties(native_asset_stream_byte_budget PROPERTIES TIMEOUT 20)
add_executable(native_asset_stream_reserve_tests fixtures/native_asset_stream_reserve_tests.cpp)
target_link_libraries(native_asset_stream_reserve_tests PRIVATE native_asset_stream)
if(MSVC)
  target_compile_options(native_asset_stream_reserve_tests PRIVATE /W4 /WX /permissive-)
else()
  target_compile_options(native_asset_stream_reserve_tests PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME native_asset_stream_critical_reserve COMMAND native_asset_stream_reserve_tests)
set_tests_properties(native_asset_stream_critical_reserve PROPERTIES TIMEOUT 20)
add_library(native_asset_cache STATIC src/native_asset_cache.cpp)
target_link_libraries(native_asset_cache PUBLIC native_asset_stream)
add_executable(native_asset_cache_tests fixtures/native_asset_cache_tests.cpp)
target_link_libraries(native_asset_cache_tests PRIVATE native_asset_cache)
foreach(target native_asset_cache native_asset_cache_tests)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
  endif()
endforeach()
add_test(NAME native_asset_cache_lru COMMAND native_asset_cache_tests)
set_tests_properties(native_asset_cache_lru PROPERTIES TIMEOUT 20)
add_library(native_asset_singleflight STATIC src/native_asset_singleflight.cpp)
target_link_libraries(native_asset_singleflight PUBLIC native_asset_cache)
add_executable(native_asset_singleflight_tests fixtures/native_asset_singleflight_tests.cpp)
target_link_libraries(native_asset_singleflight_tests PRIVATE native_asset_singleflight)
foreach(target native_asset_singleflight native_asset_singleflight_tests)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
  endif()
endforeach()
add_test(NAME native_asset_singleflight_coalescing COMMAND native_asset_singleflight_tests)
set_tests_properties(native_asset_singleflight_coalescing PROPERTIES TIMEOUT 20)
add_executable(native_asset_live_budget_tests fixtures/native_asset_live_budget_tests.cpp)
target_link_libraries(native_asset_live_budget_tests PRIVATE native_asset_singleflight)
if(MSVC)
  target_compile_options(native_asset_live_budget_tests PRIVATE /W4 /WX /permissive-)
else()
  target_compile_options(native_asset_live_budget_tests PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME native_asset_live_budget COMMAND native_asset_live_budget_tests)
set_tests_properties(native_asset_live_budget PROPERTIES TIMEOUT 20)
add_executable(native_asset_pre_admission_tests fixtures/native_asset_pre_admission_tests.cpp)
target_link_libraries(native_asset_pre_admission_tests PRIVATE native_asset_singleflight)
if(MSVC)
  target_compile_options(native_asset_pre_admission_tests PRIVATE /W4 /WX /permissive-)
else()
  target_compile_options(native_asset_pre_admission_tests PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME native_asset_pre_admission COMMAND native_asset_pre_admission_tests)
set_tests_properties(native_asset_pre_admission PROPERTIES TIMEOUT 20)
add_library(native_texture_upload STATIC src/native_texture_upload.cpp)
target_include_directories(native_texture_upload PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(native_texture_upload PUBLIC cxx_std_17)
add_executable(native_texture_upload_tests fixtures/native_texture_upload_tests.cpp)
target_link_libraries(native_texture_upload_tests PRIVATE native_texture_upload)
foreach(target native_texture_upload native_texture_upload_tests)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
  endif()
endforeach()
add_test(NAME native_texture_upload_layout COMMAND native_texture_upload_tests)
set_tests_properties(native_texture_upload_layout PROPERTIES TIMEOUT 30)

add_library(native_upload_arena STATIC src/native_upload_arena.cpp)
target_link_libraries(native_upload_arena PUBLIC native_texture_upload)
add_executable(native_upload_arena_tests fixtures/native_upload_arena_tests.cpp)
target_link_libraries(native_upload_arena_tests PRIVATE native_upload_arena)
foreach(target native_upload_arena native_upload_arena_tests)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
  endif()
endforeach()
add_test(NAME native_upload_arena_fence_lifetime COMMAND native_upload_arena_tests)
set_tests_properties(native_upload_arena_fence_lifetime PROPERTIES TIMEOUT 30)

if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  add_executable(d3d12_native_upload_roundtrip fixtures/d3d12_native_upload_roundtrip.cpp)
  target_link_libraries(d3d12_native_upload_roundtrip PRIVATE native_upload_arena d3d12 dxgi)
  target_compile_features(d3d12_native_upload_roundtrip PRIVATE cxx_std_17)
  if(MSVC)
    target_compile_options(d3d12_native_upload_roundtrip PRIVATE /W4 /WX /permissive-)
  endif()
  add_test(NAME native_win_d3d12_upload_roundtrip_warp COMMAND d3d12_native_upload_roundtrip warp)
  set_tests_properties(native_win_d3d12_upload_roundtrip_warp PROPERTIES TIMEOUT 40 SKIP_RETURN_CODE 77)
endif()



option(NATIVEX_ENABLE_ZSTD "Enable optional system Zstandard decoder" OFF)
if(NATIVEX_ENABLE_ZSTD)
  find_path(NATIVEX_ZSTD_INCLUDE_DIR NAMES zstd.h)
  find_library(NATIVEX_ZSTD_LIBRARY NAMES zstd libzstd zstd_static libzstd_static)
  if(NOT NATIVEX_ZSTD_INCLUDE_DIR OR NOT NATIVEX_ZSTD_LIBRARY)
    message(FATAL_ERROR "Install libzstd development files (1.4.0+) or disable NATIVEX_ENABLE_ZSTD")
  endif()
  add_library(native_asset_decode STATIC src/native_asset_decode.cpp)
  target_include_directories(native_asset_decode PRIVATE "${NATIVEX_ZSTD_INCLUDE_DIR}")
  target_link_libraries(native_asset_decode PUBLIC native_asset_stream PRIVATE "${NATIVEX_ZSTD_LIBRARY}")
  add_executable(native_asset_decode_tests fixtures/native_asset_decode_tests.cpp)
  target_include_directories(native_asset_decode_tests PRIVATE "${NATIVEX_ZSTD_INCLUDE_DIR}")
  target_link_libraries(native_asset_decode_tests PRIVATE native_asset_decode native_asset_singleflight "${NATIVEX_ZSTD_LIBRARY}")
  foreach(target native_asset_decode native_asset_decode_tests)
    if(MSVC)
      target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
    else()
      target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
    endif()
  endforeach()
  add_test(NAME native_asset_bounded_zstd COMMAND native_asset_decode_tests)
  set_tests_properties(native_asset_bounded_zstd PROPERTIES TIMEOUT 30)
  add_library(native_asset_decode_budget STATIC src/native_asset_decode_budget.cpp)
  target_link_libraries(native_asset_decode_budget PUBLIC native_asset_decode)
  add_executable(native_asset_decode_budget_tests fixtures/native_asset_decode_budget_tests.cpp)
  target_include_directories(native_asset_decode_budget_tests PRIVATE "${NATIVEX_ZSTD_INCLUDE_DIR}")
  target_link_libraries(native_asset_decode_budget_tests PRIVATE native_asset_decode_budget native_asset_singleflight native_texture_upload "${NATIVEX_ZSTD_LIBRARY}")
  foreach(target native_asset_decode_budget native_asset_decode_budget_tests)
    if(MSVC)
      target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
    else()
      target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
    endif()
  endforeach()
  add_test(NAME native_asset_shared_decode_budget COMMAND native_asset_decode_budget_tests)
  set_tests_properties(native_asset_shared_decode_budget PROPERTIES TIMEOUT 30)

endif()








# Read-only, offline synthetic asset override planner. Never rewrites originals.
# File/manifest compatibility with the actual game has not been tested.
add_library(native_asset_overlay STATIC src/native_asset_overlay.cpp)
target_include_directories(native_asset_overlay PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_compile_features(native_asset_overlay PUBLIC cxx_std_17)
add_executable(native_asset_overlay_tests fixtures/native_asset_overlay_tests.cpp)
target_link_libraries(native_asset_overlay_tests PRIVATE native_asset_overlay native_asset_stream)
foreach(target native_asset_overlay native_asset_overlay_tests)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
  endif()
endforeach()
add_test(NAME native_pc_readonly_asset_override_and_fallback COMMAND native_asset_overlay_tests)
set_tests_properties(native_pc_readonly_asset_override_and_fallback PROPERTIES TIMEOUT 20)

# Real D3D12 rasterization (original VS/PS) and CPU texture readback on WARP.
# This does not exercise a window/swapchain Present or Xbox D3D12.X.
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  add_executable(d3d12_warp_raster_readback fixtures/d3d12_warp_raster_readback.cpp)
  target_compile_features(d3d12_warp_raster_readback PRIVATE cxx_std_17)
  target_link_libraries(d3d12_warp_raster_readback PRIVATE d3d12 dxgi d3dcompiler)
  if(MSVC)
    target_compile_options(d3d12_warp_raster_readback PRIVATE /W4 /WX /permissive-)
  endif()
  add_test(NAME d3d12_warp_raster_readback COMMAND d3d12_warp_raster_readback)
  set_tests_properties(d3d12_warp_raster_readback PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 25)
endif()

# Original allocation-free portable input state/events, no vendor SDK needed.
add_library(native_input_core STATIC src/native_input_core.cpp)
target_include_directories(native_input_core PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_compile_features(native_input_core PUBLIC cxx_std_17)
add_executable(native_input_core_tests fixtures/native_input_core_tests.cpp)
target_link_libraries(native_input_core_tests PRIVATE native_input_core)
foreach(target native_input_core native_input_core_tests)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
  endif()
endforeach()
add_test(NAME native_pc_input_event_state_machine COMMAND native_input_core_tests)

# Source-built fixed-capacity native keyboard/mouse/XInput action mapper.
# Profile is an illustrative remappable PC action scheme, not title-specific bindings.
add_library(native_input_bindings STATIC src/native_input_bindings.cpp)
target_include_directories(native_input_bindings PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(native_input_bindings PUBLIC native_input_core)
target_compile_features(native_input_bindings PUBLIC cxx_std_17)
add_executable(native_input_bindings_tests fixtures/native_input_bindings_tests.cpp)
target_link_libraries(native_input_bindings_tests PRIVATE native_input_bindings)
foreach(target native_input_bindings native_input_bindings_tests)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
  endif()
endforeach()
add_test(NAME native_pc_remappable_keyboard_mouse_gamepad_actions
         COMMAND native_input_bindings_tests)

# Win32 foreground Raw Input + native operating-system XInput 1.4, not a virtual pad.
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  add_executable(native_win_input_probe fixtures/native_win_input_probe.cpp)
  target_link_libraries(native_win_input_probe PRIVATE native_input_core user32 xinput)
  target_compile_features(native_win_input_probe PRIVATE cxx_std_17)
  if(MSVC)
    target_compile_options(native_win_input_probe PRIVATE /W4 /WX /permissive-)
  endif()
  add_test(NAME native_win_foreground_raw_input_xinput COMMAND native_win_input_probe)
  set_tests_properties(native_win_foreground_raw_input_xinput
    PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 15)
endif()

# Offline-compilable Microsoft MIT GameInput v4 API + DualSense USB helper.
# No SDK binary or proprietary runtime is vendored, and no device outputs are sent.
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  add_executable(native_win_gameinput4_probe fixtures/native_win_gameinput4_probe.cpp)
  target_include_directories(native_win_gameinput4_probe PRIVATE
    "${CMAKE_SOURCE_DIR}/third_party/microsoft-gameinput")
  target_compile_features(native_win_gameinput4_probe PRIVATE cxx_std_17)
  if(MSVC)
    target_compile_options(native_win_gameinput4_probe PRIVATE /W4 /WX /permissive-)
  endif()
  add_test(NAME official_gameinput4_dualsense_source_contract COMMAND native_win_gameinput4_probe)
  set_tests_properties(official_gameinput4_dualsense_source_contract PROPERTIES TIMEOUT 15)
endif()

# Native Windows x64 Win32 IO completion port and real temporary-file I/O.
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  add_executable(native_win_iocp_disk fixtures/native_win_iocp_disk.cpp)
  target_compile_features(native_win_iocp_disk PRIVATE cxx_std_17)
  if(MSVC)
    target_compile_options(native_win_iocp_disk PRIVATE /W4 /WX /permissive-)
  endif()
  add_test(NAME native_win_iocp_overlapped_disk_readback COMMAND native_win_iocp_disk)
  set_tests_properties(native_win_iocp_overlapped_disk_readback PROPERTIES TIMEOUT 45)
endif()

# Bounded portable stereo PCM reference mixer and deterministic tests.
add_library(native_audio_mix STATIC src/native_audio_mix.cpp)
target_include_directories(native_audio_mix PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_compile_features(native_audio_mix PUBLIC cxx_std_17)
add_executable(native_audio_mix_tests fixtures/native_audio_mix_tests.cpp)
target_link_libraries(native_audio_mix_tests PRIVATE native_audio_mix)
foreach(target native_audio_mix native_audio_mix_tests)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
  endif()
endforeach()
add_test(NAME native_pc_pcm_stereo_reference COMMAND native_audio_mix_tests)

# WASAPI Core Audio COM endpoint check, explicitly SKIP when CI has no audio.
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  add_executable(native_win_wasapi_probe fixtures/native_win_wasapi_probe.cpp)
  target_compile_features(native_win_wasapi_probe PRIVATE cxx_std_17)
  target_link_libraries(native_win_wasapi_probe PRIVATE ole32 uuid)
  if(MSVC)
    target_compile_options(native_win_wasapi_probe PRIVATE /W4 /WX /permissive-)
  endif()
  add_test(NAME native_win_wasapi_shared_mix_format COMMAND native_win_wasapi_probe)
  set_tests_properties(native_win_wasapi_shared_mix_format
    PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 15)
endif()

# Original native Windows/Linux journaled two-slot persistent save snapshot.
# Uses platform flush+replace, never a commercial game's save format.
add_library(native_save_slots STATIC src/native_save_slots.cpp)
target_include_directories(native_save_slots PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_compile_features(native_save_slots PUBLIC cxx_std_17)
add_executable(native_save_slots_tests fixtures/native_save_slots_tests.cpp)
target_link_libraries(native_save_slots_tests PRIVATE native_save_slots)
foreach(target native_save_slots native_save_slots_tests)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
  endif()
endforeach()
add_test(NAME native_pc_save_two_slot_corruption_recovery COMMAND native_save_slots_tests)
set_tests_properties(native_pc_save_two_slot_corruption_recovery PROPERTIES TIMEOUT 25)

# Native Win32 window + DXGI flip-model Present, software WARP only.
# May SKIP on headless CI, never fake successful presentation.
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  add_executable(native_win_dxgi_present fixtures/native_win_dxgi_present.cpp)
  target_compile_features(native_win_dxgi_present PRIVATE cxx_std_17)
  target_link_libraries(native_win_dxgi_present PRIVATE d3d12 dxgi user32)
  if(MSVC)
    target_compile_options(native_win_dxgi_present PRIVATE /W4 /WX /permissive-)
  endif()
  add_test(NAME native_win_d3d12_dxgi_window_present COMMAND native_win_dxgi_present)
  set_tests_properties(native_win_d3d12_dxgi_window_present
    PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
  add_test(NAME native_win_dxgi_one_frame_latency_waitable
    COMMAND native_win_dxgi_present --frame-latency-one)
  set_tests_properties(native_win_dxgi_one_frame_latency_waitable
    PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
endif()

# Native D3D12 reserved buffer mapped in real 64 KiB tiles, software WARP.
# Not Xbox guest UMA translation; capabilities may skip on headless runners.
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  add_executable(d3d12_warp_tiled64k fixtures/d3d12_warp_tiled64k.cpp)
  target_compile_features(d3d12_warp_tiled64k PRIVATE cxx_std_17)
  target_link_libraries(d3d12_warp_tiled64k PRIVATE d3d12 dxgi)
  if(MSVC)
    target_compile_options(d3d12_warp_tiled64k PRIVATE /W4 /WX /permissive-)
  endif()
  add_test(NAME d3d12_warp_tiled64k_exact_readback COMMAND d3d12_warp_tiled64k)
  set_tests_properties(d3d12_warp_tiled64k_exact_readback
    PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 25)
endif()

# Original fixed-memory Windows QueryPerformanceCounter / Linux CLOCK_MONOTONIC
# phase timeline and deterministic p50/p95/p99 diagnostics. No background services.
add_library(native_frame_timeline STATIC src/native_frame_timeline.cpp)
target_include_directories(native_frame_timeline PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_compile_features(native_frame_timeline PUBLIC cxx_std_17)
add_executable(native_frame_timeline_tests fixtures/native_frame_timeline_tests.cpp)
target_link_libraries(native_frame_timeline_tests PRIVATE native_frame_timeline)
foreach(target native_frame_timeline native_frame_timeline_tests)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
  endif()
endforeach()
add_test(NAME native_pc_frame_phase_percentiles COMMAND native_frame_timeline_tests)

# Native Windows D3D12 WARP tile alias: two 64 KiB virtual pages share
# a single backing 64 KiB heap tile. Explicit capability skip on WARP.
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  add_executable(d3d12_warp_tile_alias fixtures/d3d12_warp_tile_alias.cpp)
  target_compile_features(d3d12_warp_tile_alias PRIVATE cxx_std_17)
  target_link_libraries(d3d12_warp_tile_alias PRIVATE d3d12 dxgi)
  if(MSVC)
    target_compile_options(d3d12_warp_tile_alias PRIVATE /W4 /WX /permissive-)
  endif()
  add_test(NAME d3d12_warp_tile_alias_exact_readback COMMAND d3d12_warp_tile_alias)
  set_tests_properties(d3d12_warp_tile_alias_exact_readback
    PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 25)
endif()

# One integrated Windows native frame: synthetic per-device Raw Input snapshot
# -> simulation phase -> WARP D3D12 clear -> real DXGI Present -> CPU QPC stats.
# No virtual controllers, physical hardware or game-title code.
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  add_executable(native_win_input_to_present fixtures/native_win_input_to_present.cpp)
  target_compile_features(native_win_input_to_present PRIVATE cxx_std_17)
  target_link_libraries(native_win_input_to_present PRIVATE
    native_input_core native_frame_timeline d3d12 dxgi user32)
  if(MSVC)
    target_compile_options(native_win_input_to_present PRIVATE /W4 /WX /permissive-)
  endif()
  add_test(NAME native_win_input_frame_to_d3d12_present
    COMMAND native_win_input_to_present)
  set_tests_properties(native_win_input_frame_to_d3d12_present
    PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 35)
endif()

# Portable zero-allocation keyboard/mouse/gamepad action mapping and deterministic
# versioned configuration snapshots persisted by the existing native save core.
add_library(native_action_bindings STATIC src/native_action_bindings.cpp)
target_include_directories(native_action_bindings PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_compile_features(native_action_bindings PUBLIC cxx_std_17)
add_executable(native_action_bindings_tests fixtures/native_action_bindings_tests.cpp)
target_link_libraries(native_action_bindings_tests PRIVATE
  native_action_bindings native_input_core native_save_slots)
foreach(target native_action_bindings native_action_bindings_tests)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
  endif()
endforeach()
add_test(NAME native_pc_input_action_remap_profile COMMAND native_action_bindings_tests)
set_tests_properties(native_pc_input_action_remap_profile PROPERTIES TIMEOUT 25)

# Original native Win32 named file-mapping / Linux POSIX shm_open shared memory.
# Two CPU virtual views, independent child process, 64 KiB full-byte oracle.
# This is CPU memory only, not Xbox guest UMA or hardware GPU coherence.
add_executable(native_shared_cpu64 fixtures/native_shared_cpu64.cpp)
target_compile_features(native_shared_cpu64 PRIVATE cxx_std_17)
if(MSVC)
  target_compile_options(native_shared_cpu64 PRIVATE /W4 /WX /permissive-)
else()
  target_compile_options(native_shared_cpu64 PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME native_pc_crossprocess_shared_cpu64 COMMAND native_shared_cpu64)
set_tests_properties(native_pc_crossprocess_shared_cpu64 PROPERTIES TIMEOUT 25)

# Native Win32 D3D12 WARP 12-frame flip-chain and ResizeBuffers acceptance.
# Real DXGI Present + fence + one-frame pacing, no human-visible GPU claim.
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  add_executable(native_win_dxgi_multiframe fixtures/native_win_dxgi_multiframe.cpp)
  target_compile_features(native_win_dxgi_multiframe PRIVATE cxx_std_17)
  target_link_libraries(native_win_dxgi_multiframe PRIVATE
    native_frame_timeline d3d12 dxgi user32)
  if(MSVC)
    target_compile_options(native_win_dxgi_multiframe PRIVATE /W4 /WX /permissive-)
  endif()
  add_test(NAME native_win_dxgi_multiframe_resize_warp
    COMMAND native_win_dxgi_multiframe)
  set_tests_properties(native_win_dxgi_multiframe_resize_warp
    PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 45)
endif()

# Native OS loopback-only TCP: 2 independent child processes, 256 KiB exact
# framed request/response, strict checksum, oversize reject before allocation.
# Infrastructure acceptance, NOT gameplay networking or a listening daemon.
add_executable(native_tcp_loopback fixtures/native_tcp_loopback.cpp)
target_compile_features(native_tcp_loopback PRIVATE cxx_std_17)
if(MSVC)
  target_compile_options(native_tcp_loopback PRIVATE /W4 /WX /permissive-)
else()
  target_compile_options(native_tcp_loopback PRIVATE -Wall -Wextra -Werror)
endif()
if(WIN32)
  target_link_libraries(native_tcp_loopback PRIVATE ws2_32)
endif()
add_test(NAME native_pc_loopback_tcp_framed_transport COMMAND native_tcp_loopback)
set_tests_properties(native_pc_loopback_tcp_framed_transport PROPERTIES TIMEOUT 30)

# Original no-heap native Windows/Linux NV12 4:2:0 -> BGRA8 CPU fallback.
# Software surface conversion only; not hardware video bitstream decoding.
add_library(native_nv12_bgra STATIC src/native_nv12_bgra.cpp)
target_include_directories(native_nv12_bgra PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_compile_features(native_nv12_bgra PUBLIC cxx_std_17)
add_executable(native_nv12_bgra_tests fixtures/native_nv12_bgra_tests.cpp)
target_link_libraries(native_nv12_bgra_tests PRIVATE native_nv12_bgra)
foreach(target native_nv12_bgra native_nv12_bgra_tests)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
  endif()
endforeach()
add_test(NAME native_pc_nv12_bgra_surface_conversion COMMAND native_nv12_bgra_tests)

# Pinned dual-licensed MIT bcdec v0.985: full BC7 CPU *reference* only.
# Isolated vendor compilation preserves third-party source and diagnostics.
add_library(native_bcdec_vendor STATIC src/bcdec_implementation.cpp)
target_include_directories(native_bcdec_vendor PUBLIC
  "${CMAKE_SOURCE_DIR}/third_party/iOrange-bcdec")
target_compile_features(native_bcdec_vendor PUBLIC cxx_std_17)

# Native Windows/Linux original BC1 and BC3 CPU compressed-block texture reference.
# Linear block raster with arbitrary rectangular dimensions; not Xbox tile swizzle.
add_library(native_bc_texture STATIC src/native_bc_texture.cpp)
target_include_directories(native_bc_texture PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_compile_features(native_bc_texture PUBLIC cxx_std_17)

target_link_libraries(native_bc_texture PUBLIC native_bcdec_vendor)
add_executable(native_bc_texture_tests fixtures/native_bc_texture_tests.cpp)
target_link_libraries(native_bc_texture_tests PRIVATE native_bc_texture)
foreach(target native_bc_texture native_bc_texture_tests)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
  endif()
endforeach()
add_test(NAME native_pc_bc1_bc3_rectangular_raster_decode
  COMMAND native_bc_texture_tests)
set_tests_properties(native_pc_bc1_bc3_rectangular_raster_decode PROPERTIES TIMEOUT 25)

# Read-only source-built BCn texture pipeline from real native asynchronous
# file IO. Tests are synthetic and do NOT use RPF9/BCPack or the game's GPU.
add_library(native_asset_texture_publish STATIC src/native_asset_texture_publish.cpp)
target_include_directories(native_asset_texture_publish PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(native_asset_texture_publish PUBLIC
  native_asset_stream native_bc_texture)
target_compile_features(native_asset_texture_publish PUBLIC cxx_std_17)
add_executable(native_asset_texture_publish_tests
  fixtures/native_asset_texture_publish_tests.cpp)
target_link_libraries(native_asset_texture_publish_tests PRIVATE
  native_asset_texture_publish)
foreach(target native_asset_texture_publish native_asset_texture_publish_tests)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
  endif()
endforeach()
add_test(NAME native_pc_asset_stream_bc_texture_publication
  COMMAND native_asset_texture_publish_tests)
set_tests_properties(native_pc_asset_stream_bc_texture_publication PROPERTIES TIMEOUT 25)

# Full BC7 (all eight modes) CPU path, pinned iOrange reference with
# existing strict Reader -> RGBA8 publication. Not Xbox texture tiling.
add_executable(native_bc7_all_tests fixtures/native_bc7_all_tests.cpp)
target_link_libraries(native_bc7_all_tests PRIVATE native_asset_texture_publish)
target_compile_features(native_bc7_all_tests PRIVATE cxx_std_17)
if(MSVC)
  target_compile_options(native_bc7_all_tests PRIVATE /W4 /WX /permissive-)
else()
  target_compile_options(native_bc7_all_tests PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME native_pc_bc7_all_cpu_modes_and_atomicity COMMAND native_bc7_all_tests)
set_tests_properties(native_pc_bc7_all_cpu_modes_and_atomicity PROPERTIES TIMEOUT 25)

# Original Win32 D3D12 WARP compressed BC1/BC3 real texture/SRV shader Load
# and exact 16x16 RGBA readback versus our source-tested native CPU decoder.
# Not Xbox texture swizzle or real AMD/NVIDIA/Intel GPU acceptance.
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  add_executable(d3d12_warp_bc_texture fixtures/d3d12_warp_bc_texture.cpp)
  target_compile_features(d3d12_warp_bc_texture PRIVATE cxx_std_17)
  target_link_libraries(d3d12_warp_bc_texture PRIVATE
    native_bc_texture d3d12 dxgi d3dcompiler)
  if(MSVC)
    target_compile_options(d3d12_warp_bc_texture PRIVATE /W4 /WX /permissive-)
  endif()
  add_test(NAME native_win_d3d12_bc1_gpu_cpu_differential
    COMMAND d3d12_warp_bc_texture bc1)
  add_test(NAME native_win_d3d12_bc3_gpu_cpu_differential
    COMMAND d3d12_warp_bc_texture bc3)
  add_test(NAME native_win_d3d12_bc4_gpu_cpu_differential
    COMMAND d3d12_warp_bc_texture bc4)
  add_test(NAME native_win_d3d12_bc5_gpu_cpu_differential
    COMMAND d3d12_warp_bc_texture bc5)
  add_test(NAME native_win_d3d12_bc7_mode6_gpu_cpu_differential
    COMMAND d3d12_warp_bc_texture bc7-mode6)
  # Compare each legal BC7 compression mode with actual D3D12 WARP decoder.
  add_test(NAME native_win_d3d12_bc7_all_mode0_gpu_cpu_differential
    COMMAND d3d12_warp_bc_texture bc7-0)
  add_test(NAME native_win_d3d12_bc7_all_mode1_gpu_cpu_differential
    COMMAND d3d12_warp_bc_texture bc7-1)
  add_test(NAME native_win_d3d12_bc7_all_mode2_gpu_cpu_differential
    COMMAND d3d12_warp_bc_texture bc7-2)
  add_test(NAME native_win_d3d12_bc7_all_mode3_gpu_cpu_differential
    COMMAND d3d12_warp_bc_texture bc7-3)
  add_test(NAME native_win_d3d12_bc7_all_mode4_gpu_cpu_differential
    COMMAND d3d12_warp_bc_texture bc7-4)
  add_test(NAME native_win_d3d12_bc7_all_mode5_gpu_cpu_differential
    COMMAND d3d12_warp_bc_texture bc7-5)
  add_test(NAME native_win_d3d12_bc7_all_mode6_gpu_cpu_differential
    COMMAND d3d12_warp_bc_texture bc7-6)
  add_test(NAME native_win_d3d12_bc7_all_mode7_gpu_cpu_differential
    COMMAND d3d12_warp_bc_texture bc7-7)
  set_tests_properties(native_win_d3d12_bc1_gpu_cpu_differential
    native_win_d3d12_bc3_gpu_cpu_differential
    native_win_d3d12_bc4_gpu_cpu_differential
    native_win_d3d12_bc5_gpu_cpu_differential
    native_win_d3d12_bc7_mode6_gpu_cpu_differential
    native_win_d3d12_bc7_all_mode0_gpu_cpu_differential
    native_win_d3d12_bc7_all_mode1_gpu_cpu_differential
    native_win_d3d12_bc7_all_mode2_gpu_cpu_differential
    native_win_d3d12_bc7_all_mode3_gpu_cpu_differential
    native_win_d3d12_bc7_all_mode4_gpu_cpu_differential
    native_win_d3d12_bc7_all_mode5_gpu_cpu_differential
    native_win_d3d12_bc7_all_mode6_gpu_cpu_differential
    native_win_d3d12_bc7_all_mode7_gpu_cpu_differential
    PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 35)
endif()

# Real Win32 x64 VirtualAlloc2 placeholder splitting and 64 KiB MapViewOfFile3
# backing alias, UnmapViewOfFile2 preserve/remap and negative range checks.
# Native CPU address-space test, NOT Xbox UMA or GPU residency.
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  add_executable(native_win_placeholder64 fixtures/native_win_placeholder64.cpp)
  target_compile_features(native_win_placeholder64 PRIVATE cxx_std_17)
  if(MSVC)
    target_compile_options(native_win_placeholder64 PRIVATE /W4 /WX /permissive-)
  endif()
  add_test(NAME native_win_real_placeholder64_remap COMMAND native_win_placeholder64)
  set_tests_properties(native_win_real_placeholder64_remap
    PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 25)
endif()

# Original x64 Windows dynamic RUNTIME_FUNCTION / .pdata registration.
# Hand-authored host CPU bytes RaiseException and walk a genuine SEH frame.
# W^X memory; no title EXE, CPU emulation or Xbox exception assumptions.
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  add_executable(native_win_dynamic_unwind fixtures/native_win_dynamic_unwind.cpp)
  target_compile_features(native_win_dynamic_unwind PRIVATE cxx_std_17)
  if(MSVC)
    target_compile_options(native_win_dynamic_unwind PRIVATE /W4 /WX /permissive-)
  endif()
  add_test(NAME native_win_seh_registered_x64_unwind
    COMMAND native_win_dynamic_unwind)
  set_tests_properties(native_win_seh_registered_x64_unwind PROPERTIES TIMEOUT 20)
endif()

# Real source-built Win64 LoadLibrary PE TLS callback/thread attach/detach.
# CRT-owned fixture DLL and explicit host OS loader, not console GDK/AnyXboxX.
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  add_library(scarlett_native_tls_fixture SHARED fixtures/native_win_tls_module.cpp)
  target_compile_features(scarlett_native_tls_fixture PRIVATE cxx_std_17)
  add_executable(native_win_tls_loader fixtures/native_win_tls_loader.cpp)
  target_compile_features(native_win_tls_loader PRIVATE cxx_std_17)
  foreach(target scarlett_native_tls_fixture native_win_tls_loader)
    if(MSVC)
      target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
    endif()
  endforeach()
  add_test(NAME native_win_pe_tls_callbacks_thread_lifecycle
    COMMAND native_win_tls_loader "$<TARGET_FILE:scarlett_native_tls_fixture>")
  set_tests_properties(native_win_pe_tls_callbacks_thread_lifecycle
    PROPERTIES TIMEOUT 25)
endif()

# Cross-check PE metadata parser against the ACTUAL same source-built MSVC DLL
# whose Windows LoadLibrary TLS/thread lifecycle is separately executed above.
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  add_test(NAME native_win_compiled_pe_tls_pdata_intake
    COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_SOURCE_DIR}/tests/check_compiled_pe_tls.py"
      "$<TARGET_FILE:pe_intake>" "$<TARGET_FILE:scarlett_native_tls_fixture>")
  set_tests_properties(native_win_compiled_pe_tls_pdata_intake PROPERTIES TIMEOUT 25)
endif()

# Original per-frame native GameInput/XInput axis normalization contract.
# Radial deadzones, outer saturation, cubic curve and inverted axes are
# pure C++17; no virtual gamepad, GPU, haptic device or added smoothing.
add_library(native_gamepad_analog STATIC src/native_gamepad_analog.cpp)
target_include_directories(native_gamepad_analog PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_compile_features(native_gamepad_analog PUBLIC cxx_std_17)
add_executable(native_gamepad_analog_tests fixtures/native_gamepad_analog_tests.cpp)
target_link_libraries(native_gamepad_analog_tests PRIVATE
  native_gamepad_analog native_input_core)
foreach(target native_gamepad_analog native_gamepad_analog_tests)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
  endif()
endforeach()
add_test(NAME native_pc_gamepad_radial_analog_response
  COMMAND native_gamepad_analog_tests)
set_tests_properties(native_pc_gamepad_radial_analog_response PROPERTIES TIMEOUT 20)

# Allocation-free absolute-phase PCM stereo sample-rate conversion reference.
# Not a production-quality polyphase resampler, decoder or audio device.
add_library(native_audio_resample STATIC src/native_audio_resample.cpp)
target_include_directories(native_audio_resample PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_compile_features(native_audio_resample PUBLIC cxx_std_17)
add_executable(native_audio_resample_tests fixtures/native_audio_resample_tests.cpp)
target_link_libraries(native_audio_resample_tests PRIVATE native_audio_resample)
foreach(target native_audio_resample native_audio_resample_tests)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
  endif()
endforeach()
add_test(NAME native_pc_stereo_pcm_rate_conversion COMMAND native_audio_resample_tests)
set_tests_properties(native_pc_stereo_pcm_rate_conversion PROPERTIES TIMEOUT 15)


# Integrated source-built *native* Windows/Linux PC executable.
# One deterministic scenario crosses OS file I/O, BC1 publication, per-device
# input, profile serialization, durable saves and PCM mixing. No proprietary assets,
# title EXE, proprietary SDK, game boot, real GPU output or autosave claims.
add_executable(native_pc_vertical_slice fixtures/native_pc_vertical_slice.cpp)
target_compile_features(native_pc_vertical_slice PRIVATE cxx_std_17)
target_link_libraries(native_pc_vertical_slice PRIVATE
  native_asset_texture_publish native_input_core native_action_bindings
  native_save_slots native_audio_mix)
if(MSVC)
  target_compile_options(native_pc_vertical_slice PRIVATE /W4 /WX /permissive-)
else()
  target_compile_options(native_pc_vertical_slice PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME native_pc_vertical_slice_io_input_audio_save
  COMMAND native_pc_vertical_slice)
set_tests_properties(native_pc_vertical_slice_io_input_audio_save
  PROPERTIES TIMEOUT 25)
