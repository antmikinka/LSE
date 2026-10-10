# Shared compiler/linker flags. Applied via lse_apply_common_flags(<target>) so
# every target gets the same treatment without repeating the list.

function(lse_apply_common_flags target)
  if(MSVC)
    # clang-cl reinterprets the CL-mode spelling of -Wall as -Weverything; the
    # /clang:- passthrough keeps the GNU meaning (verified on LLVM 21).
    # -Wformat=2 is a GCC spelling Clang does not have; the two flags covering
    # its core replace it so the MSVC-mode build keeps format checking.
    set(_lse_warn_flags
      /clang:-Wall
      -Wextra
      -Wpedantic
      -Wshadow
      -Wconversion
      -Wsign-conversion
      -Wnon-virtual-dtor
      -Wcast-align
      -Wunused
      -Woverloaded-virtual
      -Wnull-dereference
      -Wdouble-promotion
      -Wformat -Wformat-security
    )
  else()
    set(_lse_warn_flags
      -Wall
      -Wextra
      -Wpedantic
      -Wshadow
      -Wconversion
      -Wsign-conversion
      -Wnon-virtual-dtor
      -Wcast-align
      -Wunused
      -Woverloaded-virtual
      -Wnull-dereference
      -Wdouble-promotion
      -Wformat=2
    )
  endif()
  target_compile_options(${target} PRIVATE ${_lse_warn_flags})

  if(LSE_WERROR)
    target_compile_options(${target} PRIVATE -Werror)
  endif()

  # P2996 is off until this flag is set; __cpp_impl_reflection follows it.
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
      AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 16)
    target_compile_options(${target} PRIVATE -freflection)
  endif()

  if(LSE_ASAN)
    if(MSVC)
      # UBSan has no Windows runtime; ASan only under clang-cl.
      target_compile_options(${target} PRIVATE
        /clang:-fsanitize=address -fno-omit-frame-pointer)
      target_link_options(${target} PRIVATE /clang:-fsanitize=address)
    else()
      target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
      target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
  endif()

  # The engine is a numerics project: never let the compiler reassociate float
  # math behind our backs. Reproducibility across backends depends on it.
  if(MSVC)
    # The clang-cl driver silently IGNORES GNU-spelled codegen flags passed
    # bare ("-fno-fast-math" prints "unknown argument ignored" and exits 0).
    # The /clang:- spelling is what actually reaches cc1; verified empirically
    # on LLVM 21. -ffp-contract=off pins FMA contraction the same way the
    # POSIX build's `#pragma clang fp contract(off)` sites do.
    target_compile_options(${target} PRIVATE
      /clang:-fno-fast-math /clang:-ffp-contract=off)
  else()
    target_compile_options(${target} PRIVATE -fno-fast-math)
  endif()

  if(WIN32)
    # UCRT marks the POSIX names this codebase reaches through <io.h> as
    # deprecated; the platform shim uses them deliberately. NOMINMAX keeps
    # windows.h from defining min/max macros over std::min/std::max, and
    # WIN32_LEAN_AND_MEAN keeps it from dragging in winsock.h (the comm layer
    # includes <winsock2.h> itself, and the two headers conflict).
    target_compile_definitions(${target} PRIVATE
      _CRT_NONSTDC_NO_DEPRECATE
      _CRT_SECURE_NO_WARNINGS
      NOMINMAX
      WIN32_LEAN_AND_MEAN)
  endif()

  set_target_properties(${target} PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
  )
endfunction()
