# HRX backend — the native low-latency path.
#
# This binds the *native* HRX C ABI (hrx_device_t / hrx_buffer_t /
# hrx_stream_dispatch / hrx_executable_load_data). It deliberately does not use
# the HIP compatibility layer HRX also ships (libamdhip64.so); that shim exists
# to run unmodified HIP applications, and going through it would give up the
# low-latency dispatch path that is the reason to be on HRX at all.

add_library(lse_backend_hrx
  hrx_backend.cpp
  sq_profiler.cpp
  code_object.cpp
  code_census.cpp
  hipc/hip_emitter.cpp
  hipc/phase_emit.cpp
  hipc/hip_sources.cpp
  hipc/comgr_compiler.cpp
  loomc/loomc_compiler.cpp
  loomc/loom_types.cpp
  loomc/loom_sources.cpp
  loomc/loom_print.cpp
  loomc/loom_emitter.cpp
  trace.cpp
  hipc/hip_types.cpp
  kernels/dist_codec.cpp
  kernels/probe.cpp
  kernels/probe_loom.cpp
)
add_library(lse::backend_hrx ALIAS lse_backend_hrx)

target_link_libraries(lse_backend_hrx PUBLIC ${CMAKE_DL_LIBS} lse_core lse_backend_registry lse_graph lse_kernels lse_dist lse_probe lse_trace)
target_include_directories(lse_backend_hrx SYSTEM PUBLIC ${LSE_HRX_INCLUDE_DIR})
target_include_directories(lse_backend_hrx PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# The root discovery looks for a Linux ROCm tree. On Windows the compiler
# is the ROCm 7.1 amd_comgr hipwin already uses, not a System32 copy. One
# definition of these objects has to serve both backends, so this target
# compiles them with that compiler found.
if(WIN32 AND NOT LSE_HAVE_COMGR)
  set(_lse_hip_root "$ENV{HIP_PATH}")
  if(NOT _lse_hip_root)
    set(_lse_hip_root "C:/Program Files/AMD/ROCm/7.1")
  endif()
  file(TO_CMAKE_PATH "${_lse_hip_root}" _lse_hip_root)
  find_library(LSE_WIN_HRX_COMGR NAMES amd_comgr0701 amd_comgr_3
    HINTS "${_lse_hip_root}/lib"
    NO_DEFAULT_PATH)
  find_path(LSE_WIN_HRX_COMGR_INCLUDE NAMES amd_comgr/amd_comgr.h
    HINTS "${_lse_hip_root}/include"
    NO_DEFAULT_PATH)
  set(_lse_win_clang "${_lse_hip_root}/lib/clang/21/include")
  if(NOT EXISTS "${_lse_win_clang}")
    file(GLOB _lse_win_clang_dirs LIST_DIRECTORIES true
         "${_lse_hip_root}/lib/clang/*/include")
    if(_lse_win_clang_dirs)
      list(GET _lse_win_clang_dirs 0 _lse_win_clang)
    endif()
  endif()
  if(LSE_WIN_HRX_COMGR AND LSE_WIN_HRX_COMGR_INCLUDE AND EXISTS "${_lse_win_clang}")
    # A /D path splits on the space in "Program Files". The header is the
    # same channel hipwin uses.
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/hipwin_comgr_paths.h"
"#pragma once
#define LSE_ROCM_INCLUDE_DIR \"${_lse_hip_root}/include\"
#define LSE_CLANG_RESOURCE_DIR \"${_lse_win_clang}\"
")
    set(LSE_HAVE_COMGR ON)
    set(LSE_HAVE_COMGR ON PARENT_SCOPE)
    set(LSE_COMGR_LIBRARY "${LSE_WIN_HRX_COMGR}")
    set(LSE_COMGR_INCLUDE_DIR "${LSE_WIN_HRX_COMGR_INCLUDE}")
    set(LSE_HIP_INCLUDE_DIR "${_lse_hip_root}/include")
    set(_lse_clang_res_dir "${_lse_win_clang}")
    target_include_directories(lse_backend_hrx PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    target_compile_definitions(lse_backend_hrx PRIVATE LSE_HIPWIN_COMGR_PATHS=1)
    message(STATUS "  clang resource  : ${_lse_win_clang}")
  endif()
endif()

if(LSE_HAVE_COMGR)
  target_compile_definitions(lse_backend_hrx PUBLIC LSE_HAVE_COMGR=1)
  # comgr does not know where the HIP headers live; the discovered include dir
  # is baked in and can be overridden at runtime with LSE_ROCM_INCLUDE.
  # comgr's bundled clang does not resolve its own resource headers here, so
  # the resource dir is discovered and passed explicitly.
  if(NOT WIN32)
    file(GLOB _lse_clang_res LIST_DIRECTORIES true
         "${LSE_ROCM_PATH}/core-*/lib/llvm/lib/clang/*/include"
         "${LSE_ROCM_PATH}/lib/llvm/lib/clang/*/include")
    list(GET _lse_clang_res 0 _lse_clang_res_dir)
    # A Windows path with spaces is the generated header above, not a /D flag.
    target_compile_definitions(lse_backend_hrx PRIVATE
      LSE_ROCM_INCLUDE_DIR="${LSE_HIP_INCLUDE_DIR}"
      LSE_CLANG_RESOURCE_DIR="${_lse_clang_res_dir}")
    message(STATUS "  clang resource  : ${_lse_clang_res_dir}")
  endif()
  target_include_directories(lse_backend_hrx SYSTEM PUBLIC ${LSE_COMGR_INCLUDE_DIR})
  target_link_libraries(lse_backend_hrx PUBLIC ${LSE_COMGR_LIBRARY})
else()
  target_compile_definitions(lse_backend_hrx PUBLIC LSE_HAVE_COMGR=0)
endif()

if(LSE_HAVE_LOOMC)
  target_compile_definitions(lse_backend_hrx PUBLIC LSE_HAVE_LOOMC=1)
  # SYSTEM so loomc's headers are not held to this project's warning flags, and
  # the installed library rather than a rebuild: libloomc exports only loomc_*,
  # statically absorbs its own IREE and pulls in no LLVM, ROCm or HSA, so it
  # sits beside libhrx with nothing to reconcile.
  target_include_directories(lse_backend_hrx SYSTEM PUBLIC ${LSE_LOOMC_INCLUDE_DIR})
  target_link_libraries(lse_backend_hrx PUBLIC ${LSE_LOOMC_LIBRARY})
  target_compile_definitions(lse_backend_hrx PUBLIC LOOMC_USING_SHARED_LIBRARY)
  # Supplemental package version; runtime identity resolves the loaded image
  # and hashes its contents once, including loader-selected overrides.
  target_compile_definitions(lse_backend_hrx PRIVATE
    LSE_LOOMC_VERSION="${LSE_LOOMC_VERSION}")
else()
  target_compile_definitions(lse_backend_hrx PUBLIC LSE_HAVE_LOOMC=0)
endif()

if(LSE_HRX_LIBRARY)
  target_link_libraries(lse_backend_hrx PUBLIC ${LSE_HRX_LIBRARY})
  target_compile_definitions(lse_backend_hrx PUBLIC LSE_HRX_LINKED=1)
else()
  # Headers are available but libhrx has not been built yet. The backend still
  # compiles (so the API stays honest and type-checked against the real header)
  # but registers itself as unavailable at runtime.
  target_compile_definitions(lse_backend_hrx PUBLIC LSE_HRX_LINKED=0)
endif()

# See the cpu backend: self-registration requires whole-archive linking.
lse_force_load(lse_backend_hrx)

lse_apply_common_flags(lse_backend_hrx)

# Device power through the mac_linuxgpu HSA runtime's mac_hsa_agent_* calls.
# Where the runtime is loaded at run time they are looked up with dlsym and
# are optional; a statically linked runtime (iOS) names them at link time,
# so the build enables them only when that runtime exports them
# (scripts/ios/build-ios.sh decides).
option(LSE_HSA_POWER "Link the HSA runtime's device power calls (static runtimes)" OFF)
if(LSE_HSA_POWER)
  target_compile_definitions(lse_backend_hrx PRIVATE LSE_HSA_POWER=1)
endif()

# One engine-compiled kernel through HrxBackend. This is the load and launch
# path a generate uses, not the hand-written libhrx smoke.
if(WIN32 AND LSE_HRX_LIBRARY AND LSE_HAVE_COMGR)
  add_executable(hrx-compile-smoke hrx_compile_smoke.cpp)
  target_link_libraries(hrx-compile-smoke PRIVATE lse_backend_hrx)
  lse_apply_common_flags(hrx-compile-smoke)
  get_filename_component(_lse_hrx_dll_dir "${LSE_HRX_LIBRARY}" DIRECTORY)
  set(_lse_hrx_comgr_dll "C:/Program Files/AMD/ROCm/7.1/bin/amd_comgr_3.dll")
  add_custom_command(TARGET hrx-compile-smoke POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_lse_hrx_dll_dir}/hrx.dll"
            "$<TARGET_FILE_DIR:hrx-compile-smoke>/hrx.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/src/backends/hipwin/hsa-runtime64.dll"
            "$<TARGET_FILE_DIR:hrx-compile-smoke>/hsa-runtime64.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_lse_hrx_comgr_dll}"
            "$<TARGET_FILE_DIR:hrx-compile-smoke>/amd_comgr_3.dll"
    VERBATIM)
endif()
