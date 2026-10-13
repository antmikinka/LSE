cmake_minimum_required(VERSION 3.24)
project(libhrx LANGUAGES CXX)

# Windows HRX runtime. It does not link HIP or ROCm. It loads hsa-runtime64.dll
# at runtime. That DLL loads an AMDGPU code object and launches it.

add_library(hrx SHARED src/runtime.cpp)
target_include_directories(hrx PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_definitions(hrx PRIVATE HRX_BUILDING_SHARED)
target_compile_features(hrx PRIVATE cxx_std_20)
set_target_properties(hrx PROPERTIES
  OUTPUT_NAME "hrx"
  PREFIX "")

add_executable(hrx-win-smoke src/smoke.cpp)
target_link_libraries(hrx-win-smoke PRIVATE hrx)
target_compile_features(hrx-win-smoke PRIVATE cxx_std_20)

set(HRX_HSA_RUNTIME_DLL
  "${CMAKE_CURRENT_SOURCE_DIR}/../build-win/hsa-runtime64.dll"
  CACHE FILEPATH "Existing Windows HSA runtime (hsa-runtime64.dll)")

if(EXISTS "${HRX_HSA_RUNTIME_DLL}")
  add_custom_command(TARGET hrx-win-smoke POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${HRX_HSA_RUNTIME_DLL}"
            "$<TARGET_FILE_DIR:hrx-win-smoke>/hsa-runtime64.dll"
    VERBATIM)
endif()
