// The platform seam: every OS-specific operation the engine performs, behind
// one namespace with two implementations (platform_posix.cpp, platform_win.cpp).
//
// Rules that shape this surface (see windows-enablement/05-quality-review-2.md,
// ID-1/ID-2):
//
//  * Every file offset and size is std::int64_t. Never off_t, never long:
//    Windows' CRT off_t and _pread are 32-bit unless the 64-bit names are
//    forced, and a naive shim silently truncates checkpoints larger than 2 GiB.
//    The POSIX implementations are pass-throughs (POSIX off_t is already
//    64-bit), so call sites name the safe function once on every platform.
//  * Functions that fail report it the POSIX way: -1/false plus errno. The
//    Windows implementation translates GetLastError into errno so callers keep
//    one error vocabulary (EINTR, EAGAIN, EEXIST, ENOENT, ...).
//  * Paths are UTF-8. The Windows side converts to UTF-16 at the boundary;
//    POSIX passes bytes through unchanged.
//  * Nothing here includes <windows.h>; the header stays includable from any
//    TU without dragging the SDK (and its min/max macros) into the engine.
#pragma once

#include <cstddef>
#include <cstdint>
#include <ctime>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#include <BaseTsd.h>  // SSIZE_T
#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif
#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif
#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#endif
#else
#include <sys/types.h>  // ssize_t
#endif

namespace lse::platform {

#ifdef _WIN32
using ssize_t = SSIZE_T;
#else
using ssize_t = ::ssize_t;
#endif

// ---------------------------------------------------------------------------
// Files. Descriptors are the CRT's int fds on both platforms; the Windows side
// opens with _SH_DENYNO so a later CreateFileMappingW on the same path does not
// fail on a sharing violation, and reads/writes go through ReadFile/WriteFile
// with an explicit 64-bit offset (never _lseeki64+_read on an fd something
// else also seeks).
// ---------------------------------------------------------------------------

// Read-only, close-on-exec. Returns -1 and sets errno.
int open_read(std::string_view path);

// Open for reading and writing, creating with `mode` if absent. Used for lock
// files. `no_follow` refuses a symlinked path (O_NOFOLLOW on POSIX); Windows
// CRT fds have no symlink-refusing open, so it is accepted there as a
// documented gap (the paths involved are inside the engine's own cache dir).
int open_read_write_create(std::string_view path, int mode = 0600,
                           bool no_follow = false);

// Write-only, created. `exclusive` fails with EEXIST when the file is there;
// `truncate` empties an existing file.
int open_write_create(std::string_view path, bool exclusive, bool truncate,
                      int mode = 0644);

// Closes the descriptor and reports whether close(2)/_close succeeded: POSIX
// callers check it to catch deferred write errors on flush paths. The fd is
// consumed either way.
bool close_fd(int fd);

// Positioned I/O, EINTR-retried inside. A short count is the caller's to
// loop on, exactly like pread(2)/pwrite(2). Returns -1 and sets errno.
ssize_t pread64(int fd, void* buf, std::size_t count, std::int64_t offset);
ssize_t pwrite64(int fd, const void* buf, std::size_t count,
                 std::int64_t offset);

// Sequential write, single attempt (callers keep their EINTR loops).
ssize_t write_fd(int fd, const void* buf, std::size_t count);

// Seek from the start of the file. Returns the new offset, or -1 plus errno.
std::int64_t seek_fd(int fd, std::int64_t offset);

// Sizes: fd_size via fstat, path_size via stat. -1 plus errno on failure.
std::int64_t fd_size(int fd);
std::int64_t path_size(std::string_view path);

bool truncate_fd(int fd, std::int64_t size);
bool truncate_path(std::string_view path, std::int64_t size);

// rename(2): atomically move `from` onto `to`, replacing an existing `to`
// (an existing destination DIRECTORY is replaced only when empty). False
// plus errno on failure; Windows maps a non-empty destination to ENOTEMPTY
// so callers' EEXIST/ENOTEMPTY race checks behave on both platforms.
bool rename_path(std::string_view from, std::string_view to);

// fsync. sync_dir commits a directory entry where that concept exists and is
// a no-op where it does not (Windows).
bool sync_fd(int fd);
void sync_dir(std::string_view path);

// ---------------------------------------------------------------------------
// Path queries.
// ---------------------------------------------------------------------------

bool exists(std::string_view path);
bool is_reg(std::string_view path);
bool is_dir(std::string_view path);
bool is_symlink(std::string_view path);
bool is_reg_fd(int fd);

// access(W_OK|X_OK) for a directory the engine wants to write into. Windows
// has no X_OK; the answer there is "a probe file can be created and removed".
bool dir_writable(std::string_view path);

// Free bytes available to the user on the volume holding `path` (statvfs /
// GetDiskFreeSpaceExW). -1 plus errno on failure.
std::int64_t free_space_bytes(std::string_view path);

// Identity for dev/ino dedup. The UCRT reports st_ino as 0, which would
// collapse every file onto one identity, so Windows answers with
// GetFileInformationByHandle's volume serial + file index instead.
struct FileId {
  std::uint64_t dev = 0;
  std::uint64_t ino = 0;
  bool valid = false;
};
FileId file_id_of_fd(int fd);
FileId file_id_of_path(std::string_view path);

// The stat fields cache-identity stamps need (dev, ino, size, mtime). UCRT
// st_ino is always 0 and its struct stat has no nanosecond mtime, so Windows
// fills this from GetFileInformationByHandle + GetFileTime; POSIX fills it
// from stat's st_mtim (st_mtimespec on Apple). Invalid when the path cannot
// be stat'ed.
struct FileStamp {
  std::uint64_t dev = 0;
  std::uint64_t ino = 0;
  std::int64_t size = -1;
  std::int64_t mtime_ns = 0;
  bool valid = false;
};
FileStamp file_stamp_of_path(std::string_view path);

// Names (not paths) of a directory's entries, "." and ".." excluded. Empty on
// failure; errno set.
std::vector<std::string> dir_entries(std::string_view path);

// mkdtemp semantics: rewrites the pattern's trailing XXXXXX in place and
// creates the directory. Returns false plus errno on failure.
bool make_temp_dir(std::string& pattern);

// mkstemp semantics: rewrites the pattern's trailing XXXXXX in place, creates
// the file atomically with owner-only permissions, and returns a read/write
// fd. -1 plus errno on failure.
int make_temp_file(std::string& pattern);

// Absolute path of this executable; empty when unknown.
std::string exe_path();

// fnmatch(pattern, name, 0): '*' spans separators, '?' is one char,
// [...] classes supported. Windows has no fnmatch; the matcher there is a
// small in-shim implementation with the same accepted grammar.
bool glob_match(std::string_view pattern, std::string_view name);

// ---------------------------------------------------------------------------
// Memory mapping. file_map_ro maps `size` bytes of a file read-only and keeps
// whatever the mapping needs alive: POSIX closes the fd (the mapping outlives
// it), Windows holds the section and file handles in an internal registry
// keyed by the view pointer, so the public shape stays (ptr, size).
// ---------------------------------------------------------------------------

std::size_t page_size();

void* anon_alloc(std::size_t bytes);            // nullptr on failure
void anon_free(void* ptr, std::size_t bytes);

void* file_map_ro(std::string_view path, std::int64_t size);  // nullptr + errno
void file_unmap(void* ptr, std::size_t mapped_bytes);

// "These pages may be dropped" (MADV_DONTNEED / MEM_RESET). For a read-only
// file mapping the bytes come back from the file on next touch.
void map_discard(void* ptr, std::size_t bytes);

// ---------------------------------------------------------------------------
// Whole-file advisory locks on an fd (flock / LockFileEx). Locking is
// per-file, held until unlock or close.
// ---------------------------------------------------------------------------

bool lock_exclusive(int fd);       // blocking; EINTR-retried
bool try_lock_exclusive(int fd);   // false + EWOULDBLOCK when held elsewhere
void unlock_exclusive(int fd);

// ---------------------------------------------------------------------------
// Pipes and descriptor juggling (the API log capture redirects stderr with).
// ---------------------------------------------------------------------------

bool make_pipe(int fds[2]);
int dup_fd(int fd);
bool dup2_fd(int old_fd, int new_fd);

// ---------------------------------------------------------------------------
// Environment, process, time.
// ---------------------------------------------------------------------------

bool set_env(std::string_view key, std::string_view value);
bool unset_env(std::string_view key);

// HOME, falling back to USERPROFILE on Windows. Empty when neither is set.
std::string home_dir();

std::int64_t pid();
bool hostname(std::string& out);
bool is_tty(int fd);

// Lift the soft fd limit to the hard limit where that concept exists; no-op
// on Windows (its limit is a per-process handle budget, not a soft cap).
void raise_fd_limit();

// localtime_r / localtime_s — note the arguments are REVERSED between the two;
// this wrapper is the only place that may know.
bool localtime_of(std::time_t t, struct tm& out);

// CLOCK_MONOTONIC and CLOCK_BOOTTIME as nanoseconds. Windows: QPC for
// monotonic and QueryInterruptTime (bias included, i.e. counts across sleep
// like CLOCK_BOOTTIME) for boot time.
std::uint64_t now_monotonic_ns();
std::uint64_t now_boottime_ns();

// ---------------------------------------------------------------------------
// Dynamic loading. On Windows LoadLibraryW fails cleanly on POSIX sonames
// ("libibverbs.so.1"), which is exactly the decline-with-reason path the
// fabric probes want: an absent library must yield nullptr, not a crash.
// ---------------------------------------------------------------------------

void* dl_open(std::string_view path);
void* dl_open_self();                       // the running image
void* dl_sym(void* handle, std::string_view symbol);
int dl_close(void* handle);

// ---------------------------------------------------------------------------
// Memory accounting for lse_status (the JSON keys must not change shape).
// ---------------------------------------------------------------------------

std::uint64_t process_rss_bytes();          // 0 when unknown

struct HeapStats {
  std::uint64_t in_use = 0;    // mallinfo2 uordblks+hblkhd / 0 on Windows
  std::uint64_t allocated = 0; // mallinfo2 arena+hblkhd   / 0 on Windows
};
HeapStats heap_stats();

}  // namespace lse::platform
