cmake_minimum_required(VERSION 3.20)
project(ScarlettNativePEProbe LANGUAGES C)
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "Only Windows x64 is supported")
endif()
foreach(kind baseline imports gdk_shim)
  add_executable(scarlett_pe_${kind} probe.c)
  if(kind STREQUAL "baseline")
    target_compile_definitions(scarlett_pe_${kind} PRIVATE SCARLETT_PROBE_MODE=0)
  elseif(kind STREQUAL "imports")
    target_compile_definitions(scarlett_pe_${kind} PRIVATE SCARLETT_PROBE_MODE=1)
    target_link_libraries(scarlett_pe_${kind} PRIVATE kernel32)
  else()
    target_compile_definitions(scarlett_pe_${kind} PRIVATE SCARLETT_PROBE_MODE=2)
    target_link_libraries(scarlett_pe_${kind} PRIVATE kernel32)
  endif()
  if(MSVC)
    target_compile_options(scarlett_pe_${kind} PRIVATE /W4 /WX /GS-)
    target_link_options(scarlett_pe_${kind} PRIVATE /ENTRY:ProbeEntry /SUBSYSTEM:CONSOLE /NODEFAULTLIB /INCREMENTAL:NO /DYNAMICBASE)
  endif()
endforeach()

# A fourth original PE32+ program: desktop D3D12 WARP device/queue/fence test.
add_executable(scarlett_pe_d3d12 d3d12_probe.c)
target_link_libraries(scarlett_pe_d3d12 PRIVATE kernel32 dxgi d3d12 dxguid)
if(MSVC)
  target_compile_options(scarlett_pe_d3d12 PRIVATE /W4 /WX /GS-)
  target_link_options(scarlett_pe_d3d12 PRIVATE /ENTRY:ProbeEntry /SUBSYSTEM:CONSOLE /NODEFAULTLIB /INCREMENTAL:NO /DYNAMICBASE)
endif()

# Fifth original CRT-free PE32+ program: Win32 HWND + WARP DXGI swapchain Present.
add_executable(scarlett_pe_dxgi_present present_probe.c)
target_link_libraries(scarlett_pe_dxgi_present PRIVATE kernel32 user32 dxgi d3d12 dxguid)
if(MSVC)
  target_compile_options(scarlett_pe_dxgi_present PRIVATE /W4 /WX /GS-)
  target_link_options(scarlett_pe_dxgi_present PRIVATE
    /ENTRY:ProbeEntry /SUBSYSTEM:CONSOLE /NODEFAULTLIB /INCREMENTAL:NO /DYNAMICBASE)
endif()

# Sixth independent CRT-free optimized Windows PE32+ program: 8KiB .bss
# and .rdata verification, x64 REP MOVSB with zero dynamic CRT dependency.
add_executable(scarlett_pe_crtfree_copy crtfree_copy_probe.c)
if(MSVC)
  target_compile_options(scarlett_pe_crtfree_copy PRIVATE /W4 /WX /GS- /Oi /O2 /Zl)
  target_link_options(scarlett_pe_crtfree_copy PRIVATE
    /ENTRY:ProbeEntry /SUBSYSTEM:CONSOLE /NODEFAULTLIB /INCREMENTAL:NO /DYNAMICBASE)
endif()
