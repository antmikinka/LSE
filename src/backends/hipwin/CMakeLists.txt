# Windows HIP backend and the HSA runtime DLL beside it.
# The HIP language stays off. Device source is compiled at runtime by
# amd_comgr, not by enable_language(HIP).

if(NOT WIN32)
  return()
endif()

set(_lse_hip_root "$ENV{HIP_PATH}")
if(NOT _lse_hip_root)
  set(_lse_hip_root "C:/Program Files/AMD/ROCm/7.1")
endif()
file(TO_CMAKE_PATH "${_lse_hip_root}" _lse_hip_root)

find_path(LSE_HIPWIN_INCLUDE hip/hip_runtime_api.h
  HINTS "${_lse_hip_root}/include"
  NO_DEFAULT_PATH)
find_library(LSE_HIPWIN_AMDHI64 amdhip64
  HINTS "${_lse_hip_root}/lib"
  NO_DEFAULT_PATH)

if(NOT LSE_HIPWIN_INCLUDE OR NOT LSE_HIPWIN_AMDHI64)
  message(STATUS "hipwin: skipped (HIP SDK not found under ${_lse_hip_root}; set HIP_PATH)")
  return()
endif()

message(STATUS "hipwin: ${LSE_HIPWIN_AMDHI64}")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/copy-hsa-dll.cmake" [=[
if(NOT EXISTS "${src}")
  message(FATAL_ERROR "hsa-runtime64 missing: ${src}")
endif()
get_filename_component(_src "${src}" ABSOLUTE)
get_filename_component(_dst "${dst}" ABSOLUTE)
if(_src STREQUAL _dst)
  return()
endif()
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different "${src}" "${dst}"
  RESULT_VARIABLE _rc)
if(_rc)
  message(FATAL_ERROR "copying hsa-runtime64 failed (${_rc})")
endif()
]=])

add_library(hsa-runtime64 SHARED hsa_runtime_win.cpp)
set_target_properties(hsa-runtime64 PROPERTIES
  OUTPUT_NAME "hsa-runtime64"
  PREFIX ""
  CXX_VISIBILITY_PRESET default
  VISIBILITY_INLINES_HIDDEN OFF)
target_include_directories(hsa-runtime64 SYSTEM PRIVATE ${LSE_HIPWIN_INCLUDE})
target_compile_definitions(hsa-runtime64 PRIVATE __HIP_PLATFORM_AMD__)
target_link_libraries(hsa-runtime64 PRIVATE ${LSE_HIPWIN_AMDHI64})
lse_apply_common_flags(hsa-runtime64)
set_target_properties(hsa-runtime64 PROPERTIES
  CXX_VISIBILITY_PRESET default
  VISIBILITY_INLINES_HIDDEN OFF)

find_library(LSE_HIPWIN_COMGR NAMES amd_comgr0701 amd_comgr_3
  HINTS "${_lse_hip_root}/lib"
  NO_DEFAULT_PATH)
find_path(LSE_HIPWIN_COMGR_HEADER amd_comgr/amd_comgr.h
  HINTS "${_lse_hip_root}/include"
  NO_DEFAULT_PATH)

set(_lse_hipwin_clang_res "${_lse_hip_root}/lib/clang/21/include")
if(NOT EXISTS "${_lse_hipwin_clang_res}")
  file(GLOB _lse_hipwin_clang_dirs LIST_DIRECTORIES true
       "${_lse_hip_root}/lib/clang/*/include")
  if(_lse_hipwin_clang_dirs)
    list(GET _lse_hipwin_clang_dirs 0 _lse_hipwin_clang_res)
  else()
    set(_lse_hipwin_clang_res "")
  endif()
endif()

add_library(lse_backend_hipwin hipwin_backend.cpp)
add_library(lse::backend_hipwin ALIAS lse_backend_hipwin)
target_include_directories(lse_backend_hipwin SYSTEM PRIVATE ${LSE_HIPWIN_INCLUDE})
target_compile_definitions(lse_backend_hipwin PRIVATE __HIP_PLATFORM_AMD__)
target_include_directories(lse_backend_hipwin PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(lse_backend_hipwin
  PUBLIC lse_core lse_backend_registry ${LSE_HIPWIN_AMDHI64})

if(LSE_HIPWIN_COMGR AND LSE_HIPWIN_COMGR_HEADER)
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/hipwin_comgr_paths.h"
"#pragma once
#define LSE_ROCM_INCLUDE_DIR \"${_lse_hip_root}/include\"
#define LSE_CLANG_RESOURCE_DIR \"${_lse_hipwin_clang_res}\"
")
  # The HRX backend compiles this one copy. A second copy in this archive
  # is a duplicate symbol under /WHOLEARCHIVE.
  if(NOT TARGET lse_backend_hrx)
    target_sources(lse_backend_hipwin PRIVATE
      ../hrx/hipc/hip_emitter.cpp
      ../hrx/hipc/phase_emit.cpp
      ../hrx/hipc/hip_sources.cpp
      ../hrx/hipc/hip_types.cpp
      ../hrx/hipc/comgr_compiler.cpp
      ../hrx/code_object.cpp
      ../hrx/code_census.cpp)
  endif()
  target_include_directories(lse_backend_hipwin PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
  # PUBLIC so every TU that includes hipwin_backend.hpp sees the same
  # class layout. The comgr macros stay private to the compiler sources.
  target_compile_definitions(lse_backend_hipwin PUBLIC LSE_HIPWIN_COMGR=1)
  target_compile_definitions(lse_backend_hipwin PRIVATE
    LSE_HAVE_COMGR=1
    LSE_HIPWIN_COMGR_PATHS=1)
  target_link_libraries(lse_backend_hipwin PUBLIC ${LSE_HIPWIN_COMGR} lse_graph lse_kernels)
  message(STATUS "hipwin compiler: ${LSE_HIPWIN_COMGR}")
  message(STATUS "  clang resource  : ${_lse_hipwin_clang_res}")
else()
  message(STATUS "hipwin compiler: amd_comgr not found under ${_lse_hip_root}; toolchains stay empty")
endif()

lse_force_load(lse_backend_hipwin)
lse_apply_common_flags(lse_backend_hipwin)

# The import is amd_comgr_3.dll. Windows searches System32 before PATH, and
# an Adrenalin copy there is not the ROCm 7.1 compiler. The executable
# directory is searched first.
set(_lse_hipwin_comgr_dll "${_lse_hip_root}/bin/amd_comgr_3.dll")
set(LSE_HIPWIN_COMGR_DLL "${_lse_hipwin_comgr_dll}" CACHE INTERNAL
    "ROCm amd_comgr_3.dll copied beside Windows executables")

if(LSE_HIPWIN_COMGR AND LSE_HIPWIN_COMGR_HEADER)
  add_executable(hipwin-compile-smoke hipwin_compile_smoke.cpp)
  target_link_libraries(hipwin-compile-smoke PRIVATE lse_backend_hipwin)
  if(TARGET lse_backend_hrx)
    target_link_libraries(hipwin-compile-smoke PRIVATE lse_backend_hrx)
  endif()
  target_compile_definitions(hipwin-compile-smoke PRIVATE __HIP_PLATFORM_AMD__)
  target_include_directories(hipwin-compile-smoke SYSTEM PRIVATE ${LSE_HIPWIN_INCLUDE})
  lse_apply_common_flags(hipwin-compile-smoke)
  if(EXISTS "${_lse_hipwin_comgr_dll}")
    add_custom_command(TARGET hipwin-compile-smoke POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${_lse_hipwin_comgr_dll}"
        "$<TARGET_FILE_DIR:hipwin-compile-smoke>/amd_comgr_3.dll")
  endif()
endif()

add_executable(hsa-win-smoke hsa_win_smoke.cpp)
target_link_libraries(hsa-win-smoke PRIVATE hsa-runtime64)
lse_apply_common_flags(hsa-win-smoke)
add_custom_command(TARGET hsa-win-smoke POST_BUILD
  COMMAND ${CMAKE_COMMAND}
    -Dsrc=$<TARGET_FILE:hsa-runtime64>
    -Ddst=$<TARGET_FILE_DIR:hsa-win-smoke>/$<TARGET_FILE_NAME:hsa-runtime64>
    -P "${CMAKE_CURRENT_BINARY_DIR}/copy-hsa-dll.cmake")
