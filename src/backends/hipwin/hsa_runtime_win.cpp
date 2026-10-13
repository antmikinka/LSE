// Windows HSA runtime. The engine looks these symbols up by name
// (hsa_init, hsa_iterate_agents, the hsa_amd_* pool and copy calls). ROCr
// does not ship on Windows, so this DLL answers that ABI from the HIP
// runtime that does: one CPU agent and one GPU agent, the GPU being
// whatever hipGetDeviceProperties reports.
//
// Attribute widths match the buffers hrx_backend.cpp actually passes.
// An attribute this runtime cannot measure returns an error. It does not
// invent a queue count or a timestamp frequency. Kernel launch is the one
// host submission path: hipModuleLoadData plus hipModuleLaunchKernel.
// hsa_amd_launch_kernel is that submission. It is not a second HRX ABI.

#include <windows.h>

#include <hip/hip_runtime_api.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <new>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr int kSuccess = 0;
constexpr int kInvalid = 0x1001;

constexpr int kInfoName = 0;
constexpr int kInfoWavefront = 6;
constexpr int kInfoWorkgroupMax = 8;
constexpr int kInfoQueuesMax = 12;
constexpr int kInfoNode = 16;
constexpr int kInfoDevice = 17;
constexpr int kInfoCuCount = 0xA002;
constexpr int kInfoBdf = 0xA006;
constexpr int kInfoDomain = 0xA00F;
constexpr int kInfoUuid = 0xA011;
constexpr int kInfoMemAvail = 0xA015;
constexpr int kInfoTimestampFreq = 0xA016;

constexpr int kDeviceCpu = 0;
constexpr int kDeviceGpu = 1;
constexpr std::uint64_t kCpuAgent = 1;
constexpr std::uint64_t kGpuAgent = 2;
constexpr std::uint64_t kGpuPool = 1;

constexpr int kPoolSegment = 0;
constexpr int kPoolGlobalFlags = 1;
constexpr std::uint32_t kSegmentGlobal = 0;
constexpr std::uint32_t kFlagCoarse = 4;
constexpr int kAccessNever = 0;
constexpr int kAccessAllowed = 1;

struct Agent {
  std::uint64_t handle;
};
struct Pool {
  std::uint64_t handle;
};
struct Signal {
  std::uint64_t handle;
};

struct SignalObject {
  std::atomic<std::int64_t> value;
};

std::mutex g_mu;
int g_init = 0;
bool g_gpu = false;
hipDeviceProp_t g_prop{};

// HIP's multiprocessor on RDNA is the WGP. Two CUs share that WGP.
// DeviceInfo in backend.hpp records the same pairing. The HSA attribute
// is a CU count, so gfx11/gfx12 report twice the multiprocessor count.
std::uint32_t compute_units() {
  const std::uint32_t mp = static_cast<std::uint32_t>(g_prop.multiProcessorCount);
  const char* arch = g_prop.gcnArchName;
  if (std::strncmp(arch, "gfx11", 5) == 0 || std::strncmp(arch, "gfx12", 5) == 0) {
    return mp * 2u;
  }
  return mp;
}

bool ensure_device() {
  if (g_gpu) return true;
  int count = 0;
  if (hipGetDeviceCount(&count) != hipSuccess || count < 1) return false;
  if (hipSetDevice(0) != hipSuccess) return false;
  if (hipGetDeviceProperties(&g_prop, 0) != hipSuccess) return false;
  g_gpu = true;
  return true;
}

void write_bytes(void* out, const void* src, std::size_t n) {
  std::memcpy(out, src, n);
}

void write_name(void* out, const char* text) {
  char buf[64] = {};
  if (text != nullptr) {
    std::strncpy(buf, text, sizeof(buf) - 1);
  }
  write_bytes(out, buf, sizeof(buf));
}

std::mutex g_error_mu;
std::string g_last_error;
int g_locks = 0;
int g_loads_ok = 0;
int g_launches_ok = 0;
std::uint64_t g_lock_bytes = 0;
char g_last_launch[256] = {};
std::vector<unsigned char> g_last_good;

void set_error(const char* text) {
  std::lock_guard lock(g_error_mu);
  g_last_error = text != nullptr ? text : "";
}

std::uint32_t read_u32(const unsigned char* p) {
  std::uint32_t v = 0;
  std::memcpy(&v, p, sizeof(v));
  return v;
}

std::uint64_t read_u64(const unsigned char* p) {
  std::uint64_t v = 0;
  std::memcpy(&v, p, sizeof(v));
  return v;
}

std::uint16_t read_u16(const unsigned char* p) {
  std::uint16_t v = 0;
  std::memcpy(&v, p, sizeof(v));
  return v;
}

bool is_elf64(const unsigned char* p, std::size_t n) {
  return n >= 64 && p[0] == 0x7f && p[1] == 'E' && p[2] == 'L' && p[3] == 'F' && p[4] == 2;
}

// A clang offload bundle lists its images after a 24-byte magic and a count.
// hipModuleLoadData accepts the whole bundle. Name parsing needs the ELF image.
bool locate_elf(const std::vector<unsigned char>& bytes, const unsigned char** elf,
                std::size_t* elf_size) {
  if (is_elf64(bytes.data(), bytes.size())) {
    *elf = bytes.data();
    *elf_size = bytes.size();
    return true;
  }
  constexpr char kMagic[] = "__CLANG_OFFLOAD_BUNDLE__";
  if (bytes.size() < 32 || std::memcmp(bytes.data(), kMagic, 24) != 0) return false;
  const std::uint64_t count = read_u64(bytes.data() + 24);
  if (count == 0 || count > 64) return false;
  std::size_t cursor = 32;
  struct Entry {
    std::uint64_t offset;
    std::uint64_t size;
  };
  std::vector<Entry> entries;
  entries.reserve(static_cast<std::size_t>(count));
  for (std::uint64_t i = 0; i < count; ++i) {
    if (cursor + 24 > bytes.size()) return false;
    const std::uint64_t offset = read_u64(bytes.data() + cursor);
    const std::uint64_t size = read_u64(bytes.data() + cursor + 8);
    const std::uint64_t id_len = read_u64(bytes.data() + cursor + 16);
    cursor += 24;
    if (id_len > bytes.size() - cursor) return false;
    cursor += static_cast<std::size_t>(id_len);
    entries.push_back(Entry{offset, size});
  }
  for (const Entry& entry : entries) {
    if (entry.size < 64 || entry.offset > bytes.size() ||
        entry.size > bytes.size() - entry.offset) {
      continue;
    }
    const unsigned char* image = bytes.data() + entry.offset;
    if (is_elf64(image, static_cast<std::size_t>(entry.size))) {
      *elf = image;
      *elf_size = static_cast<std::size_t>(entry.size);
      return true;
    }
  }
  return false;
}

bool next_token(const unsigned char* note, std::size_t size, std::size_t pos, std::size_t* next) {
  if (pos >= size) return false;
  const unsigned char tag = note[pos];
  std::size_t header = 1;
  std::size_t payload = 0;
  std::size_t length_bytes = 0;
  bool sized = false;
  if (tag >= 0xa0 && tag <= 0xbf) {
    payload = tag & 0x1fu;
  } else if (tag < 0xa0 || tag >= 0xe0 || tag == 0xc0 || tag == 0xc2 || tag == 0xc3) {
  } else if ((tag >= 0xcc && tag <= 0xcf) || (tag >= 0xd0 && tag <= 0xd3)) {
    payload = std::size_t{1} << (tag & 3u);
  } else if (tag == 0xca || tag == 0xcb) {
    payload = tag == 0xca ? 4 : 8;
  } else if (tag >= 0xd4 && tag <= 0xd8) {
    payload = 1 + (std::size_t{1} << (tag - 0xd4));
  } else if ((tag >= 0xc4 && tag <= 0xc6) || (tag >= 0xc7 && tag <= 0xc9) ||
             (tag >= 0xd9 && tag <= 0xdb)) {
    const unsigned char first = tag <= 0xc6 ? 0xc4 : tag <= 0xc9 ? 0xc7 : 0xd9;
    length_bytes = std::size_t{1} << (tag - first);
    sized = true;
    if (tag >= 0xc7 && tag <= 0xc9) ++header;
  } else if (tag >= 0xdc && tag <= 0xdf) {
    length_bytes = (tag & 1u) == 0 ? 2 : 4;
  } else {
    return false;
  }
  if (length_bytes > size - pos - 1) return false;
  if (sized) {
    for (std::size_t i = 1; i <= length_bytes; ++i) {
      payload = (payload << 8) | note[pos + i];
    }
  }
  header += length_bytes;
  if (header > size - pos || payload > size - pos - header) return false;
  *next = pos + header + payload;
  return true;
}

void add_name(std::vector<std::string>& names, std::string name) {
  if (name.size() > 3 && name.compare(name.size() - 3, 3, ".kd") == 0) {
    name.resize(name.size() - 3);
  }
  if (name.empty() || name[0] == '.') return;
  for (const std::string& have : names) {
    if (have == name) return;
  }
  names.push_back(std::move(name));
}

void names_from_note(const unsigned char* note, std::size_t size, std::vector<std::string>& names) {
  constexpr char kKey[] = ".symbol";
  constexpr unsigned char kTag = 0xa0 | 7;
  for (std::size_t pos = 0; pos < size;) {
    if (pos + 8 <= size && note[pos] == kTag && std::memcmp(note + pos + 1, kKey, 7) == 0) {
      const std::size_t value = pos + 8;
      std::size_t len = 0;
      std::size_t data = 0;
      bool have = false;
      if (value < size) {
        const unsigned char tag = note[value];
        // Fixstr is at most 31 bytes. "lse_loom_" plus a 20-digit id plus
        // ".kd" is 32 bytes, which the note encodes as str8.
        if (tag >= 0xa0 && tag <= 0xbf) {
          len = tag & 0x1fu;
          data = value + 1;
          have = data <= size && len <= size - data;
        } else if (tag == 0xd9 && value + 2 <= size) {
          len = note[value + 1];
          data = value + 2;
          have = len <= size - data;
        } else if (tag == 0xda && value + 3 <= size) {
          len = (std::size_t{note[value + 1]} << 8) | note[value + 2];
          data = value + 3;
          have = len <= size - data;
        } else if (tag == 0xdb && value + 5 <= size) {
          len = (std::size_t{note[value + 1]} << 24) | (std::size_t{note[value + 2]} << 16) |
                (std::size_t{note[value + 3]} << 8) | note[value + 4];
          data = value + 5;
          have = len <= size - data;
        }
      }
      if (have) {
        add_name(names, std::string(reinterpret_cast<const char*>(note + data), len));
      }
    }
    if (!next_token(note, size, pos, &pos)) break;
  }
}

bool note_bytes(const unsigned char* obj, std::size_t size, std::vector<unsigned char>* out) {
  if (!is_elf64(obj, size)) return false;
  const std::size_t shoff = static_cast<std::size_t>(read_u64(obj + 0x28));
  const std::uint16_t shentsize = read_u16(obj + 0x3A);
  const std::uint16_t shnum = read_u16(obj + 0x3C);
  const std::uint16_t shstrndx = read_u16(obj + 0x3E);
  if (shoff == 0 || shentsize < 64 || shnum == 0 || shnum > 4096) return false;
  if (shoff + static_cast<std::size_t>(shnum) * shentsize > size) return false;
  const unsigned char* sh = obj + shoff;
  if (shstrndx >= shnum) return false;
  const unsigned char* str_hdr = sh + static_cast<std::size_t>(shstrndx) * shentsize;
  const std::size_t names_off = static_cast<std::size_t>(read_u64(str_hdr + 0x18));
  const std::size_t names_size = static_cast<std::size_t>(read_u64(str_hdr + 0x20));
  if (names_off + names_size > size) return false;
  for (std::uint16_t i = 0; i < shnum; ++i) {
    const unsigned char* entry = sh + static_cast<std::size_t>(i) * shentsize;
    const std::uint32_t name_off = read_u32(entry);
    if (name_off >= names_size) continue;
    const char* name = reinterpret_cast<const char*>(obj + names_off + name_off);
    if (std::strcmp(name, ".note") != 0) continue;
    const std::size_t note_off = static_cast<std::size_t>(read_u64(entry + 0x18));
    const std::size_t note_size = static_cast<std::size_t>(read_u64(entry + 0x20));
    if (note_off + note_size > size) continue;
    std::size_t p = note_off;
    while (p + 12 <= note_off + note_size) {
      const std::uint32_t namesz = read_u32(obj + p);
      const std::uint32_t descsz = read_u32(obj + p + 4);
      p += 12;
      const std::uint32_t padded = (namesz + 3u) & ~3u;
      if (p + padded + descsz > note_off + note_size) break;
      if (namesz >= 7 && std::memcmp(obj + p, "AMDGPU", 6) == 0) {
        out->assign(obj + p + padded, obj + p + padded + descsz);
        return true;
      }
      p += padded + descsz;
    }
  }
  return false;
}

void names_from_symbols(const unsigned char* obj, std::size_t size, std::vector<std::string>& names) {
  if (!is_elf64(obj, size)) return;
  const std::size_t shoff = static_cast<std::size_t>(read_u64(obj + 0x28));
  const std::uint16_t shentsize = read_u16(obj + 0x3A);
  const std::uint16_t shnum = read_u16(obj + 0x3C);
  if (shoff == 0 || shentsize < 64 || shnum == 0 || shnum > 4096) return;
  if (shoff + static_cast<std::size_t>(shnum) * shentsize > size) return;
  const unsigned char* sh = obj + shoff;
  for (std::uint16_t i = 0; i < shnum; ++i) {
    const unsigned char* entry = sh + static_cast<std::size_t>(i) * shentsize;
    const std::uint32_t type = read_u32(entry + 4);
    if (type != 2 && type != 11) continue;  // SHT_SYMTAB, SHT_DYNSYM
    const std::size_t off = static_cast<std::size_t>(read_u64(entry + 0x18));
    const std::size_t bytes = static_cast<std::size_t>(read_u64(entry + 0x20));
    const std::uint32_t link = read_u32(entry + 0x28);
    std::size_t entsize = static_cast<std::size_t>(read_u64(entry + 0x38));
    if (entsize == 0) entsize = 24;
    if (link >= shnum || off + bytes > size || entsize < 24) continue;
    const unsigned char* str_hdr = sh + static_cast<std::size_t>(link) * shentsize;
    const std::size_t str_off = static_cast<std::size_t>(read_u64(str_hdr + 0x18));
    const std::size_t str_size = static_cast<std::size_t>(read_u64(str_hdr + 0x20));
    if (str_off + str_size > size) continue;
    for (std::size_t at = 0; at + entsize <= bytes; at += entsize) {
      const unsigned char* sym = obj + off + at;
      const std::uint32_t name_off = read_u32(sym);
      const unsigned char info = sym[4];
      const unsigned char kind = info & 0x0fu;
      const std::uint16_t shndx = read_u16(sym + 6);
      if (shndx == 0 || name_off >= str_size) continue;
      if (kind != 2 && kind != 10) continue;  // STT_FUNC or the AMDGPU kernel type
      const char* text = reinterpret_cast<const char*>(obj + str_off + name_off);
      const std::size_t room = str_size - name_off;
      const std::size_t len = ::strnlen(text, room);
      if (len == 0 || len == room) continue;
      add_name(names, std::string(text, len));
    }
  }
}

void kernel_names(const unsigned char* elf, std::size_t size, std::vector<std::string>& names) {
  std::vector<unsigned char> note;
  if (note_bytes(elf, size, &note)) names_from_note(note.data(), note.size(), names);
  if (names.empty()) names_from_symbols(elf, size, names);
}

struct CodeReader {
  std::vector<unsigned char> bytes;
};

struct LoadedExecutable;

struct KernelSymbol {
  LoadedExecutable* owner = nullptr;
  hipFunction_t function = nullptr;
  std::string name;
};

struct LoadedExecutable {
  bool frozen = false;
  hipModule_t module = nullptr;
  std::vector<KernelSymbol*> symbols;
};

}  // namespace

extern "C" {

__declspec(dllexport) int hsa_init(void) {
  std::lock_guard lock(g_mu);
  if (g_init > 0) {
    ++g_init;
    return kSuccess;
  }
  if (!ensure_device()) return kInvalid;
  g_init = 1;
  return kSuccess;
}

__declspec(dllexport) int hsa_iterate_agents(int (*cb)(Agent, void*), void* data) {
  if (cb == nullptr) return kInvalid;
  {
    // Released before the callback. Callers ask for agent info from inside
    // the callback, and that path takes the same lock.
    std::lock_guard lock(g_mu);
    if (!ensure_device()) return kInvalid;
  }
  const Agent cpu{kCpuAgent};
  const int cpu_st = cb(cpu, data);
  if (cpu_st != kSuccess) return cpu_st;
  const Agent gpu{kGpuAgent};
  return cb(gpu, data);
}

__declspec(dllexport) int hsa_agent_get_info(Agent agent, int attr, void* out) {
  if (out == nullptr) return kInvalid;
  std::lock_guard lock(g_mu);
  if (!ensure_device()) return kInvalid;
  if (agent.handle == kCpuAgent) {
    if (attr == kInfoDevice) {
      const int type = kDeviceCpu;
      write_bytes(out, &type, sizeof(type));
      return kSuccess;
    }
    if (attr == kInfoName) {
      write_name(out, "CPU");
      return kSuccess;
    }
    if (attr == kInfoNode) {
      const std::uint32_t node = 0;
      write_bytes(out, &node, sizeof(node));
      return kSuccess;
    }
    return kInvalid;
  }
  if (agent.handle != kGpuAgent) return kInvalid;

  switch (attr) {
    case kInfoName:
      write_name(out, g_prop.gcnArchName);
      return kSuccess;
    case kInfoDevice: {
      const int type = kDeviceGpu;
      write_bytes(out, &type, sizeof(type));
      return kSuccess;
    }
    case kInfoNode: {
      const std::uint32_t node = 1;
      write_bytes(out, &node, sizeof(node));
      return kSuccess;
    }
    case kInfoWavefront: {
      const std::uint32_t wave = static_cast<std::uint32_t>(g_prop.warpSize);
      write_bytes(out, &wave, sizeof(wave));
      return kSuccess;
    }
    case kInfoWorkgroupMax: {
      const std::uint32_t threads = static_cast<std::uint32_t>(g_prop.maxThreadsPerBlock);
      write_bytes(out, &threads, sizeof(threads));
      return kSuccess;
    }
    case kInfoCuCount: {
      const std::uint32_t cus = compute_units();
      write_bytes(out, &cus, sizeof(cus));
      return kSuccess;
    }
    case kInfoBdf: {
      const std::uint32_t bdf =
          (static_cast<std::uint32_t>(g_prop.pciBusID) << 8) |
          (static_cast<std::uint32_t>(g_prop.pciDeviceID) << 3);
      write_bytes(out, &bdf, sizeof(bdf));
      return kSuccess;
    }
    case kInfoDomain: {
      const std::uint32_t domain = static_cast<std::uint32_t>(g_prop.pciDomainID);
      write_bytes(out, &domain, sizeof(domain));
      return kSuccess;
    }
    case kInfoUuid: {
      hipUUID uuid{};
      if (hipDeviceGetUuid(&uuid, 0) != hipSuccess) return kInvalid;
      char text[64] = {};
      bool ascii = true;
      for (int i = 0; i < 16; ++i) {
        const auto b = static_cast<unsigned char>(uuid.bytes[i]);
        if (b < 0x20 || b > 0x7e) ascii = false;
      }
      if (ascii) {
        std::memcpy(text, uuid.bytes, 16);
      } else {
        static constexpr char kHex[] = "0123456789abcdef";
        for (int i = 0; i < 16; ++i) {
          const auto b = static_cast<unsigned char>(uuid.bytes[i]);
          text[i * 2] = kHex[b >> 4];
          text[i * 2 + 1] = kHex[b & 0xf];
        }
      }
      write_bytes(out, text, sizeof(text));
      return kSuccess;
    }
    case kInfoMemAvail: {
      std::size_t free_b = 0;
      std::size_t total_b = 0;
      if (hipMemGetInfo(&free_b, &total_b) != hipSuccess) return kInvalid;
      const std::uint64_t free64 = static_cast<std::uint64_t>(free_b);
      write_bytes(out, &free64, sizeof(free64));
      return kSuccess;
    }
    case kInfoQueuesMax:
    case kInfoTimestampFreq:
      return kInvalid;
    default:
      return kInvalid;
  }
}

__declspec(dllexport) int hsa_amd_agent_iterate_memory_pools(
    Agent agent, int (*cb)(Pool, void*), void* data) {
  if (cb == nullptr) return kInvalid;
  if (agent.handle != kGpuAgent) return kSuccess;
  const Pool pool{kGpuPool};
  return cb(pool, data);
}

__declspec(dllexport) int hsa_amd_memory_pool_get_info(Pool pool, int attr, void* out) {
  if (out == nullptr || pool.handle != kGpuPool) return kInvalid;
  if (attr == kPoolSegment) {
    const std::uint32_t segment = kSegmentGlobal;
    write_bytes(out, &segment, sizeof(segment));
    return kSuccess;
  }
  if (attr == kPoolGlobalFlags) {
    const std::uint32_t flags = kFlagCoarse;
    write_bytes(out, &flags, sizeof(flags));
    return kSuccess;
  }
  return kInvalid;
}

__declspec(dllexport) int hsa_amd_agent_memory_pool_get_info(Agent agent, Pool pool, int attr,
                                                            void* out) {
  if (out == nullptr || attr != 0 || pool.handle != kGpuPool) return kInvalid;
  const int access = agent.handle == kGpuAgent ? kAccessAllowed : kAccessNever;
  write_bytes(out, &access, sizeof(access));
  return kSuccess;
}

__declspec(dllexport) int hsa_amd_agents_allow_access(std::uint32_t, const Agent*,
                                                     const std::uint32_t*, const void*) {
  // One GPU, and its memory is the HIP allocation itself. There is no second
  // agent to grant.
  return kSuccess;
}

__declspec(dllexport) int hsa_amd_memory_async_copy(void* dst, Agent, const void* src, Agent,
                                                   std::size_t bytes, std::uint32_t,
                                                   const Signal*, Signal completion) {
  if (dst == nullptr || src == nullptr || bytes == 0) return kInvalid;
  {
    std::lock_guard lock(g_mu);
    if (!ensure_device()) return kInvalid;
  }
  const hipError_t copied = hipMemcpy(dst, src, bytes, hipMemcpyDefault);
  if (copied != hipSuccess) {
    const char* text = hipGetErrorString(copied);
    set_error(text);
    return kInvalid;
  }
  if (completion.handle != 0) {
    auto* signal = reinterpret_cast<SignalObject*>(completion.handle);
    signal->value.store(0, std::memory_order_release);
  }
  return kSuccess;
}

__declspec(dllexport) int hsa_amd_memory_lock(void* host, std::size_t bytes, Agent*, int,
                                             void** locked) {
  if (host == nullptr || locked == nullptr) return kInvalid;
  // Mapped registration is what a later kernel launch translates with
  // hipHostGetDevicePointer. The pointer published here stays the host
  // address: callers map that pointer from the CPU.
  hipError_t registered = hipHostRegister(host, bytes, hipHostRegisterMapped);
  if (registered != hipSuccess) {
    (void)hipGetLastError();
    registered = hipHostRegister(host, bytes, hipHostRegisterDefault);
  }
  if (registered != hipSuccess) {
    set_error(hipGetErrorString(registered));
    return kInvalid;
  }
  ++g_locks;
  g_lock_bytes += bytes;
  *locked = host;
  return kSuccess;
}

// The pointer published by hsa_amd_memory_lock stays the host address. A
// pointer table the GPU will dereference needs the translated device address.
__declspec(dllexport) int hsa_amd_get_device_pointer(void** device, void* host) {
  if (device == nullptr || host == nullptr) return kInvalid;
  {
    std::lock_guard lock(g_mu);
    if (!ensure_device()) return kInvalid;
  }
  void* translated = nullptr;
  const hipError_t got = hipHostGetDevicePointer(&translated, host, 0);
  if (got != hipSuccess || translated == nullptr) {
    set_error(got == hipSuccess ? "hipHostGetDevicePointer returned a null device pointer"
                                : hipGetErrorString(got));
    return kInvalid;
  }
  *device = translated;
  return kSuccess;
}

__declspec(dllexport) int hsa_amd_memory_unlock(void* host) {
  if (host == nullptr) return kInvalid;
  if (hipHostUnregister(host) != hipSuccess) return kInvalid;
  return kSuccess;
}

__declspec(dllexport) int hsa_signal_create(std::int64_t initial, std::uint32_t, const Agent*,
                                           Signal* out) {
  if (out == nullptr) return kInvalid;
  auto* signal = new (std::nothrow) SignalObject;
  if (signal == nullptr) return kInvalid;
  signal->value.store(initial, std::memory_order_relaxed);
  out->handle = reinterpret_cast<std::uint64_t>(signal);
  return kSuccess;
}

__declspec(dllexport) int hsa_signal_destroy(Signal signal) {
  if (signal.handle == 0) return kInvalid;
  delete reinterpret_cast<SignalObject*>(signal.handle);
  return kSuccess;
}

__declspec(dllexport) void hsa_signal_store_screlease(Signal signal, std::int64_t value) {
  if (signal.handle == 0) return;
  reinterpret_cast<SignalObject*>(signal.handle)->value.store(value, std::memory_order_release);
}

__declspec(dllexport) std::int64_t hsa_signal_wait_scacquire(Signal signal, int condition,
                                                           std::int64_t compare,
                                                           std::uint64_t timeout, int) {
  if (signal.handle == 0) return -1;
  auto* object = reinterpret_cast<SignalObject*>(signal.handle);
  const bool forever = timeout == UINT64_MAX;
  for (;;) {
    const std::int64_t value = object->value.load(std::memory_order_acquire);
    bool met = false;
    switch (condition) {
      case 0: met = value == compare; break;
      case 1: met = value != compare; break;
      case 2: met = value < compare; break;
      case 3: met = value >= compare; break;
      default: return value;
    }
    if (met) return value;
    if (!forever) return value;
    std::this_thread::yield();
  }
}

__declspec(dllexport) const char* hsa_amd_last_error(void) {
  std::lock_guard lock(g_error_mu);
  return g_last_error.c_str();
}

__declspec(dllexport) int hsa_code_object_reader_create_from_memory(const void* data, std::size_t size,
                                                                   std::uint64_t* reader) {
  if (data == nullptr || reader == nullptr || size < 64) return kInvalid;
  auto* created = new (std::nothrow) CodeReader;
  if (created == nullptr) return kInvalid;
  const auto* bytes = static_cast<const unsigned char*>(data);
  created->bytes.assign(bytes, bytes + size);
  const unsigned char* elf = nullptr;
  std::size_t elf_size = 0;
  if (!locate_elf(created->bytes, &elf, &elf_size)) {
    delete created;
    set_error("code object is not an AMDGPU ELF image");
    return kInvalid;
  }
  const std::uint16_t machine = read_u16(elf + 18);
  constexpr std::uint16_t kAmdgpu = 224;
  if (machine != kAmdgpu) {
    delete created;
    set_error("code object is not an AMDGPU ELF image");
    return kInvalid;
  }
  *reader = reinterpret_cast<std::uint64_t>(created);
  return kSuccess;
}

__declspec(dllexport) int hsa_code_object_reader_destroy(std::uint64_t reader) {
  if (reader == 0) return kInvalid;
  delete reinterpret_cast<CodeReader*>(reader);
  return kSuccess;
}

__declspec(dllexport) int hsa_executable_create(std::uint64_t* executable) {
  if (executable == nullptr) return kInvalid;
  auto* created = new (std::nothrow) LoadedExecutable;
  if (created == nullptr) return kInvalid;
  *executable = reinterpret_cast<std::uint64_t>(created);
  return kSuccess;
}

__declspec(dllexport) int hsa_executable_load_agent_code_object(std::uint64_t executable, Agent agent,
                                                               std::uint64_t reader, const char*,
                                                               std::uint64_t* loaded) {
  if (executable == 0 || reader == 0 || agent.handle != kGpuAgent) return kInvalid;
  auto* image = reinterpret_cast<LoadedExecutable*>(executable);
  auto* source = reinterpret_cast<CodeReader*>(reader);
  if (image->module != nullptr) return kInvalid;
  {
    std::lock_guard lock(g_mu);
    if (!ensure_device()) return kInvalid;
  }
  const unsigned char* elf = nullptr;
  std::size_t elf_size = 0;
  if (!locate_elf(source->bytes, &elf, &elf_size)) {
    set_error("code object is not an AMDGPU ELF image");
    return kInvalid;
  }
  std::vector<std::string> names;
  kernel_names(elf, elf_size, names);
  if (names.empty()) {
    set_error("code object advertises no kernels");
    return kInvalid;
  }
  // The image is handed to the loader as its own reservation, with a zero
  // tail. A heap vector of this exact length sits against the next
  // allocation, and a loader that reads past the ELF then faults the copy
  // kernel. hipDeviceReset does not revive a later hipModuleLoadData on this
  // machine, so a 719 is reported once, from the first call.
  const std::size_t image_bytes = source->bytes.size();
  const std::size_t image_span = (image_bytes + 65535u) & ~std::size_t{65535u};
  void* image_copy = VirtualAlloc(nullptr, image_span, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if (image_copy == nullptr) {
    set_error("code object image allocation failed");
    return kInvalid;
  }
  std::memcpy(image_copy, source->bytes.data(), image_bytes);
  const hipError_t stale = hipGetLastError();
  std::size_t free_b = 0;
  std::size_t total_b = 0;
  const hipError_t mem = hipMemGetInfo(&free_b, &total_b);
  (void)stale;
  (void)free_b;
  (void)total_b;
  (void)mem;
  hipModule_t module = nullptr;
  hipError_t loaded_module = hipModuleLoadData(&module, image_copy);
  hipError_t second = hipSuccess;
  hipError_t from_file = hipSuccess;
  hipError_t again = hipSuccess;
  bool recovered = false;
  // A 719 here is not a sticky context: hipMemGetInfo just succeeded. Retry
  // the same image before giving up. Do not hipDeviceReset; that left the
  // retry at 719 on this machine.
  if (loaded_module != hipSuccess || module == nullptr) {
    module = nullptr;
    (void)hipGetLastError();
    second = hipModuleLoadData(&module, image_copy);
    if (second == hipSuccess && module != nullptr) {
      loaded_module = hipSuccess;
      recovered = true;
    }
  }
  if (loaded_module != hipSuccess || module == nullptr) {
    char dir[MAX_PATH];
    char file[MAX_PATH];
    if (GetTempPathA(MAX_PATH, dir) > 0 && GetTempFileNameA(dir, "lco", 0, file) != 0) {
      HANDLE out = CreateFileA(file, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                               FILE_ATTRIBUTE_TEMPORARY, nullptr);
      if (out != INVALID_HANDLE_VALUE) {
        DWORD wrote = 0;
        const BOOL ok = WriteFile(out, source->bytes.data(),
                                  static_cast<DWORD>(source->bytes.size()), &wrote, nullptr);
        CloseHandle(out);
        if (ok && wrote == source->bytes.size()) {
          module = nullptr;
          (void)hipGetLastError();
          from_file = hipModuleLoad(&module, file);
          if (from_file == hipSuccess && module != nullptr) {
            loaded_module = hipSuccess;
            recovered = true;
          }
        }
      }
      // hipModuleLoad may keep the path. Leave a successful file in place.
      if (!recovered) DeleteFileA(file);
    }
  }
  if ((loaded_module != hipSuccess || module == nullptr) && !g_last_good.empty()) {
    hipModule_t probe = nullptr;
    again = hipModuleLoadData(&probe, g_last_good.data());
    if (probe != nullptr) (void)hipModuleUnload(probe);
  }
  (void)second;
  (void)from_file;
  (void)again;
  if (loaded_module != hipSuccess || module == nullptr) {
    const char* text = hipGetErrorString(loaded_module);
    set_error(text);
    VirtualFree(image_copy, 0, MEM_RELEASE);
    return kInvalid;
  }
  VirtualFree(image_copy, 0, MEM_RELEASE);
  g_last_good.assign(source->bytes.begin(), source->bytes.end());
  ++g_loads_ok;
  for (const std::string& name : names) {
    hipFunction_t function = nullptr;
    const hipError_t got = hipModuleGetFunction(&function, module, name.c_str());
    if (got != hipSuccess || function == nullptr) {
      (void)hipGetLastError();
      continue;
    }
    auto* symbol = new (std::nothrow) KernelSymbol;
    if (symbol == nullptr) {
      (void)hipModuleUnload(module);
      set_error("out of memory");
      return kInvalid;
    }
    symbol->owner = image;
    symbol->function = function;
    symbol->name = name;
    image->symbols.push_back(symbol);
  }
  if (image->symbols.empty()) {
    (void)hipModuleUnload(module);
    set_error("hipModuleGetFunction found none of the code object's kernels");
    return kInvalid;
  }
  image->module = module;
  if (loaded != nullptr) *loaded = executable;
  return kSuccess;
}

__declspec(dllexport) int hsa_executable_freeze(std::uint64_t executable, const char*) {
  if (executable == 0) return kInvalid;
  auto* image = reinterpret_cast<LoadedExecutable*>(executable);
  if (image->module == nullptr) return kInvalid;
  image->frozen = true;
  return kSuccess;
}

__declspec(dllexport) int hsa_executable_destroy(std::uint64_t executable) {
  if (executable == 0) return kInvalid;
  auto* image = reinterpret_cast<LoadedExecutable*>(executable);
  for (KernelSymbol* symbol : image->symbols) delete symbol;
  image->symbols.clear();
  if (image->module != nullptr) (void)hipModuleUnload(image->module);
  delete image;
  return kSuccess;
}

__declspec(dllexport) int hsa_executable_iterate_symbols(
    std::uint64_t executable, int (*callback)(std::uint64_t, std::uint64_t, void*), void* data) {
  if (executable == 0 || callback == nullptr) return kInvalid;
  auto* image = reinterpret_cast<LoadedExecutable*>(executable);
  for (KernelSymbol* symbol : image->symbols) {
    const int stop = callback(executable, reinterpret_cast<std::uint64_t>(symbol), data);
    if (stop != 0) return kSuccess;
  }
  return kSuccess;
}

__declspec(dllexport) int hsa_executable_symbol_get_info(std::uint64_t symbol, int attribute, void* value) {
  if (symbol == 0 || value == nullptr) return kInvalid;
  auto* kernel = reinterpret_cast<KernelSymbol*>(symbol);
  if (attribute == 0) {
    *static_cast<const char**>(value) = kernel->name.c_str();
    return kSuccess;
  }
  return kInvalid;
}

__declspec(dllexport) int hsa_amd_launch_kernel(std::uint64_t symbol, std::uint32_t grid_x,
                                               std::uint32_t grid_y, std::uint32_t grid_z,
                                               std::uint32_t block_x, std::uint32_t block_y,
                                               std::uint32_t block_z, void* const* bases,
                                               const std::uint64_t* offsets, std::uint32_t binding_count,
                                               const void* constants, std::size_t constants_bytes) {
  if (symbol == 0 || grid_x == 0 || grid_y == 0 || grid_z == 0 || block_x == 0 || block_y == 0 ||
      block_z == 0) {
    return kInvalid;
  }
  auto* kernel = reinterpret_cast<KernelSymbol*>(symbol);
  if (kernel->function == nullptr || kernel->owner == nullptr || !kernel->owner->frozen) {
    return kInvalid;
  }
  if (binding_count > 0 && (bases == nullptr || offsets == nullptr)) return kInvalid;
  if (constants_bytes > 0 && constants == nullptr) return kInvalid;
  {
    std::lock_guard lock(g_mu);
    if (!ensure_device()) return kInvalid;
  }
  // hipHostGetDevicePointer wants the address passed to hipHostRegister.
  // The byte offset is applied after that translation.
  std::vector<void*> device_ptrs(binding_count);
  for (std::uint32_t i = 0; i < binding_count; ++i) {
    void* device = bases[i];
    if (device != nullptr) {
      void* translated = nullptr;
      const hipError_t got = hipHostGetDevicePointer(&translated, device, 0);
      if (got != hipSuccess || translated == nullptr) {
        set_error(got == hipSuccess ? "hipHostGetDevicePointer returned a null device pointer"
                                    : hipGetErrorString(got));
        return kInvalid;
      }
      device = translated;
    }
    device_ptrs[i] = static_cast<unsigned char*>(device) + offsets[i];
  }
  // The HIP kernels this runtime loads take one pointer per binding, then the
  // constants block by value. An empty block is still that trailing argument.
  alignas(8) unsigned char unused[8] = {};
  std::vector<std::uint64_t> words;
  void* constants_arg = unused;
  if (constants_bytes > 0) {
    const std::size_t count = (constants_bytes + sizeof(std::uint64_t) - 1) / sizeof(std::uint64_t);
    words.assign(count, 0);
    std::memcpy(words.data(), constants, constants_bytes);
    constants_arg = words.data();
  }
  std::vector<void*> params;
  params.reserve(device_ptrs.size() + 1);
  for (std::uint32_t i = 0; i < binding_count; ++i) params.push_back(&device_ptrs[i]);
  params.push_back(constants_arg);
  // gfx1150 reports a faulting kernel's hipDeviceSynchronize as success and
  // delivers hipErrorLaunchFailure on the next kernel's sync. Keep the name
  // that was current before this launch so the 719 names the kernel that ran.
  char previous[256];
  std::memcpy(previous, g_last_launch, sizeof(previous));
  const hipError_t launched = hipModuleLaunchKernel(
      kernel->function, grid_x, grid_y, grid_z, block_x, block_y, block_z, 0, nullptr,
      params.data(), nullptr);
  std::snprintf(g_last_launch, sizeof(g_last_launch),
                "%s grid=%u x %u x %u block=%u x %u x %u bindings=%u constants=%zu",
                kernel->name.c_str(), grid_x, grid_y, grid_z, block_x, block_y, block_z,
                binding_count, constants_bytes);
  if (launched != hipSuccess) {
    const char* text = hipGetErrorString(launched);
    set_error(text);
    std::fprintf(stderr, "[hsa] hipModuleLaunchKernel failed %s hip=%d %s\n", g_last_launch,
                 static_cast<int>(launched), text != nullptr ? text : "");
    std::fprintf(stderr, "[hsa] previous %s launches=%d\n", previous, g_launches_ok);
    return kInvalid;
  }
  const hipError_t synced = hipDeviceSynchronize();
  if (synced != hipSuccess) {
    const char* text = hipGetErrorString(synced);
    set_error(text);
    std::fprintf(stderr, "[hsa] hipDeviceSynchronize failed %s hip=%d %s\n", g_last_launch,
                 static_cast<int>(synced), text != nullptr ? text : "");
    std::fprintf(stderr, "[hsa] previous %s launches=%d\n", previous, g_launches_ok);
    return kInvalid;
  }
  ++g_launches_ok;
  return kSuccess;
}

}  // extern "C"
