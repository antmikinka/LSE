// Windows test-compat layer — POSIX spellings the CRT does not export.
//
// Included two ways, both Windows-only:
//   * from harness.hpp under #ifdef _WIN32 (covers tests that call
//     ::setenv/::unsetenv without including <unistd.h>), and
//   * from tests/compat/unistd.h + tests/compat/sys/wait.h, the shim include
//     directory that satisfies literal `#include <unistd.h>` directives in
//     zero-touch test files (the shim dir is added to the include path only
//     when WIN32; on POSIX the real headers win because it is never added).
//
// _CRT_NONSTDC_NO_DEPRECATE (cmake/LSECompilerFlags.cmake) already makes
// <io.h>/<process.h> export POSIX-named read/write/close/pipe/access/getpid,
// so this header supplies only what remains missing. Every semantic delta
// from POSIX is documented at its definition (R8 pattern).
#pragma once

#ifdef _WIN32

#include <direct.h>     // _mkdir
#include <fcntl.h>      // _open, O_RDONLY
#include <io.h>         // read/write/close/access (POSIX names via NONSTDC)
#include <process.h>    // getpid (POSIX name via NONSTDC)
#include <sys/stat.h>   // struct stat, _fstat64
#include <sys/types.h>
// windows.h pulls in the legacy <winsock.h> unless it is lean-and-mean, and
// httplib-using tests (test_hub, test_api_parity) later include <winsock2.h>,
// whose ws2def.h then collides with winsock.h's sockaddr/in_addr. Including
// winsock2 first sets _WINSOCK2API_ so windows.h skips winsock.h entirely.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>

// minwindef.h defines the legacy 16-bit spelling macros `near`/`far` as empty,
// which silently eats identifiers like `void near(...)` (test_prefix_commit)
// and `LinkMember far{...}` (test_probe). No engine or CRT header needs them.
// But `FAR`/`NEAR` are themselves defined in terms of `far`/`near`, and SDK
// headers included after this one (ws2tcpip.h via httplib.h: `FAR *LPIN6_ADDR`)
// still spell FAR — so redefine them empty instead of leaving them dangling.
#ifdef near
#undef near
#endif
#ifdef far
#undef far
#endif
#undef NEAR
#undef FAR
#define NEAR
#define FAR

#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>

// ---- types / constants the UCRT does not define --------------------------

#ifndef _PID_T_DEFINED
#define _PID_T_DEFINED
typedef int pid_t;
#endif

// Delta: POSIX ssize_t is the signed counterpart of size_t; long long matches
// the 64-bit CRT's _read/_write return width.
#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
typedef long long ssize_t;
#endif

#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif
#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif
#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#endif

// Delta: Windows has no close-on-exec flag; handles are never inherited
// across CreateProcess unless explicitly marked, so the bit is a no-op.
#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

// The UCRT ships the _S_IF* mode bits but no POSIX S_IS* test macros.
#ifndef S_ISREG
#define S_ISREG(m) (((m)&_S_IFMT) == _S_IFREG)
#endif
#ifndef S_ISDIR
#define S_ISDIR(m) (((m)&_S_IFMT) == _S_IFDIR)
#endif

// ---- environment ---------------------------------------------------------

// Delta: POSIX setenv honors overwrite==0 (keep the old value); _putenv_s
// always overwrites. Every call site in tests/ passes overwrite==1.
// Delta: returns _putenv_s's 0/errno, not POSIX's 0/-1+errno split callers
// rarely check.
inline int setenv(const char* name, const char* value, int /*overwrite*/) {
  return _putenv_s(name, value);
}

// Delta: _putenv_s(name, "") removes the variable — matches POSIX unsetenv.
inline int unsetenv(const char* name) { return _putenv_s(name, ""); }

// ---- time ----------------------------------------------------------------

// Delta: Sleep() has ~15.6 ms granularity by default; microseconds are
// rounded UP to whole milliseconds (a 1 us sleep may take ~1 ms).
inline int usleep(unsigned long usec) {
  ::Sleep(static_cast<DWORD>((usec + 999) / 1000));
  return 0;
}

inline unsigned sleep(unsigned seconds) {
  ::Sleep(static_cast<DWORD>(seconds) * 1000);
  return 0;
}

#ifndef AT_FDCWD
#define AT_FDCWD (-100)
#endif

// Delta: dirfd is IGNORED (no *at family on Windows); path is resolved
// against the process CWD when relative. AT_SYMLINK_NOFOLLOW in flags is
// ignored: SetFileTime follows the link, matching the callers' intent.
// Delta: timespec are rounded to FILETIME's 100 ns granularity.
inline int utimensat(int /*dirfd*/, const char* path,
                     const struct timespec times[2], int /*flags*/) {
  if (path == nullptr || times == nullptr) {
    errno = EINVAL;
    return -1;
  }
  // FILETIME: 100 ns ticks since 1601-01-01 UTC; Unix epoch is 11644473600 s
  // later.
  const auto to_filetime = [](const struct timespec& ts) {
    const long long ticks =
        (static_cast<long long>(ts.tv_sec) + 11644473600LL) * 10000000LL +
        static_cast<long long>(ts.tv_nsec) / 100LL;
    FILETIME ft{};
    ft.dwLowDateTime = static_cast<DWORD>(ticks & 0xffffffffLL);
    ft.dwHighDateTime = static_cast<DWORD>(ticks >> 32);
    return ft;
  };
  const int wlen = ::MultiByteToWideChar(CP_UTF8, 0, path, -1, nullptr, 0);
  if (wlen <= 0) {
    errno = EINVAL;
    return -1;
  }
  std::wstring wpath(static_cast<std::size_t>(wlen), L'\0');
  ::MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath.data(), wlen);
  HANDLE h = ::CreateFileW(wpath.c_str(), FILE_WRITE_ATTRIBUTES,
                           FILE_SHARE_READ | FILE_SHARE_WRITE |
                               FILE_SHARE_DELETE,
                           nullptr, OPEN_EXISTING,
                           FILE_FLAG_BACKUP_SEMANTICS, nullptr);
  if (h == INVALID_HANDLE_VALUE) {
    errno = ENOENT;
    return -1;
  }
  const FILETIME atime = to_filetime(times[0]);
  const FILETIME mtime = to_filetime(times[1]);
  const BOOL ok = ::SetFileTime(h, nullptr, &atime, &mtime);
  ::CloseHandle(h);
  if (!ok) {
    errno = EACCES;
    return -1;
  }
  return 0;
}

// ---- filesystem ----------------------------------------------------------

// Contract (ID-C2-7): the template's directory part MUST already be a
// writable Windows path — callers holding `/tmp/...` literals are portable
// rewrites (fs::temp_directory_path), not shim consumers. The trailing
// `XXXXXX` is replaced in place, exactly like POSIX mkdtemp.
// Delta: uniqueness comes from rand()+GetTickCount mixing, not the POSIX
// algorithm; collision behavior (EEXIST retry, 100 attempts) matches.
inline char* mkdtemp(char* tmpl) {
  if (tmpl == nullptr) {
    errno = EINVAL;
    return nullptr;
  }
  const std::size_t len = std::strlen(tmpl);
  if (len < 6 || std::strcmp(tmpl + len - 6, "XXXXXX") != 0) {
    errno = EINVAL;
    return nullptr;
  }
  static const char alphabet[] =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  unsigned long long mix =
      static_cast<unsigned long long>(::GetTickCount64()) << 16 ^
      static_cast<unsigned long long>(::GetCurrentProcessId()) << 8 ^
      static_cast<unsigned long long>(std::rand());
  for (int attempt = 0; attempt < 100; ++attempt) {
    for (int i = 0; i < 6; ++i) {
      mix = mix * 6364136223846793005ULL + 1442695040888963407ULL;
      tmpl[len - 6 + static_cast<std::size_t>(i)] =
          alphabet[(mix >> 33) % 62];
    }
    if (::_mkdir(tmpl) == 0) return tmpl;
    if (errno != EEXIST) return nullptr;
  }
  errno = EEXIST;
  return nullptr;
}

#endif  // _WIN32
