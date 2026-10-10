# Force-load a static archive into every consumer that links it.
#
# The engine's transports, primitives, model architectures and backends
# self-register from TUs that no other TU references, so a linker doing
# symbol-driven member extraction drops them unless the whole archive is
# forced in. Every platform spells that differently:
#
#   Apple ld64   -force_load <archive>
#   GNU ld/lld   --whole-archive <archive> --no-whole-archive
#   MSVC/lld-link  /WHOLEARCHIVE:<archive>
#
# CMake's LINKER: prefix hands each item to the linker driver as-is; lld-link
# rejects the GNU spelling, and clang-cl's MSVC mode does not understand
# `-force_load`, so the WIN32 branch uses the MSVC spelling. The INTERFACE
# keyword mirrors the historical call sites: the requirement belongs to the
# archive, and every executable that links it inherits the flag.
function(lse_force_load target)
  if(APPLE)
    target_link_options(${target} INTERFACE
      "LINKER:-force_load,$<TARGET_FILE:${target}>")
  elseif(WIN32)
    target_link_options(${target} INTERFACE
      "LINKER:/WHOLEARCHIVE:$<TARGET_FILE:${target}>")
  else()
    target_link_options(${target} INTERFACE
      "LINKER:--whole-archive,$<TARGET_FILE:${target}>,--no-whole-archive")
  endif()
endfunction()
