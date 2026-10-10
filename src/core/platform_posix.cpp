// POSIX implementation of the platform seam. Every function reproduces the
// exact call the engine made before the seam existed: same flags, same EINTR
// policy, same errno. Offsets are int64_t at the seam and off_t underneath,
// which is 64-bit on every POSIX target this engine builds for.
#include "lse/core/platform.hpp"

#ifndef _WIN32

#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstring>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <mach/mach.h>
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif

#ifndef MAP_ANON
#define MAP_ANON MAP_ANONYMOUS
#endif

namespace lse::platform {
namespace {

std::string to_string(std::string_view s) { return std::string(s); }

bool stat_path(std::string_view path, struct stat& st) {
  return ::stat(to_string(path).c_str(), &st) == 0;
}

}  // namespace

int open_read(std::string_view path) {
  return ::open(to_string(path).c_str(), O_RDONLY | O_CLOEXEC);
}

int open_read_write_create(std::string_view path, int mode, bool no_follow) {
  int flags = O_RDWR | O_CREAT | O_CLOEXEC;
  if (no_follow) flags |= O_NOFOLLOW;
  return ::open(to_string(path).c_str(), flags, static_cast<mode_t>(mode));
}

int open_write_create(std::string_view path, bool exclusive, bool truncate,
                      int mode) {
  int flags = O_WRONLY | O_CREAT | O_CLOEXEC;
  if (exclusive) flags |= O_EXCL;
  if (truncate) flags |= O_TRUNC;
  return ::open(to_string(path).c_str(), flags, static_cast<mode_t>(mode));
}

bool close_fd(int fd) { return ::close(fd) == 0; }

ssize_t pread64(int fd, void* buf, std::size_t count, std::int64_t offset) {
  for (;;) {
    const ssize_t n =
        ::pread(fd, buf, count, static_cast<off_t>(offset));
    if (n < 0 && errno == EINTR) continue;
    return n;
  }
}

ssize_t pwrite64(int fd, const void* buf, std::size_t count,
                 std::int64_t offset) {
  for (;;) {
    const ssize_t n =
        ::pwrite(fd, buf, count, static_cast<off_t>(offset));
    if (n < 0 && errno == EINTR) continue;
    return n;
  }
}

ssize_t write_fd(int fd, const void* buf, std::size_t count) {
  return ::write(fd, buf, count);
}

std::int64_t seek_fd(int fd, std::int64_t offset) {
  const off_t r = ::lseek(fd, static_cast<off_t>(offset), SEEK_SET);
  return r < 0 ? -1 : static_cast<std::int64_t>(r);
}

std::int64_t fd_size(int fd) {
  struct stat st {};
  if (::fstat(fd, &st) != 0) return -1;
  return static_cast<std::int64_t>(st.st_size);
}

std::int64_t path_size(std::string_view path) {
  struct stat st {};
  if (!stat_path(path, st)) return -1;
  return static_cast<std::int64_t>(st.st_size);
}

bool truncate_fd(int fd, std::int64_t size) {
  return ::ftruncate(fd, static_cast<off_t>(size)) == 0;
}

bool truncate_path(std::string_view path, std::int64_t size) {
  return ::truncate(to_string(path).c_str(), static_cast<off_t>(size)) == 0;
}

bool rename_path(std::string_view from, std::string_view to) {
  return ::rename(to_string(from).c_str(), to_string(to).c_str()) == 0;
}

bool sync_fd(int fd) { return ::fsync(fd) == 0; }

void sync_dir(std::string_view path) {
  const int fd = ::open(to_string(path).c_str(), O_RDONLY | O_CLOEXEC);
  if (fd >= 0) {
    (void)::fsync(fd);
    ::close(fd);
  }
}

bool exists(std::string_view path) {
  struct stat st {};
  return stat_path(path, st);
}

bool is_reg(std::string_view path) {
  struct stat st {};
  return stat_path(path, st) && S_ISREG(st.st_mode);
}

bool is_dir(std::string_view path) {
  struct stat st {};
  return stat_path(path, st) && S_ISDIR(st.st_mode);
}

bool is_symlink(std::string_view path) {
  struct stat st {};
  if (::lstat(to_string(path).c_str(), &st) != 0) return false;
  return S_ISLNK(st.st_mode);
}

bool is_reg_fd(int fd) {
  struct stat st {};
  return ::fstat(fd, &st) == 0 && S_ISREG(st.st_mode);
}

bool dir_writable(std::string_view path) {
  return ::access(to_string(path).c_str(), W_OK | X_OK) == 0;
}

std::int64_t free_space_bytes(std::string_view path) {
  struct statvfs st {};
  if (::statvfs(to_string(path).c_str(), &st) != 0) return -1;
  return static_cast<std::int64_t>(
      static_cast<std::uint64_t>(st.f_bavail) *
      static_cast<std::uint64_t>(st.f_frsize));
}

FileId file_id_of_fd(int fd) {
  struct stat st {};
  if (::fstat(fd, &st) != 0) return {};
  return FileId{static_cast<std::uint64_t>(st.st_dev),
                static_cast<std::uint64_t>(st.st_ino), true};
}

FileId file_id_of_path(std::string_view path) {
  struct stat st {};
  if (!stat_path(path, st)) return {};
  return FileId{static_cast<std::uint64_t>(st.st_dev),
                static_cast<std::uint64_t>(st.st_ino), true};
}

FileStamp file_stamp_of_path(std::string_view path) {
  struct stat st {};
  if (!stat_path(path, st)) return {};
  FileStamp out;
  out.dev = static_cast<std::uint64_t>(st.st_dev);
  out.ino = static_cast<std::uint64_t>(st.st_ino);
  out.size = static_cast<std::int64_t>(st.st_size);
#ifdef __APPLE__
  out.mtime_ns = static_cast<std::int64_t>(st.st_mtimespec.tv_sec) *
                     1000000000LL +
                 st.st_mtimespec.tv_nsec;
#else
  out.mtime_ns =
      static_cast<std::int64_t>(st.st_mtim.tv_sec) * 1000000000LL +
      st.st_mtim.tv_nsec;
#endif
  out.valid = true;
  return out;
}

std::vector<std::string> dir_entries(std::string_view path) {
  std::vector<std::string> out;
  DIR* d = ::opendir(to_string(path).c_str());
  if (d == nullptr) {
    return out;
  }
  while (const dirent* e = ::readdir(d)) {
    if (std::strcmp(e->d_name, ".") == 0 || std::strcmp(e->d_name, "..") == 0) {
      continue;
    }
    out.emplace_back(e->d_name);
  }
  ::closedir(d);
  return out;
}

bool make_temp_dir(std::string& pattern) {
  return ::mkdtemp(pattern.data()) != nullptr;
}

int make_temp_file(std::string& pattern) { return ::mkstemp(pattern.data()); }

std::string exe_path() {
#ifdef __APPLE__
  char buffer[4096];
  std::uint32_t size = sizeof(buffer);
  if (::_NSGetExecutablePath(buffer, &size) != 0) return {};
  return std::string(buffer);
#else
  char buffer[PATH_MAX];
  const ssize_t n = ::readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
  if (n <= 0) return {};
  buffer[n] = '\0';
  return std::string(buffer, static_cast<std::size_t>(n));
#endif
}

bool glob_match(std::string_view pattern, std::string_view name) {
  return ::fnmatch(to_string(pattern).c_str(), to_string(name).c_str(), 0) == 0;
}

std::size_t page_size() {
  const long p = ::sysconf(_SC_PAGESIZE);
  return p > 0 ? static_cast<std::size_t>(p) : 4096;
}

void* anon_alloc(std::size_t bytes) {
  if (bytes == 0) bytes = 1;
  void* p = ::mmap(nullptr, bytes, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANON, -1, 0);
  return p == MAP_FAILED ? nullptr : p;
}

void anon_free(void* ptr, std::size_t bytes) {
  if (ptr == nullptr) return;
  if (bytes == 0) bytes = 1;
  (void)::munmap(ptr, bytes);
}

void* file_map_ro(std::string_view path, std::int64_t size) {
  const int fd = ::open(to_string(path).c_str(), O_RDONLY | O_CLOEXEC);
  if (fd < 0) return nullptr;
  void* p = ::mmap(nullptr, static_cast<std::size_t>(size), PROT_READ,
                   MAP_PRIVATE, fd, 0);
  ::close(fd);
  if (p == MAP_FAILED) return nullptr;
  return p;
}

void file_unmap(void* ptr, std::size_t mapped_bytes) {
  if (ptr == nullptr) return;
  (void)::munmap(ptr, mapped_bytes);
}

void map_discard(void* ptr, std::size_t bytes) {
  if (ptr == nullptr) return;
  (void)::madvise(ptr, bytes, MADV_DONTNEED);
}

bool lock_exclusive(int fd) {
  for (;;) {
    if (::flock(fd, LOCK_EX) == 0) return true;
    if (errno == EINTR) continue;
    return false;
  }
}

bool try_lock_exclusive(int fd) { return ::flock(fd, LOCK_EX | LOCK_NB) == 0; }

void unlock_exclusive(int fd) { (void)::flock(fd, LOCK_UN); }

bool make_pipe(int fds[2]) { return ::pipe(fds) == 0; }

int dup_fd(int fd) { return ::dup(fd); }

bool dup2_fd(int old_fd, int new_fd) { return ::dup2(old_fd, new_fd) == new_fd; }

bool set_env(std::string_view key, std::string_view value) {
  return ::setenv(to_string(key).c_str(), to_string(value).c_str(), 1) == 0;
}

bool unset_env(std::string_view key) {
  return ::unsetenv(to_string(key).c_str()) == 0;
}

std::string home_dir() {
  const char* h = std::getenv("HOME");
  return h == nullptr ? std::string() : std::string(h);
}

std::int64_t pid() { return static_cast<std::int64_t>(::getpid()); }

bool hostname(std::string& out) {
  char buf[256];
  if (::gethostname(buf, sizeof(buf)) != 0) return false;
  buf[sizeof(buf) - 1] = '\0';
  out.assign(buf);
  return true;
}

bool is_tty(int fd) { return ::isatty(fd) != 0; }

void raise_fd_limit() {
  ::rlimit lim{};
  if (::getrlimit(RLIMIT_NOFILE, &lim) != 0) return;
  if (lim.rlim_cur == lim.rlim_max) return;
  ::rlimit next = lim;
  next.rlim_cur = lim.rlim_max;
  if (::setrlimit(RLIMIT_NOFILE, &next) == 0) return;
  // Some kernels refuse the unlimited form; ask for a concrete ceiling that is
  // still far past what a model needs.
  next.rlim_cur = lim.rlim_max < 65536 ? lim.rlim_max : 65536;
  if (next.rlim_cur > lim.rlim_cur) (void)::setrlimit(RLIMIT_NOFILE, &next);
}

bool localtime_of(std::time_t t, struct tm& out) {
  return ::localtime_r(&t, &out) != nullptr;
}

std::uint64_t now_monotonic_ns() {
  struct timespec ts {};
  ::clock_gettime(CLOCK_MONOTONIC, &ts);
  return static_cast<std::uint64_t>(ts.tv_sec) * 1000000000ull +
         static_cast<std::uint64_t>(ts.tv_nsec);
}

std::uint64_t now_boottime_ns() {
  struct timespec ts {};
#ifdef CLOCK_BOOTTIME
  if (::clock_gettime(CLOCK_BOOTTIME, &ts) == 0) {
    return static_cast<std::uint64_t>(ts.tv_sec) * 1000000000ull +
           static_cast<std::uint64_t>(ts.tv_nsec);
  }
#endif
  return now_monotonic_ns();
}

void* dl_open(std::string_view path) {
  return ::dlopen(to_string(path).c_str(), RTLD_LAZY | RTLD_LOCAL);
}

void* dl_open_self() { return ::dlopen(nullptr, RTLD_NOW | RTLD_GLOBAL); }

void* dl_sym(void* handle, std::string_view symbol) {
  return ::dlsym(handle, to_string(symbol).c_str());
}

int dl_close(void* handle) { return ::dlclose(handle); }

std::uint64_t process_rss_bytes() {
#if defined(__APPLE__)
  task_vm_info_data_t info{};
  mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
  if (task_info(mach_task_self(), TASK_VM_INFO,
                reinterpret_cast<task_info_t>(&info), &count) != KERN_SUCCESS)
    return 0;
  return info.phys_footprint;
#else
  // Resident set size from /proc/self/statm, in pages.
  unsigned long size = 0, resident = 0;
  if (FILE* f = std::fopen("/proc/self/statm", "r")) {
    if (std::fscanf(f, "%lu %lu", &size, &resident) != 2) resident = 0;
    std::fclose(f);
  }
  return static_cast<std::uint64_t>(resident) *
         static_cast<std::uint64_t>(page_size());
#endif
}

HeapStats heap_stats() {
  HeapStats h;
#if defined(__APPLE__)
  malloc_statistics_t heap{};
  malloc_zone_statistics(nullptr, &heap);
  h.in_use = heap.size_in_use;
  h.allocated = heap.size_allocated;
#else
  const struct mallinfo2 mi = mallinfo2();
  h.in_use = mi.uordblks + mi.hblkhd;
  h.allocated = mi.arena + mi.hblkhd;
#endif
  return h;
}

}  // namespace lse::platform

#endif  // !_WIN32
