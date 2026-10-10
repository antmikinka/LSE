// Windows implementation of the platform seam (ID-1/ID-2).
//
// The rules that keep this honest:
//
//  * Every positioned read/write goes through ReadFile/WriteFile with an
//    OVERLAPPED carrying a 64-bit offset. The CRT's _pread takes a 32-bit long
//    offset and would silently truncate checkpoints above 2 GiB; _lseeki64 +
//    _read on a shared fd races any other user of that fd. Neither is used.
//  * Files are opened with _wsopen_s(_SH_DENYNO): CreateFileMappingW on the
//    same path fails with a sharing violation otherwise, and weight loading
//    maps and streams the same checkpoint.
//  * Failures translate GetLastError into errno so callers keep one error
//    vocabulary with POSIX.
//  * Paths arrive UTF-8 and convert to UTF-16 at the boundary.
//
// file_map_ro keeps the section and file handles alive in a process-wide
// registry keyed by the view pointer: weights.cpp closes its fd right after
// mapping, and on Windows a view dies with its section handle, so the shim
// owns the handles until file_unmap.
#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <direct.h>
#include <fcntl.h>
#include <io.h>
#include <psapi.h>  // PROCESS_MEMORY_COUNTERS (K32GetProcessMemoryInfo is in kernel32)
#include <share.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>

#include "lse/core/platform.hpp"

namespace lse::platform {
namespace {

// ---------------------------------------------------------------------------
// Error translation.
// ---------------------------------------------------------------------------

void set_errno_from_win32(DWORD e) {
  errno = [e] {
    switch (e) {
      case ERROR_FILE_NOT_FOUND:
      case ERROR_PATH_NOT_FOUND:
      case ERROR_INVALID_NAME:
        return ENOENT;
      case ERROR_ACCESS_DENIED:
      case ERROR_SHARING_VIOLATION:
      case ERROR_LOCK_VIOLATION:
        return EACCES;
      case ERROR_FILE_EXISTS:
      case ERROR_ALREADY_EXISTS:
        return EEXIST;
      case ERROR_INVALID_HANDLE:
        return EBADF;
      case ERROR_NOT_ENOUGH_MEMORY:
      case ERROR_OUTOFMEMORY:
      case ERROR_COMMITMENT_LIMIT:
        return ENOMEM;
      case ERROR_DISK_FULL:
        return ENOSPC;
      case ERROR_NOT_SAME_DEVICE:
        return EXDEV;
      case ERROR_DIRECTORY:
        return EISDIR;
      case ERROR_TOO_MANY_OPEN_FILES:
        return EMFILE;
      case ERROR_INVALID_PARAMETER:
        return EINVAL;
      default:
        return EIO;
    }
  }();
}

void set_errno_from_last() { set_errno_from_win32(::GetLastError()); }

std::wstring to_wide(std::string_view s) {
  if (s.empty()) return {};
  const int n = ::MultiByteToWideChar(CP_UTF8, 0, s.data(),
                                      static_cast<int>(s.size()), nullptr, 0);
  if (n <= 0) return {};
  std::wstring w(static_cast<std::size_t>(n), L'\0');
  ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()),
                        w.data(), n);
  return w;
}

std::string from_wide(std::wstring_view w) {
  if (w.empty()) return {};
  const int n = ::WideCharToMultiByte(CP_UTF8, 0, w.data(),
                                      static_cast<int>(w.size()), nullptr, 0,
                                      nullptr, nullptr);
  if (n <= 0) return {};
  std::string s(static_cast<std::size_t>(n), '\0');
  ::WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()),
                        s.data(), n, nullptr, nullptr);
  return s;
}

HANDLE fd_to_handle(int fd) {
  if (fd < 0) return INVALID_HANDLE_VALUE;
  const intptr_t h = ::_get_osfhandle(fd);
  return h == -1 ? INVALID_HANDLE_VALUE : reinterpret_cast<HANDLE>(h);
}

// A whole-file byte-range lock, matching flock()'s whole-file semantics.
OVERLAPPED lock_range() {
  OVERLAPPED ov{};
  ov.Offset = 0;
  ov.OffsetHigh = 0;
  return ov;
}

constexpr DWORD kLockFlags = LOCKFILE_EXCLUSIVE_LOCK;

// ---------------------------------------------------------------------------
// Mapping registry (see the file header).
// ---------------------------------------------------------------------------

struct FileMap {
  HANDLE section = INVALID_HANDLE_VALUE;
  HANDLE file = INVALID_HANDLE_VALUE;
  std::size_t size = 0;
};

std::mutex& map_registry_mutex() {
  static std::mutex m;
  return m;
}

std::map<void*, FileMap>& map_registry() {
  static std::map<void*, FileMap> r;
  return r;
}

// ---------------------------------------------------------------------------
// Clocks.
// ---------------------------------------------------------------------------

std::uint64_t qpc_ns() {
  static const struct Qpc {
    LARGE_INTEGER freq;
    Qpc() { ::QueryPerformanceFrequency(&freq); }
  } qpc;
  LARGE_INTEGER now;
  ::QueryPerformanceCounter(&now);
  // Split into whole seconds + fraction so neither product can overflow:
  // ticks * 1e9 wraps after ~30 minutes of uptime at a 10 MHz counter.
  const std::uint64_t freq = static_cast<std::uint64_t>(qpc.freq.QuadPart);
  const std::uint64_t t = static_cast<std::uint64_t>(now.QuadPart);
  const std::uint64_t sec = t / freq, frac = t % freq;
  return sec * 1000000000ull + (frac * 1000000000ull) / freq;
}

// ---------------------------------------------------------------------------
// fnmatch-equivalent matcher: '*' spans everything, '?' is one character,
// [...] is a class with optional leading ! and ranges. Backslash escapes.
// ---------------------------------------------------------------------------

bool match_here(const char* p, const char* s);

bool match_class(const char*& p, const char* s) {
  // *p points just past '['. Returns whether s[0] matched; advances p past ']'.
  bool negate = false;
  if (*p == '!' || *p == '^') {
    negate = true;
    ++p;
  }
  bool matched = false;
  bool first = true;
  for (; *p != '\0'; ++p) {
    if (*p == ']' && !first) {
      ++p;
      return matched != negate;
    }
    first = false;
    char lo = *p;
    if (lo == '\\') {
      if (*++p == '\0') break;
      lo = *p;
    }
    if (p[1] == '-' && p[2] != '\0' && p[2] != ']') {
      char hi = p[2];
      if (hi == '\\') {
        if (p[3] == '\0') break;
        hi = p[3];
        p += 1;  // the loop's ++p steps once more
      }
      if (*s >= lo && *s <= hi) matched = true;
      p += 2;
    } else if (*s == lo) {
      matched = true;
    }
  }
  return false;  // unterminated class: no match, like fnmatch
}

bool match_here(const char* p, const char* s) {
  for (;;) {
    if (*p == '\0') return *s == '\0';
    if (*p == '*') {
      while (*p == '*') ++p;
      if (*p == '\0') return true;
      for (; *s != '\0'; ++s) {
        if (match_here(p, s)) return true;
      }
      return false;
    }
    if (*s == '\0') return false;
    if (*p == '?') {
      ++p;
      ++s;
      continue;
    }
    if (*p == '[') {
      const char* q = p + 1;
      if (!match_class(q, s)) return false;
      p = q;
      ++s;
      continue;
    }
    if (*p == '\\' && p[1] != '\0') ++p;
    if (*p != *s) return false;
    ++p;
    ++s;
  }
}

}  // namespace

// ---------------------------------------------------------------------------
// Files.
// ---------------------------------------------------------------------------

int open_read(std::string_view path) {
  const std::wstring w = to_wide(path);
  int fd = -1;
  // _SH_DENYNO: other handles (including our own CreateFileMappingW) must be
  // able to open the same file while this fd lives.
  const errno_t e = ::_wsopen_s(&fd, w.c_str(), _O_RDONLY | _O_BINARY,
                                _SH_DENYNO, 0);
  if (e != 0) {
    errno = e;
    return -1;
  }
  return fd;
}

int open_read_write_create(std::string_view path, int mode, bool no_follow) {
  (void)no_follow;  // documented gap: CRT opens follow reparse points
  const std::wstring w = to_wide(path);
  int fd = -1;
  const errno_t e =
      ::_wsopen_s(&fd, w.c_str(), _O_RDWR | _O_CREAT | _O_BINARY, _SH_DENYNO,
                  _S_IREAD | _S_IWRITE);
  (void)mode;
  if (e != 0) {
    errno = e;
    return -1;
  }
  return fd;
}

int open_write_create(std::string_view path, bool exclusive, bool truncate,
                      int mode) {
  (void)mode;
  const std::wstring w = to_wide(path);
  int oflag = _O_WRONLY | _O_CREAT | _O_BINARY;
  if (exclusive) oflag |= _O_EXCL;
  if (truncate) oflag |= _O_TRUNC;
  int fd = -1;
  const errno_t e =
      ::_wsopen_s(&fd, w.c_str(), oflag, _SH_DENYNO, _S_IREAD | _S_IWRITE);
  if (e != 0) {
    errno = e;
    return -1;
  }
  return fd;
}

bool close_fd(int fd) {
  if (fd < 0) return false;
  return ::_close(fd) == 0;
}

ssize_t pread64(int fd, void* buf, std::size_t count, std::int64_t offset) {
  HANDLE h = fd_to_handle(fd);
  if (h == INVALID_HANDLE_VALUE) {
    errno = EBADF;
    return -1;
  }
  std::size_t done = 0;
  while (done < count) {
    // ReadFile's length is a DWORD: never ask for more than it can express.
    const std::size_t want =
        (std::min)(count - done, static_cast<std::size_t>(0x7FFFFFFFul));
    OVERLAPPED ov{};
    const std::uint64_t at = static_cast<std::uint64_t>(offset) + done;
    ov.Offset = static_cast<DWORD>(at & 0xFFFFFFFFull);
    ov.OffsetHigh = static_cast<DWORD>(at >> 32);
    DWORD got = 0;
    if (!::ReadFile(h, static_cast<char*>(buf) + done,
                    static_cast<DWORD>(want), &got, &ov)) {
      if (::GetLastError() == ERROR_HANDLE_EOF) {
        break;  // short read at EOF, exactly like pread returning < count
      }
      set_errno_from_last();
      return -1;
    }
    if (got == 0) break;
    done += got;
  }
  return static_cast<ssize_t>(done);
}

ssize_t pwrite64(int fd, const void* buf, std::size_t count,
                 std::int64_t offset) {
  HANDLE h = fd_to_handle(fd);
  if (h == INVALID_HANDLE_VALUE) {
    errno = EBADF;
    return -1;
  }
  std::size_t done = 0;
  while (done < count) {
    const std::size_t want =
        (std::min)(count - done, static_cast<std::size_t>(0x7FFFFFFFul));
    OVERLAPPED ov{};
    const std::uint64_t at = static_cast<std::uint64_t>(offset) + done;
    ov.Offset = static_cast<DWORD>(at & 0xFFFFFFFFull);
    ov.OffsetHigh = static_cast<DWORD>(at >> 32);
    DWORD wrote = 0;
    if (!::WriteFile(h, static_cast<const char*>(buf) + done,
                     static_cast<DWORD>(want), &wrote, &ov)) {
      set_errno_from_last();
      return -1;
    }
    if (wrote == 0) {
      errno = EIO;
      return -1;
    }
    done += wrote;
  }
  return static_cast<ssize_t>(done);
}

ssize_t write_fd(int fd, const void* buf, std::size_t count) {
  // Sequential write at the fd's current position: the CRT tracks it, and the
  // callers (flush loops) own the EINTR/partial-write policy exactly as they
  // do with write(2).
  const int wrote = ::_write(fd, buf, static_cast<unsigned>(count));
  return static_cast<ssize_t>(wrote);
}

std::int64_t seek_fd(int fd, std::int64_t offset) {
  const long long r = ::_lseeki64(fd, static_cast<long long>(offset), SEEK_SET);
  if (r == -1) return -1;
  return static_cast<std::int64_t>(r);
}

std::int64_t fd_size(int fd) {
  struct _stat64 st {};
  if (::_fstat64(fd, &st) != 0) return -1;
  return static_cast<std::int64_t>(st.st_size);
}

std::int64_t path_size(std::string_view path) {
  struct _stat64 st {};
  if (::_wstat64(to_wide(path).c_str(), &st) != 0) return -1;
  return static_cast<std::int64_t>(st.st_size);
}

bool truncate_fd(int fd, std::int64_t size) {
  return ::_chsize_s(fd, static_cast<long long>(size)) == 0;
}

bool truncate_path(std::string_view path, std::int64_t size) {
  const int fd = open_read_write_create(path);
  if (fd < 0) return false;
  const bool ok = truncate_fd(fd, size);
  close_fd(fd);
  return ok;
}

bool rename_path(std::string_view from, std::string_view to) {
  // MOVEFILE_REPLACE_EXISTING is rename(2)'s contract: an empty destination
  // (file or directory) is replaced; a NON-empty destination directory fails.
  if (::MoveFileExW(to_wide(from).c_str(), to_wide(to).c_str(),
                    MOVEFILE_REPLACE_EXISTING)) {
    return true;
  }
  const DWORD e = ::GetLastError();
  if (e == ERROR_DIR_NOT_EMPTY) {
    errno = ENOTEMPTY;
    return false;
  }
  if (e == ERROR_ACCESS_DENIED && is_dir(to)) {
    // Windows reports replacing a non-empty directory as access-denied; the
    // POSIX answer is ENOTEMPTY, which is what concurrent-writer races check.
    errno = ENOTEMPTY;
    return false;
  }
  set_errno_from_win32(e);
  return false;
}

bool sync_fd(int fd) { return ::_commit(fd) == 0; }

void sync_dir(std::string_view path) {
  (void)path;  // NTFS metadata ordering needs no user-space directory fsync
}

bool exists(std::string_view path) {
  const DWORD a = ::GetFileAttributesW(to_wide(path).c_str());
  return a != INVALID_FILE_ATTRIBUTES;
}

bool is_reg(std::string_view path) {
  struct _stat64 st {};
  return ::_wstat64(to_wide(path).c_str(), &st) == 0 && (st.st_mode & _S_IFREG);
}

bool is_dir(std::string_view path) {
  struct _stat64 st {};
  return ::_wstat64(to_wide(path).c_str(), &st) == 0 && (st.st_mode & _S_IFDIR);
}

bool is_symlink(std::string_view path) {
  const DWORD a = ::GetFileAttributesW(to_wide(path).c_str());
  return a != INVALID_FILE_ATTRIBUTES &&
         (a & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

bool is_reg_fd(int fd) {
  struct _stat64 st {};
  return ::_fstat64(fd, &st) == 0 && (st.st_mode & _S_IFREG);
}

bool dir_writable(std::string_view path) {
  // W_OK|X_OK has no meaning on NTFS ACLs; probe by creating and removing a
  // temp name, which is what the engine actually needs to know.
  std::wstring w = to_wide(path);
  if (w.empty()) return false;
  if (w.back() != L'\\' && w.back() != L'/') w.push_back(L'\\');
  w += L".lse-probe-XXXXXX";
  char tail[16];
  for (unsigned attempt = 0; attempt < 8; ++attempt) {
    std::snprintf(tail, sizeof tail, "%06u",
                  static_cast<unsigned>(
                      (::GetCurrentProcessId() + attempt * 7919u) % 1000000u));
    std::wstring probe = w;
    const std::size_t at = probe.find(L"XXXXXX");
    const std::wstring t = to_wide(std::string_view(tail));
    probe.replace(at, 6, t);
    HANDLE h = ::CreateFileW(probe.c_str(), GENERIC_WRITE, 0, nullptr,
                             CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
      ::CloseHandle(h);
      ::DeleteFileW(probe.c_str());
      return true;
    }
    const DWORD e = ::GetLastError();
    if (e != ERROR_FILE_EXISTS && e != ERROR_ALREADY_EXISTS) {
      set_errno_from_win32(e);
      return false;
    }
  }
  errno = EACCES;
  return false;
}

std::int64_t free_space_bytes(std::string_view path) {
  ULARGE_INTEGER free_to_user{}, total{}, total_free{};
  if (!::GetDiskFreeSpaceExW(to_wide(path).c_str(), &free_to_user, &total,
                             &total_free)) {
    set_errno_from_last();
    return -1;
  }
  return static_cast<std::int64_t>(free_to_user.QuadPart);
}

FileId file_id_of_fd(int fd) {
  HANDLE h = fd_to_handle(fd);
  if (h == INVALID_HANDLE_VALUE) return {};
  BY_HANDLE_FILE_INFORMATION info{};
  if (!::GetFileInformationByHandle(h, &info)) return {};
  return FileId{info.dwVolumeSerialNumber,
                (static_cast<std::uint64_t>(info.nFileIndexHigh) << 32) |
                    info.nFileIndexLow,
                true};
}

FileId file_id_of_path(std::string_view path) {
  HANDLE h = ::CreateFileW(to_wide(path).c_str(), 0,
                           FILE_SHARE_READ | FILE_SHARE_WRITE |
                               FILE_SHARE_DELETE,
                           nullptr, OPEN_EXISTING,
                           FILE_FLAG_BACKUP_SEMANTICS, nullptr);
  if (h == INVALID_HANDLE_VALUE) return {};
  BY_HANDLE_FILE_INFORMATION info{};
  const bool ok = ::GetFileInformationByHandle(h, &info);
  ::CloseHandle(h);
  if (!ok) return {};
  return FileId{info.dwVolumeSerialNumber,
                (static_cast<std::uint64_t>(info.nFileIndexHigh) << 32) |
                    info.nFileIndexLow,
                true};
}

FileStamp file_stamp_of_path(std::string_view path) {
  HANDLE h = ::CreateFileW(to_wide(path).c_str(), 0,
                           FILE_SHARE_READ | FILE_SHARE_WRITE |
                               FILE_SHARE_DELETE,
                           nullptr, OPEN_EXISTING,
                           FILE_FLAG_BACKUP_SEMANTICS, nullptr);
  if (h == INVALID_HANDLE_VALUE) {
    set_errno_from_last();
    return {};
  }
  BY_HANDLE_FILE_INFORMATION info{};
  FILETIME written{};
  const bool ok = ::GetFileInformationByHandle(h, &info) &&
                  ::GetFileTime(h, nullptr, nullptr, &written);
  ::CloseHandle(h);
  if (!ok) {
    errno = EIO;
    return {};
  }
  FileStamp out;
  out.dev = info.dwVolumeSerialNumber;
  out.ino = (static_cast<std::uint64_t>(info.nFileIndexHigh) << 32) |
            info.nFileIndexLow;
  out.size = (static_cast<std::int64_t>(info.nFileSizeHigh) << 32) |
             info.nFileSizeLow;
  // FILETIME is 100 ns ticks since 1601-01-01; the Unix epoch is
  // 11644473600 s later. Timestamps before the epoch clamp to 0.
  const std::uint64_t ticks =
      (static_cast<std::uint64_t>(written.dwHighDateTime) << 32) |
      written.dwLowDateTime;
  constexpr std::uint64_t kEpoch100ns = 116444736000000000ull;
  out.mtime_ns = ticks > kEpoch100ns
                     ? static_cast<std::int64_t>((ticks - kEpoch100ns) * 100ull)
                     : 0;
  out.valid = true;
  return out;
}

std::vector<std::string> dir_entries(std::string_view path) {
  std::vector<std::string> out;
  std::wstring pattern = to_wide(path);
  if (pattern.empty()) return out;
  if (pattern.back() != L'\\' && pattern.back() != L'/') pattern.push_back(L'\\');
  pattern += L'*';
  WIN32_FIND_DATAW fd{};
  HANDLE h = ::FindFirstFileW(pattern.c_str(), &fd);
  if (h == INVALID_HANDLE_VALUE) {
    set_errno_from_last();
    return out;
  }
  do {
    if (std::wcscmp(fd.cFileName, L".") == 0 ||
        std::wcscmp(fd.cFileName, L"..") == 0) {
      continue;
    }
    out.push_back(from_wide(fd.cFileName));
  } while (::FindNextFileW(h, &fd));
  ::FindClose(h);
  return out;
}

bool make_temp_dir(std::string& pattern) {
  // mkdtemp contract: replace the trailing XXXXXX and create the directory
  // atomically, retrying on collision.
  const std::size_t n = pattern.size();
  if (n < 6 || pattern.compare(n - 6, 6, "XXXXXX") != 0) {
    errno = EINVAL;
    return false;
  }
  for (unsigned int attempt = 0; attempt < 64; ++attempt) {
    // Collision space is the retry loop's job; the seed only has to differ
    // between concurrent processes.
    const unsigned int seed =
        ::GetTickCount() ^ (static_cast<unsigned int>(::GetCurrentProcessId())
                            << 8) ^ (attempt * 2654435761u);
    char tail[8];
    std::snprintf(tail, sizeof tail, "%06u", seed % 1000000u);
    std::string candidate =
        pattern.substr(0, n - 6) + std::string(tail, 6);
    const std::wstring w = to_wide(candidate);
    if (::CreateDirectoryW(w.c_str(), nullptr)) {
      pattern = std::move(candidate);
      return true;
    }
    const DWORD e = ::GetLastError();
    if (e != ERROR_ALREADY_EXISTS && e != ERROR_FILE_EXISTS) {
      set_errno_from_win32(e);
      return false;
    }
  }
  errno = EEXIST;
  return false;
}

int make_temp_file(std::string& pattern) {
  // mkstemp contract: replace the trailing XXXXXX, create the file with
  // owner-only access (CREATE_NEW with no sharing is at least as strict), and
  // hand back a read/write fd.
  const std::size_t n = pattern.size();
  if (n < 6 || pattern.compare(n - 6, 6, "XXXXXX") != 0) {
    errno = EINVAL;
    return -1;
  }
  for (unsigned int attempt = 0; attempt < 64; ++attempt) {
    const unsigned int seed =
        ::GetTickCount() ^ (static_cast<unsigned int>(::GetCurrentProcessId())
                            << 8) ^ (attempt * 2654435761u);
    char tail[8];
    std::snprintf(tail, sizeof tail, "%06u", seed % 1000000u);
    std::string candidate =
        pattern.substr(0, n - 6) + std::string(tail, 6);
    HANDLE h = ::CreateFileW(to_wide(candidate).c_str(),
                             GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                             CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
      const int fd = ::_open_osfhandle(reinterpret_cast<intptr_t>(h), 0);
      if (fd < 0) {
        ::CloseHandle(h);
        errno = EMFILE;
        return -1;
      }
      pattern = std::move(candidate);
      return fd;
    }
    const DWORD e = ::GetLastError();
    if (e != ERROR_FILE_EXISTS && e != ERROR_ALREADY_EXISTS) {
      set_errno_from_win32(e);
      return -1;
    }
  }
  errno = EEXIST;
  return -1;
}

std::string exe_path() {
  wchar_t buf[MAX_PATH * 4];
  const DWORD n = ::GetModuleFileNameW(nullptr, buf,
                                       static_cast<DWORD>(std::size(buf)));
  if (n == 0 || n >= std::size(buf)) return {};
  return from_wide(std::wstring_view(buf, n));
}

bool glob_match(std::string_view pattern, std::string_view name) {
  const std::string p(pattern), s(name);
  return match_here(p.c_str(), s.c_str());
}

// ---------------------------------------------------------------------------
// Memory.
// ---------------------------------------------------------------------------

std::size_t page_size() {
  SYSTEM_INFO si{};
  ::GetSystemInfo(&si);
  return si.dwPageSize;
}

void* anon_alloc(std::size_t bytes) {
  if (bytes == 0) bytes = 1;
  void* p = ::VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT,
                           PAGE_READWRITE);
  if (p == nullptr) set_errno_from_last();
  return p;
}

void anon_free(void* ptr, std::size_t bytes) {
  (void)bytes;
  if (ptr != nullptr) (void)::VirtualFree(ptr, 0, MEM_RELEASE);
}

void* file_map_ro(std::string_view path, std::int64_t size) {
  if (size <= 0) {
    errno = EINVAL;
    return nullptr;
  }
  const std::wstring w = to_wide(path);
  HANDLE file = ::CreateFileW(w.c_str(), GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE |
                                  FILE_SHARE_DELETE,
                              nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                              nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    set_errno_from_last();
    return nullptr;
  }
  const auto usize = static_cast<std::uint64_t>(size);
  HANDLE section = ::CreateFileMappingW(file, nullptr, PAGE_READONLY,
                                        static_cast<DWORD>(usize >> 32),
                                        static_cast<DWORD>(usize & 0xFFFFFFFFull),
                                        nullptr);
  if (section == nullptr) {
    set_errno_from_last();
    ::CloseHandle(file);
    return nullptr;
  }
  // Map in <=2 GiB chunks would be needed above that; checkpoints are mapped
  // whole, and MapViewOfFile's SIZE_T length is 64-bit, so one call suffices.
  void* view = ::MapViewOfFile(section, FILE_MAP_READ, 0, 0,
                               static_cast<SIZE_T>(size));
  if (view == nullptr) {
    set_errno_from_last();
    ::CloseHandle(section);
    ::CloseHandle(file);
    return nullptr;
  }
  {
    std::lock_guard held(map_registry_mutex());
    map_registry()[view] = FileMap{section, file, static_cast<std::size_t>(size)};
  }
  return view;
}

void file_unmap(void* ptr, std::size_t mapped_bytes) {
  (void)mapped_bytes;  // UnmapViewOfFile unmaps the whole view
  if (ptr == nullptr) return;
  FileMap fm;
  bool found = false;
  {
    std::lock_guard held(map_registry_mutex());
    auto& reg = map_registry();
    if (const auto it = reg.find(ptr); it != reg.end()) {
      fm = it->second;
      found = true;
      reg.erase(it);
    }
  }
  (void)::UnmapViewOfFile(ptr);
  if (found) {
    ::CloseHandle(fm.section);
    ::CloseHandle(fm.file);
  }
}

void map_discard(void* ptr, std::size_t bytes) {
  if (ptr == nullptr || bytes == 0) return;
  // MEM_RESET: "these pages' contents are no longer of interest" — the VM
  // manager may drop them and re-fault from the backing file, which is the
  // MADV_DONTNEED contract read-only checkpoint mappings rely on.
  (void)::VirtualAlloc(ptr, bytes, MEM_RESET, PAGE_READONLY);
}

// ---------------------------------------------------------------------------
// Locks.
// ---------------------------------------------------------------------------

bool lock_exclusive(int fd) {
  HANDLE h = fd_to_handle(fd);
  if (h == INVALID_HANDLE_VALUE) {
    errno = EBADF;
    return false;
  }
  OVERLAPPED ov = lock_range();
  for (;;) {
    if (::LockFileEx(h, kLockFlags, 0, 0xFFFFFFFFul, 0xFFFFFFFFul, &ov)) {
      return true;
    }
    const DWORD e = ::GetLastError();
    if (e == ERROR_LOCK_VIOLATION) {
      // LockFileEx without FAIL_IMMEDIATELY blocks; a violation here means an
      // overlapping byte-range lock raced — retry like EINTR.
      ::Sleep(10);
      continue;
    }
    set_errno_from_win32(e);
    return false;
  }
}

bool try_lock_exclusive(int fd) {
  HANDLE h = fd_to_handle(fd);
  if (h == INVALID_HANDLE_VALUE) {
    errno = EBADF;
    return false;
  }
  OVERLAPPED ov = lock_range();
  if (::LockFileEx(h, kLockFlags | LOCKFILE_FAIL_IMMEDIATELY, 0, 0xFFFFFFFFul,
                   0xFFFFFFFFul, &ov)) {
    return true;
  }
  const DWORD e = ::GetLastError();
  errno = (e == ERROR_LOCK_VIOLATION) ? EWOULDBLOCK : 0;
  if (errno == 0) set_errno_from_win32(e);
  return false;
}

void unlock_exclusive(int fd) {
  HANDLE h = fd_to_handle(fd);
  if (h == INVALID_HANDLE_VALUE) return;
  OVERLAPPED ov = lock_range();
  (void)::UnlockFileEx(h, 0, 0xFFFFFFFFul, 0xFFFFFFFFul, &ov);
}

// ---------------------------------------------------------------------------
// Pipes.
// ---------------------------------------------------------------------------

bool make_pipe(int fds[2]) { return ::_pipe(fds, 4096, _O_BINARY) == 0; }

int dup_fd(int fd) { return ::_dup(fd); }

bool dup2_fd(int old_fd, int new_fd) { return ::_dup2(old_fd, new_fd) == 0; }

// ---------------------------------------------------------------------------
// Environment, process, time.
// ---------------------------------------------------------------------------

bool set_env(std::string_view key, std::string_view value) {
  return ::_wputenv_s(to_wide(key).c_str(), to_wide(value).c_str()) == 0;
}

bool unset_env(std::string_view key) {
  return ::_wputenv_s(to_wide(key).c_str(), L"") == 0;
}

std::string home_dir() {
  const char* h = std::getenv("HOME");
  if (h != nullptr && *h != '\0') return std::string(h);
  wchar_t* profile = nullptr;
  std::size_t len = 0;
  if (::_wdupenv_s(&profile, &len, L"USERPROFILE") == 0 && profile != nullptr) {
    std::string out = from_wide(profile);
    std::free(profile);
    return out;
  }
  return {};
}

std::int64_t pid() { return static_cast<std::int64_t>(::_getpid()); }

bool hostname(std::string& out) {
  wchar_t buf[256];
  DWORD n = static_cast<DWORD>(std::size(buf));
  if (!::GetComputerNameExW(ComputerNamePhysicalDnsHostname, buf, &n)) {
    set_errno_from_last();
    return false;
  }
  out = from_wide(std::wstring_view(buf, n));
  return true;
}

bool is_tty(int fd) { return ::_isatty(fd) != 0; }

void raise_fd_limit() {
  // Windows has no soft fd cap to raise; the CRT grows its fd table on demand.
}

bool localtime_of(std::time_t t, struct tm& out) {
  // localtime_s takes (tm*, time_t*) — REVERSED against localtime_r.
  return ::localtime_s(&out, &t) == 0;
}

std::uint64_t now_monotonic_ns() { return qpc_ns(); }

std::uint64_t now_boottime_ns() {
  // QueryInterruptTime: 100 ns units since boot, biased (counts across sleep),
  // matching CLOCK_BOOTTIME.
  ULONGLONG ticks = 0;
  ::QueryInterruptTime(&ticks);
  return static_cast<std::uint64_t>(ticks) * 100ull;
}

// ---------------------------------------------------------------------------
// Dynamic loading.
// ---------------------------------------------------------------------------

void* dl_open(std::string_view path) {
  HMODULE m = ::LoadLibraryW(to_wide(path).c_str());
  if (m == nullptr) set_errno_from_last();
  return static_cast<void*>(m);
}

void* dl_open_self() {
  HMODULE m = nullptr;
  (void)::GetModuleHandleExW(
      GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, nullptr, &m);
  return static_cast<void*>(m);
}

void* dl_sym(void* handle, std::string_view symbol) {
  if (handle == nullptr) return nullptr;
  std::string s(symbol);
  void* p = reinterpret_cast<void*>(
      ::GetProcAddress(static_cast<HMODULE>(handle), s.c_str()));
  if (p == nullptr) set_errno_from_last();
  return p;
}

int dl_close(void* handle) {
  if (handle == nullptr) return 0;
  return ::FreeLibrary(static_cast<HMODULE>(handle)) ? 0 : -1;
}

// ---------------------------------------------------------------------------
// Memory accounting.
// ---------------------------------------------------------------------------

std::uint64_t process_rss_bytes() {
  PROCESS_MEMORY_COUNTERS pmc{};
  if (!::K32GetProcessMemoryInfo(::GetCurrentProcess(), &pmc, sizeof(pmc))) {
    return 0;
  }
  return pmc.WorkingSetSize;
}

HeapStats heap_stats() {
  // The UCRT heap has no mallinfo2 equivalent that is cheap enough for a
  // status line; report zeros and keep the JSON keys (ID-2).
  return HeapStats{};
}

}  // namespace lse::platform

#endif  // _WIN32
