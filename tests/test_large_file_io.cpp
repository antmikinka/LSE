// ID-1 >2 GiB acceptance (04-test-strategy §3, adopted unmodified; registered
// LARGE_IO, excluded from PR CI, weekly/dispatch + mandatory local M3 only).
//
// One sparse fixture of logical size 4 GiB + 1 MiB with four 1 MiB seeded-PRNG
// probe regions at the offsets where 32-bit file arithmetic breaks:
//   0                     — baseline
//   2^31 - 512 KiB        — signed-32 straddle
//   2^31 + 1 MiB          — past the truncation point
//   4 GiB - 1 MiB         — past the 2^32 wrap
// ~4 MiB of real data; < 30 s on NVMe. Plus a synthetic minimal safetensors
// whose data section starts beyond 2^31, loaded through the production
// SafeTensors::open path (exercises _fstat64 + CreateFileMappingW/MapViewOfFile
// + the streaming-fd pread together).
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "harness.hpp"

#include "lse/core/file_read.hpp"
#include "lse/core/platform.hpp"
#include "lse/core/sha256.hpp"
#include "lse/model/weights.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winioctl.h>  // FSCTL_SET_SPARSE
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;
using lse::platform::ssize_t;

namespace {

constexpr std::uint64_t kMiB = 1ull << 20;
constexpr std::uint64_t kGiB = 1ull << 30;
constexpr std::uint64_t kFileSize = 4 * kGiB + kMiB;  // 4296015872
constexpr std::int64_t kStraddle31 = std::int64_t{1} << 31;

const std::uint64_t kProbeOffsets[4] = {
    0,
    static_cast<std::uint64_t>(kStraddle31) - 512 * 1024,  // 2^31 - 512 KiB
    static_cast<std::uint64_t>(kStraddle31) + kMiB,        // 2^31 + 1 MiB
    4 * kGiB - kMiB,                                       // 4 GiB - 1 MiB
};

// splitmix64: the committed fixture is this generator plus the seeds below —
// identical bytes on every platform, no blob in the repo.
std::uint64_t splitmix64(std::uint64_t& s) {
  s += 0x9E3779B97F4A7C15ull;
  std::uint64_t z = s;
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}

void fill_probe(std::vector<std::byte>& buf, std::uint64_t seed) {
  std::uint64_t s = seed;
  auto* p = reinterpret_cast<std::uint64_t*>(buf.data());
  for (std::size_t i = 0; i < buf.size() / 8; ++i) {
    const std::uint64_t v = splitmix64(s);
    std::memcpy(p + i, &v, 8);
  }
}

std::string hash_bytes(const void* data, std::size_t n) {
  return lse::sha256(
      std::as_bytes(std::span(static_cast<const std::byte*>(data), n)));
}

fs::path fixture_dir() { return fs::temp_directory_path(); }

std::string fixture_path(const char* tag) {
  return (fixture_dir() /
          ("lse-large-io-" + std::string(tag) + "-" +
           std::to_string(lse::platform::pid()) + ".bin"))
      .string();
}

// 04 §3.4 guard: skip when the volume has < 8 GiB free (checked through the
// statvfs shim itself, G3) or cannot make the fixture sparse.
bool volume_usable(std::string& why) {
  const std::int64_t freeb =
      lse::platform::free_space_bytes(fixture_dir().string());
  if (freeb < 0) {
    why = "free_space_bytes failed";
    return false;
  }
  if (freeb < std::int64_t{8} * 1024 * 1024 * 1024) {
    why = "only " + std::to_string(freeb >> 30) + " GiB free (< 8 GiB)";
    return false;
  }
  return true;
}

#ifdef _WIN32
// Creates `path` as a sparse file of `size` logical bytes. Returns false with
// sparse_ok=false when the volume refuses FSCTL_SET_SPARSE (not NTFS/ReFS).
bool create_sparse(const std::string& path, std::uint64_t size, bool& sparse_ok,
                   std::vector<std::pair<std::uint64_t, const std::vector<std::byte>*>> writes) {
  sparse_ok = true;
  const fs::path p(path);
  HANDLE h = ::CreateFileW(p.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                           CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) return false;
  DWORD br = 0;
  if (!::DeviceIoControl(h, FSCTL_SET_SPARSE, nullptr, 0, nullptr, 0, &br,
                         nullptr)) {
    ::CloseHandle(h);
    sparse_ok = false;
    std::error_code ec;
    fs::remove(p, ec);
    return false;
  }
  LARGE_INTEGER sz{};
  sz.QuadPart = static_cast<LONGLONG>(size);
  if (!::SetFilePointerEx(h, sz, nullptr, FILE_BEGIN) || !::SetEndOfFile(h)) {
    ::CloseHandle(h);
    return false;
  }
  for (const auto& [off, data] : writes) {
    OVERLAPPED ov{};
    ov.Offset = static_cast<DWORD>(off & 0xFFFFFFFFull);
    ov.OffsetHigh = static_cast<DWORD>(off >> 32);
    DWORD done = 0;
    if (!::WriteFile(h, data->data(), static_cast<DWORD>(data->size()), &done,
                     &ov) ||
        done != data->size()) {
      ::CloseHandle(h);
      return false;
    }
  }
  ::FlushFileBuffers(h);
  ::CloseHandle(h);
  return true;
}
#else
bool create_sparse(const std::string& path, std::uint64_t size, bool& sparse_ok,
                   std::vector<std::pair<std::uint64_t, const std::vector<std::byte>*>> writes) {
  sparse_ok = true;  // ftruncate holes are sparse on every CI/dev filesystem
  const int fd = ::open(path.c_str(), O_CREAT | O_RDWR | O_EXCL, 0644);
  if (fd < 0) return false;
  if (::ftruncate(fd, static_cast<off_t>(size)) != 0) {
    ::close(fd);
    return false;
  }
  for (const auto& w : writes) {
    std::uint64_t off = w.first;
    const std::vector<std::byte>* data = w.second;
    const auto* p = data->data();
    std::size_t left = data->size();
    while (left > 0) {
      const auto n = ::pwrite(fd, p, left, static_cast<off_t>(off));
      if (n <= 0) {
        ::close(fd);
        return false;
      }
      p += n;
      left -= static_cast<std::size_t>(n);
      off += static_cast<std::uint64_t>(n);
    }
  }
  ::close(fd);
  return true;
}
#endif

void remove_quietly(const std::string& path) {
  std::error_code ec;
  fs::remove(path, ec);
}

}  // namespace

LSE_TEST(a_four_gib_file_reads_back_exactly_through_the_production_path) {
  std::string why;
  if (!volume_usable(why)) {
    LSE_SKIP(("volume guard: " + why).c_str());
  }
  const std::string path = fixture_path("main");
  remove_quietly(path);

  // Expected probe contents, generated identically on every platform.
  std::vector<std::vector<std::byte>> probes(4);
  std::vector<std::pair<std::uint64_t, const std::vector<std::byte>*>> writes;
  for (std::size_t i = 0; i < 4; ++i) {
    probes[i].resize(static_cast<std::size_t>(kMiB));
    fill_probe(probes[i], 0x5EED0000ull + i);
    writes.emplace_back(kProbeOffsets[i], &probes[i]);
  }

  bool sparse_ok = false;
  const auto t0 = std::chrono::steady_clock::now();
  if (!create_sparse(path, kFileSize, sparse_ok, writes)) {
    remove_quietly(path);
    if (!sparse_ok) {
      LSE_SKIP("volume does not support sparse files (FSCTL_SET_SPARSE refused)");
    }
    LSE_EXPECT(false && "sparse fixture creation failed");
    return;
  }

  // 1. stat size is the exact logical size — catches _stat64i32 / 32-bit off_t.
  LSE_EXPECT_EQ(lse::platform::path_size(path), static_cast<std::int64_t>(kFileSize));

  // 2. The production read path (open_for_streaming + read_file_range, chunked
  //    at 1<<30) returns every probe byte-exactly.
  auto opened = lse::open_for_streaming(path);
  LSE_EXPECT_OK(opened.status());
  if (!opened.ok()) {
    remove_quietly(path);
    return;
  }
  const int fd = *opened;
  std::vector<std::byte> buf(static_cast<std::size_t>(kMiB));
  for (std::size_t i = 0; i < 4; ++i) {
    const auto st = lse::read_file_range(fd, kProbeOffsets[i], buf.data(),
                                         buf.size(), "probe");
    LSE_EXPECT_OK(st);
    if (!st.ok()) continue;
    const std::string got = hash_bytes(buf.data(), buf.size());
    const std::string want = hash_bytes(probes[i].data(), probes[i].size());
    std::printf("       probe %zu @ %llu: %s\n", i,
                static_cast<unsigned long long>(kProbeOffsets[i]), got.c_str());
    LSE_EXPECT(got == want);
  }

  // 3. One call spanning [2^31-512KiB, 2^31+512KiB] — the exact point where a
  //    static_cast<off_t> truncates; it must equal probe 1 byte-for-byte.
  {
    const std::uint64_t off =
        static_cast<std::uint64_t>(kStraddle31) - 512 * 1024;
    const auto st =
        lse::read_file_range(fd, off, buf.data(), buf.size(), "straddle");
    LSE_EXPECT_OK(st);
    if (st.ok()) {
      LSE_EXPECT(hash_bytes(buf.data(), buf.size()) ==
                 hash_bytes(probes[1].data(), probes[1].size()));
    }
  }

  // 4. A short read at EOF is kIoError, not garbage.
  {
    std::vector<std::byte> tail(512 * 1024);
    const auto st = lse::read_file_range(fd, kFileSize - 1024, tail.data(),
                                         tail.size(), "eof");
    LSE_EXPECT(!st.ok());
    LSE_EXPECT(st.code() == lse::StatusCode::kIoError);
  }
  lse::platform::close_fd(fd);  // before remove: Windows deletes only closed files
  remove_quietly(path);
  LSE_EXPECT(!fs::exists(path));

  const double secs =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - t0)
          .count();
  std::printf("       4 GiB sparse fixture + probes: %.1f s\n", secs);
}

LSE_TEST(safetensors_with_its_data_past_2gib_loads_and_hashes) {
  std::string why;
  if (!volume_usable(why)) {
    LSE_SKIP(("volume guard: " + why).c_str());
  }
  const std::string path = fixture_path("st");
  remove_quietly(path);

  // Minimal valid safetensors: u64-LE header length + JSON header for one F32
  // [16,64] tensor whose data section begins beyond 2^31.
  const std::uint64_t data_begin = static_cast<std::uint64_t>(kStraddle31);
  constexpr std::uint64_t kFloats = 16 * 64;
  constexpr std::uint64_t kBytes = kFloats * 4;
  const std::string json =
      "{\"probe.f32\":{\"dtype\":\"F32\",\"shape\":[16,64],\"data_offsets\":[" +
      std::to_string(data_begin) + "," + std::to_string(data_begin + kBytes) +
      "]}}";
  std::vector<std::byte> header;
  const std::uint64_t hlen = json.size();
  header.resize(8);
  std::memcpy(header.data(), &hlen, 8);
  header.insert(header.end(),
                reinterpret_cast<const std::byte*>(json.data()),
                reinterpret_cast<const std::byte*>(json.data()) + json.size());
  const std::uint64_t file_size = header.size() + data_begin + kBytes;

  std::vector<std::byte> data(static_cast<std::size_t>(kBytes));
  {
    std::uint64_t s = 0x5AFE7E95ull;
    auto* p = reinterpret_cast<std::uint32_t*>(data.data());
    for (std::size_t i = 0; i < kFloats; ++i) {
      const std::uint32_t v = static_cast<std::uint32_t>(splitmix64(s));
      std::memcpy(p + i, &v, 4);
    }
  }

  bool sparse_ok = false;
  const std::vector<std::pair<std::uint64_t, const std::vector<std::byte>*>>
      writes{{0, &header}, {header.size() + data_begin, &data}};
  if (!create_sparse(path, file_size, sparse_ok, writes)) {
    remove_quietly(path);
    if (!sparse_ok) {
      LSE_SKIP("volume does not support sparse files (FSCTL_SET_SPARSE refused)");
    }
    LSE_EXPECT(false && "safetensors fixture creation failed");
    return;
  }
  LSE_EXPECT_EQ(lse::platform::path_size(path),
                static_cast<std::int64_t>(file_size));

  {
    auto opened = lse::model::SafeTensors::open(path);
    LSE_EXPECT_OK(opened.status());
    if (opened.ok()) {
      auto reader = std::move(opened).release();
      const auto* tv = reader.find("probe.f32");
      LSE_EXPECT(tv != nullptr);
      if (tv != nullptr) {
        LSE_EXPECT(tv->dtype == lse::DType::kF32);
        LSE_EXPECT_EQ(tv->element_count(), static_cast<std::size_t>(kFloats));
        // The loader resolved the offset past 2^31 (no 32-bit truncation).
        LSE_EXPECT_EQ(tv->file_offset, header.size() + data_begin);
        std::vector<float> got(static_cast<std::size_t>(kFloats), 0.0f);
        const auto st = tv->read_f32(got.data(), got.size());
        LSE_EXPECT_OK(st);
        if (st.ok()) {
          LSE_EXPECT(hash_bytes(got.data(), got.size() * 4) ==
                     hash_bytes(data.data(), data.size()));
        }
      }
    }
  }  // reader destructs here: unmaps and closes its fd before the remove
  remove_quietly(path);
  LSE_EXPECT(!fs::exists(path));
}

LSE_TEST_MAIN()
