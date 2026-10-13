#pragma once

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace lse::backend::detail {
// Acquire a reference to an existing image only. Darwin's RTLD_NOLOAD lookup
// need not resolve the search-path alias used to load a versioned dylib. A
// globally visible anchor identifies its actual resident path without loading
// another runtime or bypassing an already-installed interposer.
//
// On Windows the same contract is: a module already in the process (the
// preload of hsa-runtime64.dll, or libhrx's own load) is pinned with one
// extra reference. A name that is not loaded returns null and does not search
// PATH. close_library releases that one reference.
inline void *open_loaded_library(const char *name,
                                 const char *anchor) noexcept {
#if defined(_WIN32)
  (void)anchor;
  if (name == nullptr || name[0] == '\0') return nullptr;
  if (GetModuleHandleA(name) == nullptr) return nullptr;
  return reinterpret_cast<void *>(LoadLibraryA(name));
#else
  if (void *library = dlopen(name, RTLD_LAZY | RTLD_NOLOAD))
    return library;
#if defined(__APPLE__)
  Dl_info image{};
  void *symbol = dlsym(RTLD_DEFAULT, anchor);
  if (symbol != nullptr && dladdr(symbol, &image) != 0 &&
      image.dli_fname != nullptr)
    return dlopen(image.dli_fname, RTLD_LAZY | RTLD_NOLOAD);
#else
  (void)anchor;
#endif
  return nullptr;
#endif
}

// Load by name when the caller asked for a runtime that is not resident yet.
// Windows searches the executable directory before PATH, which is where the
// build copies hsa-runtime64.dll.
inline void *open_library(const char *name) noexcept {
  if (name == nullptr || name[0] == '\0') return nullptr;
#if defined(_WIN32)
  return reinterpret_cast<void *>(LoadLibraryA(name));
#else
  return dlopen(name, RTLD_LAZY);
#endif
}

inline void *library_symbol(void *library, const char *name) noexcept {
  if (library == nullptr || name == nullptr) return nullptr;
#if defined(_WIN32)
  return reinterpret_cast<void *>(
      GetProcAddress(static_cast<HMODULE>(library), name));
#else
  return dlsym(library, name);
#endif
}

inline void close_library(void *library) noexcept {
  if (library == nullptr) return;
#if defined(_WIN32)
  FreeLibrary(static_cast<HMODULE>(library));
#else
  dlclose(library);
#endif
}
} // namespace lse::backend::detail
