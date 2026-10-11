// Direct unit coverage of the lse::platform seam (04-test-strategy §4, B1):
// every shim API gets at least one case here rather than relying on an
// integration test to happen to touch it. Registered on ALL platforms — the
// POSIX side proves pass-through parity (WP3 acceptance), the Windows side
// exercises the GetDiskFreeSpaceExW / LockFileEx / QPC / DiscardVirtualMemory
// translations. Runtime budget < 5 s; the >2 GiB file cases live in
// test_large_file_io (LARGE_IO label).
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "harness.hpp"

#include "lse/core/platform.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>  // ::read on the pipe fd (sequential reads have no offset)
#endif

namespace fs = std::filesystem;
namespace plat = lse::platform;

namespace {

// A private temp directory for one case group; removed on scope exit.
struct TempDir {
  fs::path path;
  TempDir(const char* tag) {
    std::string pattern =
        (fs::temp_directory_path() / ("lse-shim-" + std::string(tag) +
                                      "-" + std::to_string(plat::pid()) +
                                      "-XXXXXX"))
            .string();
    if (!plat::make_temp_dir(pattern)) {
      std::fprintf(stderr, "make_temp_dir failed: %s\n", std::strerror(errno));
      std::abort();
    }
    path = fs::path(pattern);
  }
  ~TempDir() {
    std::error_code ec;
    fs::remove_all(path, ec);
  }
  fs::path operator/(const std::string& name) const { return path / name; }
};

std::string read_whole(const fs::path& p) {
  const int fd = plat::open_read(p.string());
  if (fd < 0) return {};
  std::string out;
  char buf[4096];
  for (;;) {
    const auto n = plat::pread64(fd, buf, sizeof buf,
                                 static_cast<std::int64_t>(out.size()));
    if (n <= 0) break;
    out.append(buf, static_cast<std::size_t>(n));
  }
  plat::close_fd(fd);
  return out;
}

}  // namespace

// --- types (MJ-3/G10): the vocabulary the shim promises ---------------------

LSE_TEST(types_and_constants_match_the_posix_vocabulary) {
  static_assert(sizeof(plat::ssize_t) == 8, "ssize_t must be 64-bit");
  static_assert(sizeof(std::int64_t) == 8);
  LSE_EXPECT_EQ(STDIN_FILENO, 0);
  LSE_EXPECT_EQ(STDOUT_FILENO, 1);
  LSE_EXPECT_EQ(STDERR_FILENO, 2);
}

// --- fd ops: positioned I/O at unaligned offsets, EOF, sizes, flags ---------

LSE_TEST(positioned_io_round_trips_and_reports_eof) {
  TempDir td("fd");
  const fs::path f = td / "probe.bin";
  // Read/write: the positioned reads below need a readable descriptor (a
  // write-only fd answers EBADF to pread on both platforms, as it must).
  const int fd = plat::open_read_write_create(f.string());
  LSE_EXPECT(fd >= 0);
  if (fd < 0) return;

  // Unaligned offsets on both sides of a page boundary.
  const std::string a(1000, 'a'), b(233, 'b');
  LSE_EXPECT_EQ(plat::pwrite64(fd, a.data(), a.size(), 4093), (plat::ssize_t)1000);
  LSE_EXPECT_EQ(plat::pwrite64(fd, b.data(), b.size(), 99999), (plat::ssize_t)233);
  LSE_EXPECT_EQ(plat::fd_size(fd), (std::int64_t)100232);
  LSE_EXPECT_EQ(plat::path_size(f.string()), (std::int64_t)100232);

  std::string back(233, '\0');
  LSE_EXPECT_EQ(plat::pread64(fd, back.data(), back.size(), 99999),
                (plat::ssize_t)233);
  LSE_EXPECT(back == b);
  // The hole between the two writes reads as zeros.
  std::vector<char> hole(64, 'x');
  LSE_EXPECT_EQ(plat::pread64(fd, hole.data(), hole.size(), 60000),
                (plat::ssize_t)64);
  LSE_EXPECT(std::all_of(hole.begin(), hole.end(),
                         [](char c) { return c == '\0'; }));

  // EOF: a read starting at/past the end yields 0, never garbage or -1.
  char one = 'z';
  LSE_EXPECT_EQ(plat::pread64(fd, &one, 1, 100232), (plat::ssize_t)0);
  LSE_EXPECT_EQ(plat::pread64(fd, &one, 1, 1 << 20), (plat::ssize_t)0);
  LSE_EXPECT_EQ(one, 'z');
  // A read straddling EOF is short, exactly like pread(2).
  std::vector<char> tail(16, 'y');
  LSE_EXPECT_EQ(plat::pread64(fd, tail.data(), tail.size(), 100224),
                (plat::ssize_t)8);

  // seek_fd + write_fd (sequential path) and sync_fd.
  LSE_EXPECT_EQ(plat::seek_fd(fd, 0), (std::int64_t)0);
  LSE_EXPECT_EQ(plat::write_fd(fd, "head", 4), (plat::ssize_t)4);
  LSE_EXPECT(plat::sync_fd(fd));
  LSE_EXPECT(plat::is_reg_fd(fd));
  LSE_EXPECT(plat::close_fd(fd));

  // truncate_fd shrinks; truncate_path grows (zero-filled) and shrinks again.
  const int fd2 = plat::open_read_write_create(f.string());
  LSE_EXPECT(fd2 >= 0);
  LSE_EXPECT(plat::truncate_fd(fd2, 10));
  LSE_EXPECT_EQ(plat::fd_size(fd2), (std::int64_t)10);
  LSE_EXPECT(plat::close_fd(fd2));
  LSE_EXPECT(plat::truncate_path(f.string(), 50));
  LSE_EXPECT_EQ(plat::path_size(f.string()), (std::int64_t)50);
  LSE_EXPECT_EQ(read_whole(f).size(), 50u);
}

LSE_TEST(open_flag_mapping_is_faithful) {
  TempDir td("flags");
  const fs::path f = td / "excl.bin";
  const int first = plat::open_write_create(f.string(), true, false);
  LSE_EXPECT(first >= 0);
  LSE_EXPECT(plat::close_fd(first));

  // exclusive create over an existing file: -1 + EEXIST on both platforms.
  errno = 0;
  const int second = plat::open_write_create(f.string(), true, false);
  LSE_EXPECT_EQ(second, -1);
  LSE_EXPECT_EQ(errno, EEXIST);

  // truncate on open empties.
  const int trunc = plat::open_write_create(f.string(), false, true);
  LSE_EXPECT(trunc >= 0);
  LSE_EXPECT_EQ(plat::fd_size(trunc), (std::int64_t)0);
  LSE_EXPECT(plat::close_fd(trunc));

  // read-only open of a missing file: -1 + ENOENT.
  errno = 0;
  LSE_EXPECT_EQ(plat::open_read((td / "missing.bin").string()), -1);
  LSE_EXPECT_EQ(errno, ENOENT);

  // no_follow is accepted on a regular file (it is a symlink-refusing hint;
  // Windows documents the CRT-fd gap in platform.hpp).
  const int nf = plat::open_read_write_create((td / "nf.bin").string(), 0600,
                                              /*no_follow=*/true);
  LSE_EXPECT(nf >= 0);
  if (nf >= 0) LSE_EXPECT(plat::close_fd(nf));
}

LSE_TEST(rename_replaces_and_reports) {
  TempDir td("rename");
  const fs::path from = td / "from.txt";
  const fs::path to = td / "to.txt";
  const int fd = plat::open_write_create(from.string(), true, false);
  LSE_EXPECT(fd >= 0);
  LSE_EXPECT_EQ(plat::write_fd(fd, "data", 4), (plat::ssize_t)4);
  LSE_EXPECT(plat::close_fd(fd));
  plat::sync_dir(td.path.string());

  // rename onto an existing regular file replaces it (POSIX rename /
  // MoveFileExW REPLACE_EXISTING).
  const int blocker = plat::open_write_create(to.string(), true, false);
  LSE_EXPECT(blocker >= 0);
  LSE_EXPECT(plat::close_fd(blocker));
  LSE_EXPECT(plat::rename_path(from.string(), to.string()));
  LSE_EXPECT(!plat::exists(from.string()));
  LSE_EXPECT(read_whole(to) == "data");

  errno = 0;
  LSE_EXPECT(!plat::rename_path((td / "ghost.txt").string(), to.string()));
  LSE_EXPECT_EQ(errno, ENOENT);
}

// --- stat family / ids ------------------------------------------------------

LSE_TEST(stat_family_classifies_and_identifies) {
  TempDir td("stat");
  const fs::path f = td / "file.bin";
  const int fd = plat::open_write_create(f.string(), true, false);
  LSE_EXPECT(fd >= 0);
  const std::string body(777, 'q');
  LSE_EXPECT_EQ(plat::write_fd(fd, body.data(), body.size()), (plat::ssize_t)777);
  LSE_EXPECT(plat::close_fd(fd));

  LSE_EXPECT(plat::exists(f.string()));
  LSE_EXPECT(plat::is_reg(f.string()));
  LSE_EXPECT(!plat::is_dir(f.string()));
  LSE_EXPECT(!plat::is_symlink(f.string()));
  LSE_EXPECT(plat::is_dir(td.path.string()));
  LSE_EXPECT(!plat::is_reg(td.path.string()));
  LSE_EXPECT(!plat::exists((td / "nothing").string()));
  LSE_EXPECT(plat::dir_writable(td.path.string()));
  LSE_EXPECT(!plat::dir_writable(f.string()));

  // dev/ino identity: two stats of one file agree; a second file differs.
  const plat::FileId a = plat::file_id_of_path(f.string());
  const plat::FileId b = plat::file_id_of_path(f.string());
  LSE_EXPECT(a.valid && b.valid);
  LSE_EXPECT_EQ(a.dev, b.dev);
  LSE_EXPECT_EQ(a.ino, b.ino);
  const fs::path other = td / "other.bin";
  const int fd2 = plat::open_write_create(other.string(), true, false);
  LSE_EXPECT(fd2 >= 0);
  LSE_EXPECT(plat::close_fd(fd2));
  const plat::FileId c = plat::file_id_of_path(other.string());
  LSE_EXPECT(c.valid);
  LSE_EXPECT(a.ino != c.ino);
  const int fd3 = plat::open_read(f.string());
  LSE_EXPECT(fd3 >= 0);
  if (fd3 >= 0) {
    LSE_EXPECT_EQ(plat::file_id_of_fd(fd3).ino, a.ino);
    plat::close_fd(fd3);
  }
  // The UCRT reports st_ino == 0; the shim must not pass that through (it
  // would collapse every file onto one identity — platform.hpp:127-134).
  LSE_EXPECT(a.ino != 0u);

  const plat::FileStamp st = plat::file_stamp_of_path(f.string());
  LSE_EXPECT(st.valid);
  LSE_EXPECT_EQ(st.size, (std::int64_t)777);
  LSE_EXPECT(st.mtime_ns > 0);
  LSE_EXPECT(!plat::file_stamp_of_path((td / "nothing").string()).valid);

  // Symlink classification where the privilege exists; on Windows without
  // Developer Mode create_symlink fails and this degrades to a printed note.
  std::error_code ec;
  const fs::path link = td / "link.bin";
  fs::create_symlink(f, link, ec);
  if (ec) {
    std::printf("       note: symlink creation unavailable (%s)\n",
                ec.message().c_str());
  } else {
    LSE_EXPECT(plat::is_symlink(link.string()));
    LSE_EXPECT(plat::is_reg(link.string()));  // follows, like stat(2)
  }
}

LSE_TEST(dir_entries_is_the_exact_name_set) {
  TempDir td("dir");
  for (const char* n : {"alpha", "beta", "gamma"}) {
    const int fd = plat::open_write_create((td / n).string(), true, false);
    LSE_EXPECT(fd >= 0);
    if (fd >= 0) plat::close_fd(fd);
  }
  std::vector<std::string> got = plat::dir_entries(td.path.string());
  std::sort(got.begin(), got.end());
  const std::vector<std::string> want{"alpha", "beta", "gamma"};
  LSE_EXPECT(got == want);  // "." and ".." excluded
  LSE_EXPECT(plat::dir_entries((td / "alpha").string()).empty());
}

// --- temps ------------------------------------------------------------------

LSE_TEST(mkdtemp_makes_a_hundred_unique_dirs) {
  std::set<std::string> made;
  for (int i = 0; i < 100; ++i) {
    std::string pattern =
        (fs::temp_directory_path() /
         ("lse-shim-mk-XXXXXX"))
            .string();
    LSE_EXPECT(plat::make_temp_dir(pattern));
    LSE_EXPECT(pattern.find("XXXXXX") == std::string::npos);
    LSE_EXPECT(plat::is_dir(pattern));
    LSE_EXPECT(made.insert(pattern).second);  // unique
  }
  for (const auto& p : made) {
    std::error_code ec;
    fs::remove_all(p, ec);
    LSE_EXPECT(!ec);
  }
  LSE_EXPECT_EQ(made.size(), 100u);
}

LSE_TEST(mkstemp_hands_back_a_usable_rw_descriptor) {
  std::string pattern =
      (fs::temp_directory_path() / "lse-shim-file-XXXXXX").string();
  const int fd = plat::make_temp_file(pattern);
  LSE_EXPECT(fd >= 0);
  if (fd < 0) return;
  LSE_EXPECT(pattern.find("XXXXXX") == std::string::npos);
  LSE_EXPECT(plat::is_reg(pattern));
  LSE_EXPECT_EQ(plat::write_fd(fd, "temp", 4), (plat::ssize_t)4);
  char buf[4] = {};
  LSE_EXPECT_EQ(plat::pread64(fd, buf, 4, 0), (plat::ssize_t)4);
  LSE_EXPECT(std::memcmp(buf, "temp", 4) == 0);
  LSE_EXPECT(plat::close_fd(fd));
  std::error_code ec;
  fs::remove(pattern, ec);
  LSE_EXPECT(!plat::exists(pattern));
}

// --- mmap family ------------------------------------------------------------

LSE_TEST(anon_and_file_mappings_behave_as_documented) {
  LSE_EXPECT(plat::page_size() >= 4096u);
  LSE_EXPECT((plat::page_size() & (plat::page_size() - 1)) == 0u);

  // Anon RW: write/read pattern.
  const std::size_t bytes = plat::page_size() * 3;
  std::uint8_t* mem = static_cast<std::uint8_t*>(plat::anon_alloc(bytes));
  LSE_EXPECT(mem != nullptr);
  if (mem == nullptr) return;
  for (std::size_t i = 0; i < bytes; ++i) mem[i] = static_cast<std::uint8_t>(i * 31 + 7);
  LSE_EXPECT_EQ(mem[bytes - 1], static_cast<std::uint8_t>((bytes - 1) * 31 + 7));

  // MADV_DONTNEED / DiscardVirtualMemory on anonymous memory reads back zero
  // (04 §4 read-back-zero; documented on Windows and Linux).
  plat::map_discard(mem, bytes);
#if defined(_WIN32) || defined(__linux__)
  LSE_EXPECT(std::all_of(mem, mem + bytes,
                         [](std::uint8_t v) { return v == 0; }));
#endif
  plat::anon_free(mem, bytes);

  // File-backed RO map equals pread content; discard re-faults from the file.
  TempDir td("map");
  const fs::path f = td / "mapped.bin";
  const int fd = plat::open_write_create(f.string(), true, false);
  LSE_EXPECT(fd >= 0);
  if (fd < 0) return;
  std::string body(plat::page_size() * 2, '\0');
  for (std::size_t i = 0; i < body.size(); ++i)
    body[i] = static_cast<char>('A' + (i % 29));
  LSE_EXPECT_EQ(plat::write_fd(fd, body.data(), body.size()),
                (plat::ssize_t)body.size());
  LSE_EXPECT(plat::close_fd(fd));

  void* map = plat::file_map_ro(f.string(), static_cast<std::int64_t>(body.size()));
  LSE_EXPECT(map != nullptr);
  if (map == nullptr) return;
  LSE_EXPECT(std::memcmp(map, body.data(), body.size()) == 0);
  plat::map_discard(map, body.size());
  // For a read-only FILE mapping the bytes come back from the file on next
  // touch (platform.hpp:188-190) — NOT zeros; that is the checkpoint
  // release_pages() contract.
  LSE_EXPECT(std::memcmp(map, body.data(), body.size()) == 0);
  plat::file_unmap(map, body.size());
  // (Deliberately no double-unmap of the same pointer: on POSIX the address
  // range may already be reused, so munmap-twice is UB the shim cannot make
  // safe; nothing in the engine does it.)
}

// --- clocks -----------------------------------------------------------------

LSE_TEST(clocks_are_monotonic_and_consistent) {
  std::uint64_t mono = plat::now_monotonic_ns();
  std::uint64_t boot = plat::now_boottime_ns();
  LSE_EXPECT(mono > 0u);
  LSE_EXPECT(boot > 0u);
  for (int i = 0; i < 10000; ++i) {  // non-decreasing over 10k samples
    const std::uint64_t m2 = plat::now_monotonic_ns();
    const std::uint64_t b2 = plat::now_boottime_ns();
    LSE_EXPECT(m2 >= mono);
    LSE_EXPECT(b2 >= boot);
    mono = m2;
    boot = b2;
  }

  // A 10 ms sleep measures within [8, 200] ms of monotonic time.
  const std::uint64_t t0 = plat::now_monotonic_ns();
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  const double ms =
      static_cast<double>(plat::now_monotonic_ns() - t0) / 1.0e6;
  std::printf("       10 ms sleep measured as %.3f ms\n", ms);
  LSE_EXPECT(ms >= 8.0 && ms <= 200.0);

  // boottime (biased interrupt time / CLOCK_BOOTTIME) counts sleep, so it is
  // >= monotonic up to the 15.6 ms Windows tick quantization of the interrupt
  // counter against continuous QPC.
  LSE_EXPECT(plat::now_boottime_ns() + 100'000'000u >= plat::now_monotonic_ns());

#ifdef _WIN32
  // Drift vs GetTickCount64 < 1% — measured over 2 s, not the 500 ms in the
  // 04 §4 table: GetTickCount64 updates once per 15.6 ms timer tick, so over
  // 500 ms the quantization alone (±1 tick = 3.1%) swamps a 1% bound. Over
  // 2 s, 1% = 20 ms > one tick, making the assertion physically meaningful.
  const std::uint64_t q0 = plat::now_monotonic_ns();
  const ULONGLONG g0 = ::GetTickCount64();
  std::this_thread::sleep_for(std::chrono::milliseconds(2000));
  const std::uint64_t q1 = plat::now_monotonic_ns();
  const ULONGLONG g1 = ::GetTickCount64();
  const double qpc_ms = static_cast<double>(q1 - q0) / 1.0e6;
  const double tick_ms = static_cast<double>(g1 - g0);
  std::printf("       2 s window: QPC %.3f ms vs GetTickCount64 %.0f ms\n",
              qpc_ms, tick_ms);
  LSE_EXPECT(std::fabs(qpc_ms - tick_ms) < 0.01 * tick_ms);
#endif
}

// --- statvfs → GetDiskFreeSpaceExW (G3) --------------------------------------

LSE_TEST(free_space_matches_the_os) {
  const std::string dir = fs::temp_directory_path().string();
  const std::int64_t free_bytes = plat::free_space_bytes(dir);
  LSE_EXPECT(free_bytes > 0);
  LSE_EXPECT(free_bytes < (std::int64_t{1} << 50));  // < 1 PiB sanity
  errno = 0;
  LSE_EXPECT_EQ(plat::free_space_bytes((fs::path(dir) / "no-such-dir").string()),
                (std::int64_t)-1);
#ifdef _WIN32
  ULARGE_INTEGER avail{}, total{}, total_free{};
  const fs::path p(dir);
  const std::wstring root = p.root_name().wstring() + L"\\";
  LSE_EXPECT(::GetDiskFreeSpaceExW(root.c_str(), &avail, &total, &total_free));
  LSE_EXPECT(std::llabs(free_bytes - static_cast<std::int64_t>(avail.QuadPart)) <
             (std::int64_t{256} << 20));  // live volume: allow async drift
  LSE_EXPECT(free_bytes <= static_cast<std::int64_t>(total.QuadPart));
#endif
}

// --- flock → LockFileEx (R8 whole-file semantics) ---------------------------

LSE_TEST(exclusive_locks_block_a_second_descriptor) {
  TempDir td("lock");
  const fs::path f = td / "the.lock";
  const int fd1 = plat::open_read_write_create(f.string());
  const int fd2 = plat::open_read_write_create(f.string());
  LSE_EXPECT(fd1 >= 0 && fd2 >= 0);
  if (fd1 < 0 || fd2 < 0) return;

  LSE_EXPECT(plat::lock_exclusive(fd1));
  errno = 0;
  LSE_EXPECT(!plat::try_lock_exclusive(fd2));
  LSE_EXPECT_EQ(errno, EWOULDBLOCK);  // ERROR_LOCK_VIOLATION translation
  plat::unlock_exclusive(fd1);
  LSE_EXPECT(plat::try_lock_exclusive(fd2));
  plat::unlock_exclusive(fd2);
  LSE_EXPECT(plat::close_fd(fd1));
  LSE_EXPECT(plat::close_fd(fd2));
}

// --- env --------------------------------------------------------------------

LSE_TEST(env_round_trips_through_the_c_runtime) {
  const char* key = "LSE_SHIM_ENV_PROBE";
  LSE_EXPECT(plat::set_env(key, "first"));
  LSE_EXPECT(std::getenv(key) != nullptr);
  LSE_EXPECT(std::string(std::getenv(key)) == "first");
  LSE_EXPECT(plat::set_env(key, "second"));  // overwrite
  LSE_EXPECT(std::string(std::getenv(key)) == "second");
  LSE_EXPECT(plat::unset_env(key));
  LSE_EXPECT(std::getenv(key) == nullptr);
}

// --- localtime_r → localtime_s (G2 reversed-args trap detector) -------------

LSE_TEST(localtime_of_matches_the_reference_for_known_epochs) {
  // Epochs chosen far from 0 and from 2^31 so swapped or truncated arguments
  // yield visibly wrong fields.
  for (const std::time_t t : {std::time_t{1234567890}, std::time_t{0},
                              std::time_t{2147483647}}) {
    struct tm got {};
    LSE_EXPECT(plat::localtime_of(t, got));
    const struct tm* want = std::localtime(&t);  // same TZ, same process
    LSE_EXPECT(want != nullptr);
    if (want == nullptr) continue;
    LSE_EXPECT_EQ(got.tm_year, want->tm_year);
    LSE_EXPECT_EQ(got.tm_mon, want->tm_mon);
    LSE_EXPECT_EQ(got.tm_mday, want->tm_mday);
    LSE_EXPECT_EQ(got.tm_hour, want->tm_hour);
    LSE_EXPECT_EQ(got.tm_min, want->tm_min);
    LSE_EXPECT_EQ(got.tm_sec, want->tm_sec);
    LSE_EXPECT_EQ(got.tm_wday, want->tm_wday);
    LSE_EXPECT_EQ(got.tm_yday, want->tm_yday);
    // mktime interprets `got` as local time and must land back on t.
    struct tm round = got;
    LSE_EXPECT_EQ(std::mktime(&round), t);
  }
}

// --- process, exe, tty, rlimit no-op ----------------------------------------

LSE_TEST(process_queries_answer) {
  LSE_EXPECT(plat::pid() > 0);
  std::string host;
  if (plat::hostname(host)) LSE_EXPECT(!host.empty());
  const std::string exe = plat::exe_path();
  LSE_EXPECT(!exe.empty());
  LSE_EXPECT(plat::is_reg(exe));
  (void)plat::is_tty(STDOUT_FILENO);  // callable, answer env-dependent
  (void)plat::is_tty(STDIN_FILENO);
  plat::raise_fd_limit();  // G1: a no-op on Windows; must not crash
}

LSE_TEST(memory_accounting_reports_the_documented_shape) {
  LSE_EXPECT(plat::process_rss_bytes() > 0u);
  const plat::HeapStats heap = plat::heap_stats();
#if defined(_WIN32)
  // N2 / 06 §4.4: the UCRT has no mallinfo2 — the shim answers zeros and the
  // API's JSON keys stay (host_memory_json in src/api/lse.cpp keeps the shape;
  // asserting the zeros here pins the documented contract).
  LSE_EXPECT_EQ(heap.in_use, 0u);
  LSE_EXPECT_EQ(heap.allocated, 0u);
#else
  (void)heap;  // POSIX: mallinfo2 numbers are live; only callability asserted.
#endif
}

// --- dl shim ----------------------------------------------------------------

LSE_TEST(dl_declines_cleanly_and_finds_self) {
  // A nonexistent library is a clean nullptr, not a crash (the fabric probes'
  // decline-with-reason path; platform.hpp:237-241).
  LSE_EXPECT(plat::dl_open("liblse_definitely_missing.so.1") == nullptr);
  LSE_EXPECT(plat::dl_open("lse_definitely_missing.dll") == nullptr);
  void* self = plat::dl_open_self();
  LSE_EXPECT(self != nullptr);
  LSE_EXPECT(plat::dl_sym(nullptr, "anything") == nullptr);
  if (self != nullptr) {
    LSE_EXPECT(plat::dl_sym(self, "lse_no_such_symbol_xyz") == nullptr);
    // Not dl_close(self): Windows resolves self with UNCHANGED_REFCOUNT, so a
    // FreeLibrary on it would drop a reference the shim never took. POSIX
    // dlclose(NULL) is undefined, so dl_close itself is not exercised here.
  }
}

// --- pipes and descriptor juggling -------------------------------------------

LSE_TEST(pipes_and_dups_carry_bytes) {
  int fds[2] = {-1, -1};
  LSE_EXPECT(plat::make_pipe(fds));
  if (fds[0] < 0 || fds[1] < 0) return;
  LSE_EXPECT_EQ(plat::write_fd(fds[1], "hello", 5), (plat::ssize_t)5);
  char buf[8] = {};
  // Pipes have no offsets: read through the sequential path.
  LSE_EXPECT_EQ(::read(fds[0], buf, 5), 5);
  LSE_EXPECT(std::memcmp(buf, "hello", 5) == 0);
  LSE_EXPECT(plat::close_fd(fds[0]));
  LSE_EXPECT(plat::close_fd(fds[1]));

  TempDir td("dup");
  const fs::path f1 = td / "one.txt";
  const fs::path f2 = td / "two.txt";
  const int w1 = plat::open_write_create(f1.string(), true, false);
  LSE_EXPECT_EQ(plat::write_fd(w1, "one", 3), (plat::ssize_t)3);
  LSE_EXPECT(plat::close_fd(w1));
  const int w2 = plat::open_write_create(f2.string(), true, false);
  LSE_EXPECT_EQ(plat::write_fd(w2, "two", 3), (plat::ssize_t)3);
  LSE_EXPECT(plat::close_fd(w2));

  const int r1 = plat::open_read(f1.string());
  LSE_EXPECT(r1 >= 0);
  const int d = plat::dup_fd(r1);
  LSE_EXPECT(d >= 0);
  if (d >= 0) {
    char b[3] = {};
    LSE_EXPECT_EQ(plat::pread64(d, b, 3, 0), (plat::ssize_t)3);
    LSE_EXPECT(std::memcmp(b, "one", 3) == 0);
    LSE_EXPECT(plat::close_fd(d));
  }
  // dup2 retargets an existing descriptor onto r1's file.
  const int r2 = plat::open_read(f2.string());
  LSE_EXPECT(r2 >= 0);
  LSE_EXPECT(plat::dup2_fd(r1, r2));
  char b2[3] = {};
  LSE_EXPECT_EQ(plat::pread64(r2, b2, 3, 0), (plat::ssize_t)3);
  LSE_EXPECT(std::memcmp(b2, "one", 3) == 0);  // now reads f1, not f2
  LSE_EXPECT(plat::close_fd(r1));
  LSE_EXPECT(plat::close_fd(r2));
}

// --- glob_match (fnmatch grammar, no FNM_PATHNAME) ---------------------------

LSE_TEST(glob_match_implements_the_fnmatch_grammar) {
  LSE_EXPECT(plat::glob_match("*", "anything/at/all"));  // '*' spans separators
  LSE_EXPECT(plat::glob_match("model*.safetensors", "model-00001.safetensors"));
  LSE_EXPECT(!plat::glob_match("*.safetensors", "model.bin"));
  LSE_EXPECT(plat::glob_match("a?c", "abc"));
  LSE_EXPECT(!plat::glob_match("a?c", "ac"));
  LSE_EXPECT(plat::glob_match("[a-c]x", "bx"));
  LSE_EXPECT(!plat::glob_match("[a-c]x", "dx"));
  LSE_EXPECT(plat::glob_match("literal", "literal"));
  LSE_EXPECT(!plat::glob_match("literal", "Literal"));
  LSE_EXPECT(plat::glob_match("*config*.json", "generation_config.json"));
}

// --- home_dir ----------------------------------------------------------------

LSE_TEST(home_dir_resolves_from_the_environment) {
  // HOME (or USERPROFILE on Windows) is set in every shell and CI runner that
  // runs ctest; an empty answer means the shim lost the fallback chain.
  const bool have_env =
      std::getenv("HOME") != nullptr ||
#ifdef _WIN32
      std::getenv("USERPROFILE") != nullptr ||
#endif
      false;
  const std::string home = plat::home_dir();
  if (have_env) {
    LSE_EXPECT(!home.empty());
    LSE_EXPECT(plat::is_dir(home));
  }
}

LSE_TEST_MAIN()
