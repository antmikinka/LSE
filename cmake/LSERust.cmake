# Drives cargo for the fastokens FFI shim and exposes it as an imported target.

find_program(LSE_CARGO cargo)

# fastokens is a path dependency of the shim and lives outside this repo, so a
# fresh clone has nothing to build against and cargo fails with a missing
# directory rather than anything naming the cause. Fetch it at the revision CI
# pins. Nothing here is a fallback: the checkout is the dependency, and a build
# that skipped it would silently drop the tokenizer.
set(LSE_FASTOKENS_REPO "https://github.com/crusoecloud/fastokens.git"
    CACHE STRING "Where to fetch fastokens from")
set(LSE_FASTOKENS_REF "7973014e4f3a6028ac48f305704eacd64d0b4ef6"
    CACHE STRING "fastokens revision to build against")

function(lse_provide_fastokens dir)
  if(EXISTS "${dir}/Cargo.toml")
    return()   # a working copy is already there; leave whatever is checked out
  endif()
  find_package(Git QUIET REQUIRED)
  message(STATUS "fetching fastokens ${LSE_FASTOKENS_REF} into ${dir}")
  get_filename_component(_parent "${dir}" DIRECTORY)
  file(MAKE_DIRECTORY "${_parent}")
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" clone --quiet "${LSE_FASTOKENS_REPO}" "${dir}"
    RESULT_VARIABLE _rc)
  if(NOT _rc EQUAL 0)
    message(FATAL_ERROR
      "could not clone ${LSE_FASTOKENS_REPO} into ${dir}. Clone it by hand, or "
      "point -DLSE_FASTOKENS_REPO at a mirror.")
  endif()
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" -C "${dir}" checkout --quiet "${LSE_FASTOKENS_REF}"
    RESULT_VARIABLE _rc)
  if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "fastokens has no revision ${LSE_FASTOKENS_REF}")
  endif()
endfunction()

# Cross builds name cargo's target triple. iOS links PCRE2 statically (the
# app cannot use a system copy) and pcre2-sys then builds its bundled source
# without the JIT, which iOS forbids.
set(LSE_CARGO_TARGET_ARGS "")
set(LSE_CARGO_TARGET_DIR "release")
set(LSE_CARGO_ENV "")
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  set(LSE_CARGO_TARGET_ARGS --target aarch64-apple-ios)
  set(LSE_CARGO_TARGET_DIR "aarch64-apple-ios/release")
  set(LSE_CARGO_ENV "PCRE2_SYS_STATIC=1"
      "IPHONEOS_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}")
elseif(WIN32)
  # Naming the triple puts the artifact under target/<triple>/release and, on
  # the MSVC host toolchain, emits an MSVC .lib rather than a GNU .a.
  set(LSE_CARGO_TARGET_ARGS --target x86_64-pc-windows-msvc)
  set(LSE_CARGO_TARGET_DIR "x86_64-pc-windows-msvc/release")
endif()

# A Rust staticlib does not carry the native C libraries its crates link
# against (pcre2, ring, ...), so the consumer must link them. Ask rustc for the
# list instead of hardcoding it — the set changes with fastokens' dependencies.
function(_lse_rust_native_libs crate_dir out_var)
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E env ${LSE_CARGO_ENV}
            ${LSE_CARGO} rustc --locked --release --lib ${LSE_CARGO_TARGET_ARGS}
            --manifest-path "${crate_dir}/Cargo.toml"
            -- --print native-static-libs
    OUTPUT_VARIABLE _out ERROR_VARIABLE _err RESULT_VARIABLE _rc)

  set(_libs "")
  if(_rc EQUAL 0 AND "${_out}${_err}" MATCHES "native-static-libs: ([^\n]*)")
    if(WIN32)
      # rustc names them "foo.lib" tokens here, not -lfoo. The real system
      # import libs (kernel32.lib, ws2_32.lib, ...) are on the MSVC linker path,
      # and the driver deduplicates the ones it links by default, so they pass
      # straight through. The `windows` crate is built with raw-dylib, so rustc
      # ALSO lists synthetic import stubs (windows.0.53.0.lib). Those are only
      # materialized during a rustc-driven link; a `cargo build --lib` staticlib
      # never links, so no such file exists for our external linker to open.
      # Each stub forwards to a system DLL that a real import lib in this same
      # list already provides, so drop the stubs and keep the real libs.
      string(REGEX MATCHALL "[A-Za-z0-9_.\\-]+\\.lib" _all "${CMAKE_MATCH_1}")
      foreach(_lib IN LISTS _all)
        if(NOT _lib MATCHES "^windows\\.[0-9]")
          list(APPEND _libs "${_lib}")
        endif()
      endforeach()
    else()
      string(REGEX MATCHALL "-l[^ ]+" _flags "${CMAKE_MATCH_1}")
      foreach(_flag IN LISTS _flags)
        string(REGEX REPLACE "^-l" "" _name "${_flag}")
        # The C runtime and unwinder come in via the C++ driver already.
        if(NOT _name MATCHES "^(c|gcc_s|m)$")
          list(APPEND _libs "${_name}")
        endif()
      endforeach()
    endif()
  else()
    message(WARNING "could not query rustc for native libs; resolving known host dependencies")
    if(WIN32)
      set(_libs ws2_32.lib bcrypt.lib userenv.lib)
      set(${out_var} "${_libs}" PARENT_SCOPE)
      return()
    endif()
    set(_libs pthread dl)
    # Cargo's lock file can make the query unavailable in a read-only source
    # checkout even when the static archive has already been built. fastokens
    # uses the system PCRE2 library; omitting it leaves unresolved pcre2_* C
    # symbols at the final executable link.
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
      pkg_check_modules(LSE_FASTOKENS_PCRE2 QUIET IMPORTED_TARGET libpcre2-8)
      if(LSE_FASTOKENS_PCRE2_FOUND)
        list(APPEND _libs PkgConfig::LSE_FASTOKENS_PCRE2)
      endif()
    endif()
  endif()
  set(${out_var} "${_libs}" PARENT_SCOPE)
endfunction()

function(lse_add_rust_staticlib target crate_dir lib_name)
  if(NOT LSE_CARGO)
    message(STATUS "cargo not found — ${target} disabled")
    return()
  endif()

  if(WIN32)
    set(_lib "${crate_dir}/target/${LSE_CARGO_TARGET_DIR}/${lib_name}.lib")
  else()
    set(_lib "${crate_dir}/target/${LSE_CARGO_TARGET_DIR}/lib${lib_name}.a")
  endif()

  # Depend on the sources so cargo re-runs on change; cargo decides whether a
  # rebuild is actually needed.
  file(GLOB_RECURSE _srcs "${crate_dir}/src/*.rs" "${crate_dir}/Cargo.toml")

  add_custom_command(
    OUTPUT "${_lib}"
    COMMAND ${CMAKE_COMMAND} -E env ${LSE_CARGO_ENV}
            ${LSE_CARGO} build --locked --release ${LSE_CARGO_TARGET_ARGS}
            --manifest-path "${crate_dir}/Cargo.toml"
    DEPENDS ${_srcs}
    COMMENT "cargo build --release (${lib_name})"
    VERBATIM)

  add_custom_target(${target}_build DEPENDS "${_lib}")

  _lse_rust_native_libs("${crate_dir}" _native)
  message(STATUS "${lib_name} native deps: ${_native}")

  add_library(${target} INTERFACE)
  add_dependencies(${target} ${target}_build)
  target_include_directories(${target} INTERFACE "${crate_dir}/include")
  target_link_libraries(${target} INTERFACE "${_lib}" ${_native})
endfunction()
