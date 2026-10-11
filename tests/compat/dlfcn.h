// Shim: satisfies `#include <dlfcn.h>` on Windows — specifically the
// Dl_info/dladdr pair used by include/lse/backends/hrx/
// compiler_image_identity.hpp (pulled in by test_compiler_identity).
//
// dladdr maps to GetModuleHandleExW(FROM_ADDRESS): dli_fname becomes the
// module's UTF-8 image path and dli_fbase the HMODULE load base.
// Delta: dli_sname/dli_saddr are always null (no exported-symbol lookup —
// the only consumer asks for the image, never the symbol).
// Delta: a thread-local path buffer backs dli_fname; it stays valid until
// the next dladdr call on the same thread (POSIX leaves lifetime
// unspecified too).
// dlopen/dlsym/dlclose/dlerror are deliberately absent: no Windows test
// consumer needs them, and LoadLibrary semantics differ enough that a
// silent shim would mislead.
#pragma once

#include "../compat.hpp"

struct Dl_info {
  const char* dli_fname;
  void* dli_fbase;
  const char* dli_sname;
  void* dli_saddr;
};

inline int dladdr(const void* addr, Dl_info* info) {
  if (addr == nullptr || info == nullptr) return 0;
  HMODULE mod = nullptr;
  if (!::GetModuleHandleExW(
          GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
              GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
          reinterpret_cast<LPCWSTR>(addr), &mod) ||
      mod == nullptr)
    return 0;
  static thread_local wchar_t wpath[MAX_PATH + 1]{};
  static thread_local char path[(MAX_PATH + 1) * 4]{};
  const DWORD n = ::GetModuleFileNameW(mod, wpath, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) return 0;
  if (::WideCharToMultiByte(CP_UTF8, 0, wpath, -1, path, sizeof path, nullptr,
                            nullptr) == 0)
    return 0;
  info->dli_fname = path;
  info->dli_fbase = static_cast<void*>(mod);
  info->dli_sname = nullptr;
  info->dli_saddr = nullptr;
  return 1;
}
