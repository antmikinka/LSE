# CPU reference backend — the correctness oracle and the no-GPU fallback.

add_library(lse_backend_cpu
  cpu_backend.cpp
)
add_library(lse::backend_cpu ALIAS lse_backend_cpu)

target_link_libraries(lse_backend_cpu PUBLIC lse_core lse_backend_registry)

# The registrar is a static initializer in a TU with no referenced symbols;
# without whole-archive the linker drops it and the backend silently vanishes.
lse_force_load(lse_backend_cpu)
target_include_directories(lse_backend_cpu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
lse_apply_common_flags(lse_backend_cpu)
