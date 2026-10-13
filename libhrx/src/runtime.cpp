// Windows libhrx. Loads hsa-runtime64.dll and answers the HRX C ABI from the
// agents, pools, locks, copies, and kernel launch that DLL implements.
//
// Queue count, total memory, shared memory, and clock stay
// HRX_STATUS_UNAVAILABLE: this runtime does not invent them. One host
// submission path exists, named by affinity bit 0. Any other bit is
// HRX_STATUS_OUT_OF_RANGE. Kernel dispatch on that path calls
// hsa_amd_launch_kernel. A code object that is not an AMDGPU image is
// rejected. hrx.dll does not link HIP.

#include "hrx_runtime.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <atomic>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <new>
#include <string>
#include <thread>
#include <utility>
#include <vector>

// The header's opaque pointers name these types in the global namespace.
struct hrx_status_s {
  hrx_status_code_t code;
  char message[512];
};

namespace {

struct HsaAgent {
  std::uint64_t handle;
};
struct HsaPool {
  std::uint64_t handle;
};
struct HsaSignal {
  std::uint64_t handle;
};

using hsa_init_fn = int (*)();
using hsa_iterate_agents_fn = int (*)(int (*)(HsaAgent, void*), void*);
using hsa_agent_get_info_fn = int (*)(HsaAgent, int, void*);
using hsa_iterate_pools_fn = int (*)(HsaAgent, int (*)(HsaPool, void*), void*);
using hsa_pool_info_fn = int (*)(HsaPool, int, void*);
using hsa_agent_pool_info_fn = int (*)(HsaAgent, HsaPool, int, void*);
using hsa_allow_fn = int (*)(std::uint32_t, const HsaAgent*, const std::uint32_t*,
                             const void*);
using hsa_copy_fn = int (*)(void*, HsaAgent, const void*, HsaAgent, std::size_t,
                            std::uint32_t, const HsaSignal*, HsaSignal);
using hsa_lock_fn = int (*)(void*, std::size_t, HsaAgent*, int, void**);
using hsa_unlock_fn = int (*)(void*);
using hsa_reader_create_fn = int (*)(const void*, std::size_t, std::uint64_t*);
using hsa_reader_destroy_fn = int (*)(std::uint64_t);
using hsa_exe_create_fn = int (*)(std::uint64_t*);
using hsa_exe_load_fn = int (*)(std::uint64_t, HsaAgent, std::uint64_t, const char*,
                                std::uint64_t*);
using hsa_exe_freeze_fn = int (*)(std::uint64_t, const char*);
using hsa_exe_destroy_fn = int (*)(std::uint64_t);
using hsa_symbol_cb = int (*)(std::uint64_t, std::uint64_t, void*);
using hsa_exe_iterate_fn = int (*)(std::uint64_t, hsa_symbol_cb, void*);
using hsa_symbol_info_fn = int (*)(std::uint64_t, int, void*);
using hsa_device_pointer_fn = int (*)(void**, void*);
using hsa_launch_fn = int (*)(std::uint64_t, std::uint32_t, std::uint32_t, std::uint32_t,
                              std::uint32_t, std::uint32_t, std::uint32_t, void* const*,
                              const std::uint64_t*, std::uint32_t, const void*, std::size_t);
using hsa_last_error_fn = const char* (*)();

constexpr int kHsaOk = 0;
constexpr int kInfoName = 0;
constexpr int kInfoWavefront = 6;
constexpr int kInfoWorkgroup = 8;
constexpr int kInfoNode = 16;
constexpr int kInfoDevice = 17;
constexpr int kInfoCu = 0xA002;
constexpr int kInfoUuid = 0xA011;
constexpr int kInfoMemAvail = 0xA015;
constexpr int kDeviceCpu = 0;
constexpr int kDeviceGpu = 1;
constexpr int kPoolSegment = 0;
constexpr int kPoolGlobalFlags = 1;
constexpr std::uint32_t kSegmentGlobal = 0;
constexpr std::uint32_t kFlagCoarse = 4;

struct HsaApi {
  HMODULE module = nullptr;
  bool ready = false;
  hsa_init_fn init = nullptr;
  hsa_iterate_agents_fn iterate_agents = nullptr;
  hsa_agent_get_info_fn agent_info = nullptr;
  hsa_iterate_pools_fn iterate_pools = nullptr;
  hsa_pool_info_fn pool_info = nullptr;
  hsa_agent_pool_info_fn agent_pool_info = nullptr;
  hsa_allow_fn allow = nullptr;
  hsa_copy_fn copy = nullptr;
  hsa_lock_fn lock = nullptr;
  hsa_unlock_fn unlock = nullptr;
  bool can_dispatch = false;
  hsa_reader_create_fn reader_create = nullptr;
  hsa_reader_destroy_fn reader_destroy = nullptr;
  hsa_exe_create_fn exe_create = nullptr;
  hsa_exe_load_fn exe_load = nullptr;
  hsa_exe_freeze_fn exe_freeze = nullptr;
  hsa_exe_destroy_fn exe_destroy = nullptr;
  hsa_exe_iterate_fn exe_iterate = nullptr;
  hsa_symbol_info_fn symbol_info = nullptr;
  hsa_launch_fn launch = nullptr;
  hsa_last_error_fn last_error = nullptr;
  hsa_device_pointer_fn device_pointer = nullptr;
};

HsaApi g_hsa;
std::mutex g_mu;

hrx_status_s g_oom{HRX_STATUS_OUT_OF_MEMORY, {0}};

hrx_status_t fail(hrx_status_code_t code, const char* message) {
  if (code == HRX_STATUS_OK) return nullptr;
  auto* status = new (std::nothrow) hrx_status_s;
  if (status == nullptr) {
    g_oom.code = HRX_STATUS_OUT_OF_MEMORY;
    std::memcpy(g_oom.message, "out of memory", sizeof("out of memory"));
    return &g_oom;
  }
  status->code = code;
  status->message[0] = '\0';
  if (message != nullptr) {
    const std::size_t n = std::strlen(message);
    const std::size_t copy = n < sizeof(status->message) ? n : sizeof(status->message) - 1;
    std::memcpy(status->message, message, copy);
    status->message[copy] = '\0';
  }
  return status;
}

hrx_status_t fail_hsa(const char* what, int code) {
  char buffer[160];
  std::snprintf(buffer, sizeof(buffer), "%s failed (hsa status %#x)", what, code);
  return fail(HRX_STATUS_UNAVAILABLE, buffer);
}

}  // namespace

struct hrx_device_s {
  std::atomic<int> refs{1};
  bool alive = true;
  bool gpu = false;
  bool spanning = false;
  int ordinal = 0;
  std::uint64_t agent = 0;
  std::uint64_t pool = 0;
  bool has_pool = false;
  std::uint32_t node = 0;
  std::string arch;
  std::string name;
  std::uint32_t compute_units = 0;
  std::uint32_t workgroup = 0;
  std::uint32_t wavefront = 0;
  bool has_compute_units = false;
  bool has_workgroup = false;
  bool has_wavefront = false;
  hrx_allocator_t allocator = nullptr;
  std::mutex mu;
  std::vector<hrx_stream_t> streams;
};

struct hrx_allocator_s {
  std::atomic<int> refs{1};
  hrx_device_t device = nullptr;
};

struct hrx_semaphore_s {
  std::atomic<int> refs{1};
  std::atomic<std::uint64_t> value{0};
};

enum class CmdKind { Copy, Fill, Update, Wait, Dispatch };

struct Cmd {
  CmdKind kind = CmdKind::Copy;
  hrx_buffer_t src = nullptr;
  hrx_buffer_t dst = nullptr;
  const void* host = nullptr;
  std::size_t src_offset = 0;
  std::size_t dst_offset = 0;
  std::size_t size = 0;
  std::vector<unsigned char> pattern;
  hrx_semaphore_t wait_sem = nullptr;
  std::uint64_t wait_value = 0;
  hrx_executable_t executable = nullptr;
  std::uint32_t ordinal = 0;
  hrx_dispatch_config_t config{};
  std::vector<unsigned char> constants;
  std::vector<hrx_buffer_t> bound;
  std::vector<std::size_t> bound_offset;
  std::vector<std::size_t> bound_length;
};

struct hrx_stream_s {
  std::atomic<int> refs{1};
  hrx_device_t device = nullptr;
  std::uint32_t flags = 0;
  hrx_queue_affinity_t affinity = 0;
  hrx_semaphore_t timeline = nullptr;
  std::uint64_t reserved = 0;
  std::mutex mu;
  std::vector<Cmd> pending;
};

struct hrx_buffer_s {
  std::atomic<int> refs{1};
  hrx_device_t device = nullptr;
  void* base = nullptr;
  void* dev = nullptr;
  std::size_t size = 0;
  bool owned = false;
  bool locked = false;
  hrx_memory_type_t type = 0;
  hrx_buffer_usage_t usage = 0;
  bool mapped = false;
};

struct hrx_event_s {
  std::atomic<int> refs{1};
  hrx_device_t device = nullptr;
  hrx_stream_t stream = nullptr;
  hrx_event_flags_t flags = HRX_EVENT_FLAG_NONE;
  bool recorded = false;
  std::uint64_t timepoint = 0;
  std::int64_t host_ns = 0;
};

struct hrx_mem_pool_s {
  std::atomic<int> refs{1};
  hrx_device_t device = nullptr;
  hrx_mem_pool_props_t props{};
  std::mutex mu;
  std::uint64_t used = 0;
  std::uint64_t used_high = 0;
  std::uint64_t reserved = 0;
  std::uint64_t reserved_high = 0;
  std::uint64_t release_threshold = 0;
};

struct hrx_fence_s {
  std::atomic<int> refs{1};
  std::size_t capacity = 0;
  std::mutex mu;
  std::vector<hrx_semaphore_t> semaphores;
  std::vector<std::uint64_t> values;
};

struct hrx_buffer_view_s {
  std::atomic<int> refs{1};
  hrx_buffer_t buffer = nullptr;
  std::vector<std::int64_t> shape;
  hrx_element_type_t element = 0;
  hrx_encoding_type_t encoding = 0;
};

struct hrx_export_s {
  std::string name;
  std::uint64_t symbol = 0;
};

struct hrx_executable_s {
  std::atomic<int> refs{1};
  std::uint64_t handle = 0;
  std::vector<hrx_export_s> exports;
};

namespace {

struct Accelerator {
  bool up = false;
  std::vector<hrx_device_t> devices;
};

Accelerator g_gpu;
Accelerator g_cpu;
std::mutex g_buffers_mu;
std::vector<hrx_buffer_t> g_buffers;
hrx_device_event_sink_t g_sink{};

std::int64_t host_ns() {
  static const std::int64_t freq = [] {
    LARGE_INTEGER value;
    QueryPerformanceFrequency(&value);
    return value.QuadPart;
  }();
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  return static_cast<std::int64_t>(now.QuadPart * (1000000000.0 / freq));
}

bool in_range(std::size_t size, std::size_t offset, std::size_t bytes) {
  if (bytes == 0) return true;
  if (offset > size) return false;
  return bytes <= size - offset;
}

void track_buffer(hrx_buffer_t buffer) {
  std::lock_guard lock(g_buffers_mu);
  g_buffers.push_back(buffer);
}

void untrack_buffer(hrx_buffer_t buffer) {
  std::lock_guard lock(g_buffers_mu);
  for (auto it = g_buffers.begin(); it != g_buffers.end(); ++it) {
    if (*it == buffer) {
      g_buffers.erase(it);
      return;
    }
  }
}

bool agent_info(HsaAgent agent, int attribute, void* out, std::size_t bytes) {
  if (!g_hsa.ready || out == nullptr) return false;
  return g_hsa.agent_info(agent, attribute, out) == kHsaOk && bytes > 0;
}

std::string agent_name(HsaAgent agent) {
  char buffer[64] = {};
  if (!agent_info(agent, kInfoName, buffer, sizeof(buffer))) return {};
  buffer[sizeof(buffer) - 1] = '\0';
  return std::string(buffer);
}

bool agent_u32(HsaAgent agent, int attribute, std::uint32_t* out) {
  return agent_info(agent, attribute, out, sizeof(*out));
}

bool agent_kind(HsaAgent agent, int* out) {
  return agent_info(agent, kInfoDevice, out, sizeof(*out));
}

extern "C" int collect_agent(HsaAgent agent, void* data) {
  static_cast<std::vector<HsaAgent>*>(data)->push_back(agent);
  return 0;
}

extern "C" int collect_pool(HsaPool pool, void* data) {
  static_cast<std::vector<HsaPool>*>(data)->push_back(pool);
  return 0;
}

std::wstring module_dir(HMODULE module) {
  wchar_t buffer[MAX_PATH];
  const DWORD n = GetModuleFileNameW(module, buffer, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) return {};
  std::wstring path(buffer, n);
  const auto slash = path.find_last_of(L"\\/");
  if (slash == std::wstring::npos) return {};
  return path.substr(0, slash + 1);
}

template <typename T>
bool load_sym(HMODULE module, const char* name, T* out) {
  auto* found = reinterpret_cast<T>(GetProcAddress(module, name));
  *out = found;
  return found != nullptr;
}

hrx_status_t ensure_hsa() {
  if (g_hsa.ready) return nullptr;
  HMODULE self = nullptr;
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                         GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     reinterpret_cast<LPCWSTR>(&hrx_runtime_version), &self);
  const std::wstring beside_hrx = module_dir(self) + L"hsa-runtime64.dll";
  const std::wstring beside_exe = module_dir(nullptr) + L"hsa-runtime64.dll";
  HMODULE module = LoadLibraryW(beside_hrx.c_str());
  if (module == nullptr) module = LoadLibraryW(beside_exe.c_str());
  if (module == nullptr) module = LoadLibraryW(L"hsa-runtime64.dll");
  if (module == nullptr) {
    return fail(HRX_STATUS_UNAVAILABLE,
                "hsa-runtime64.dll was not found beside hrx.dll or the executable");
  }

  std::string missing;
  if (!load_sym(module, "hsa_init", &g_hsa.init)) missing = "hsa_init";
  else if (!load_sym(module, "hsa_iterate_agents", &g_hsa.iterate_agents))
    missing = "hsa_iterate_agents";
  else if (!load_sym(module, "hsa_agent_get_info", &g_hsa.agent_info))
    missing = "hsa_agent_get_info";
  else if (!load_sym(module, "hsa_amd_agent_iterate_memory_pools", &g_hsa.iterate_pools))
    missing = "hsa_amd_agent_iterate_memory_pools";
  else if (!load_sym(module, "hsa_amd_memory_pool_get_info", &g_hsa.pool_info))
    missing = "hsa_amd_memory_pool_get_info";
  else if (!load_sym(module, "hsa_amd_agent_memory_pool_get_info", &g_hsa.agent_pool_info))
    missing = "hsa_amd_agent_memory_pool_get_info";
  else if (!load_sym(module, "hsa_amd_agents_allow_access", &g_hsa.allow))
    missing = "hsa_amd_agents_allow_access";
  else if (!load_sym(module, "hsa_amd_memory_async_copy", &g_hsa.copy))
    missing = "hsa_amd_memory_async_copy";
  else if (!load_sym(module, "hsa_amd_memory_lock", &g_hsa.lock))
    missing = "hsa_amd_memory_lock";
  else if (!load_sym(module, "hsa_amd_memory_unlock", &g_hsa.unlock))
    missing = "hsa_amd_memory_unlock";
  if (!missing.empty()) {
    FreeLibrary(module);
    return fail(HRX_STATUS_UNAVAILABLE,
                (missing + " is not exported by hsa-runtime64.dll").c_str());
  }

  const int rc = g_hsa.init();
  if (rc != kHsaOk) {
    FreeLibrary(module);
    return fail_hsa("hsa_init", rc);
  }
  g_hsa.can_dispatch =
      load_sym(module, "hsa_code_object_reader_create_from_memory", &g_hsa.reader_create) &&
      load_sym(module, "hsa_code_object_reader_destroy", &g_hsa.reader_destroy) &&
      load_sym(module, "hsa_executable_create", &g_hsa.exe_create) &&
      load_sym(module, "hsa_executable_load_agent_code_object", &g_hsa.exe_load) &&
      load_sym(module, "hsa_executable_freeze", &g_hsa.exe_freeze) &&
      load_sym(module, "hsa_executable_destroy", &g_hsa.exe_destroy) &&
      load_sym(module, "hsa_executable_iterate_symbols", &g_hsa.exe_iterate) &&
      load_sym(module, "hsa_executable_symbol_get_info", &g_hsa.symbol_info) &&
      load_sym(module, "hsa_amd_launch_kernel", &g_hsa.launch);
  (void)load_sym(module, "hsa_amd_last_error", &g_hsa.last_error);
  (void)load_sym(module, "hsa_amd_get_device_pointer", &g_hsa.device_pointer);
  g_hsa.module = module;
  g_hsa.ready = true;
  return nullptr;
}

struct AgentFacts {
  HsaAgent agent{};
  int kind = -1;
  std::string arch;
  std::string uuid;
  std::uint32_t node = 0;
  std::uint32_t compute_units = 0;
  std::uint32_t workgroup = 0;
  std::uint32_t wavefront = 0;
  bool has_node = false;
  bool has_compute_units = false;
  bool has_workgroup = false;
  bool has_wavefront = false;
  std::uint64_t pool = 0;
  bool has_pool = false;
};

bool coarse_pool(HsaAgent agent, std::uint64_t* out) {
  std::vector<HsaPool> pools;
  if (g_hsa.iterate_pools(agent, collect_pool, &pools) != kHsaOk) return false;
  for (const HsaPool pool : pools) {
    std::uint32_t segment = 0xffffffffu;
    std::uint32_t flags = 0;
    if (g_hsa.pool_info(pool, kPoolSegment, &segment) != kHsaOk) continue;
    if (g_hsa.pool_info(pool, kPoolGlobalFlags, &flags) != kHsaOk) continue;
    if (segment == kSegmentGlobal && (flags & kFlagCoarse) != 0) {
      *out = pool.handle;
      return true;
    }
  }
  return false;
}

hrx_status_t read_agents(std::vector<AgentFacts>* out) {
  std::vector<HsaAgent> agents;
  const int rc = g_hsa.iterate_agents(collect_agent, &agents);
  if (rc != kHsaOk) return fail_hsa("hsa_iterate_agents", rc);
  for (const HsaAgent agent : agents) {
    AgentFacts facts;
    facts.agent = agent;
    if (!agent_kind(agent, &facts.kind)) continue;
    facts.arch = agent_name(agent);
    char uuid[64] = {};
    if (agent_info(agent, kInfoUuid, uuid, sizeof(uuid))) {
      uuid[sizeof(uuid) - 1] = '\0';
      facts.uuid = uuid;
    }
    facts.has_node = agent_u32(agent, kInfoNode, &facts.node);
    facts.has_compute_units = agent_u32(agent, kInfoCu, &facts.compute_units);
    facts.has_workgroup = agent_u32(agent, kInfoWorkgroup, &facts.workgroup);
    facts.has_wavefront = agent_u32(agent, kInfoWavefront, &facts.wavefront);
    facts.has_pool = coarse_pool(agent, &facts.pool);
    out->push_back(std::move(facts));
  }
  return nullptr;
}

hrx_device_t make_device(const AgentFacts& facts, int ordinal, bool spanning) {
  auto* device = new (std::nothrow) hrx_device_s;
  if (device == nullptr) return nullptr;
  device->gpu = facts.kind == kDeviceGpu;
  device->spanning = spanning;
  device->ordinal = ordinal;
  device->agent = facts.agent.handle;
  device->pool = facts.pool;
  device->has_pool = facts.has_pool;
  device->node = facts.node;
  device->arch = facts.arch;
  device->name = facts.arch;
  if (facts.has_node) {
    device->name += " (Node " + std::to_string(facts.node) + ")";
  }
  device->compute_units = facts.compute_units;
  device->workgroup = facts.workgroup;
  device->wavefront = facts.wavefront;
  device->has_compute_units = facts.has_compute_units && facts.compute_units != 0;
  device->has_workgroup = facts.has_workgroup && facts.workgroup != 0;
  device->has_wavefront = facts.has_wavefront && facts.wavefront != 0;
  auto* allocator = new (std::nothrow) hrx_allocator_s;
  if (allocator == nullptr) {
    delete device;
    return nullptr;
  }
  allocator->device = device;
  device->allocator = allocator;
  return device;
}

void drop_devices(std::vector<hrx_device_t>* devices) {
  for (hrx_device_t device : *devices) {
    device->alive = false;
    hrx_device_release(device);
  }
  devices->clear();
}

struct Topology {
  bool every = false;
  bool star = false;
  std::vector<int> ordinals;
  std::vector<std::string> uuids;
};

hrx_status_t parse_topology(const char* path, Topology* out) {
  if (path == nullptr || path[0] == '\0') {
    out->every = true;
    return nullptr;
  }
  std::string text(path);
  std::vector<std::string> tokens;
  std::size_t at = 0;
  while (at <= text.size()) {
    const std::size_t comma = text.find(',', at);
    std::string token = text.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
    const auto first = token.find_first_not_of(" \t");
    const auto last = token.find_last_not_of(" \t");
    if (first != std::string::npos) tokens.push_back(token.substr(first, last - first + 1));
    if (comma == std::string::npos) break;
    at = comma + 1;
  }
  if (tokens.empty()) return fail(HRX_STATUS_INVALID_ARGUMENT, "empty topology path");
  if (tokens.size() == 1 && tokens[0] == "*") {
    out->star = true;
    return nullptr;
  }
  for (const std::string& token : tokens) {
    if (token == "*") {
      return fail(HRX_STATUS_INVALID_ARGUMENT, "topology mixes * with other entries");
    }
    if (token.rfind("GPU-", 0) == 0) {
      out->uuids.push_back(token.substr(4));
      continue;
    }
    int ordinal = 0;
    const auto* begin = token.data();
    const auto* end = begin + token.size();
    const auto parsed = std::from_chars(begin, end, ordinal);
    if (parsed.ec != std::errc{} || parsed.ptr != end || ordinal < 0) {
      return fail(HRX_STATUS_INVALID_ARGUMENT, "topology entry is not an ordinal or GPU uuid");
    }
    out->ordinals.push_back(ordinal);
  }
  return nullptr;
}

bool uuid_match(const AgentFacts& facts, const std::string& uuid) {
  return !facts.uuid.empty() && (facts.uuid == uuid || facts.uuid == "GPU-" + uuid);
}

hrx_status_t select_gpus(const std::vector<AgentFacts>& all, const Topology& topology,
                         std::vector<AgentFacts>* selected, bool* spanning) {
  std::vector<AgentFacts> gpus;
  for (const AgentFacts& facts : all) {
    if (facts.kind == kDeviceGpu) gpus.push_back(facts);
  }
  if (topology.every) {
    *selected = gpus;
    *spanning = false;
    if (selected->empty()) return fail(HRX_STATUS_NOT_FOUND, "no GPU agent");
    return nullptr;
  }
  *spanning = true;
  if (topology.star) {
    *selected = gpus;
  } else {
    for (const int ordinal : topology.ordinals) {
      if (ordinal >= static_cast<int>(gpus.size())) {
        return fail(HRX_STATUS_NOT_FOUND, "topology names a GPU ordinal that is not present");
      }
      selected->push_back(gpus[static_cast<std::size_t>(ordinal)]);
    }
    for (const std::string& uuid : topology.uuids) {
      bool found = false;
      for (const AgentFacts& facts : gpus) {
        if (uuid_match(facts, uuid)) {
          selected->push_back(facts);
          found = true;
          break;
        }
      }
      if (!found) return fail(HRX_STATUS_NOT_FOUND, "topology names a GPU uuid that is not present");
    }
  }
  if (selected->empty()) return fail(HRX_STATUS_NOT_FOUND, "topology matched no GPU");
  if (selected->size() > 1) {
    return fail(HRX_STATUS_UNIMPLEMENTED,
                "spanning more than one GPU is not implemented");
  }
  return nullptr;
}

hrx_status_t publish(Accelerator* accel, const std::vector<AgentFacts>& facts, bool spanning) {
  std::vector<hrx_device_t> devices;
  for (std::size_t i = 0; i < facts.size(); ++i) {
    if (facts[i].kind == kDeviceGpu && !facts[i].has_node) {
      drop_devices(&devices);
      return fail(HRX_STATUS_UNAVAILABLE, "GPU agent published no HSA node");
    }
    hrx_device_t device = make_device(facts[i], static_cast<int>(i), spanning);
    if (device == nullptr) {
      drop_devices(&devices);
      return fail(HRX_STATUS_OUT_OF_MEMORY, "device");
    }
    devices.push_back(device);
  }
  accel->devices = std::move(devices);
  accel->up = true;
  return nullptr;
}

hrx_status_t check_affinity(hrx_queue_affinity_t affinity) {
  // Bit 0 is the one host submission path. The HSA DLL exports no queue, so
  // a wider mask would be a queue count this runtime does not have.
  if ((affinity & ~static_cast<hrx_queue_affinity_t>(1)) != 0) {
    return fail(HRX_STATUS_OUT_OF_RANGE, "queue affinity names a queue the device lacks");
  }
  return nullptr;
}

hrx_status_t write_string(const std::string& text, void* value, std::size_t bytes) {
  if (value == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "property buffer is null");
  if (text.size() + 1 > bytes) {
    return fail(HRX_STATUS_OUT_OF_RANGE, "property buffer is too small");
  }
  std::memcpy(value, text.data(), text.size());
  static_cast<char*>(value)[text.size()] = '\0';
  return nullptr;
}

hrx_status_t write_u32(std::uint32_t v, void* value, std::size_t bytes) {
  if (value == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "property buffer is null");
  if (bytes < sizeof(v)) return fail(HRX_STATUS_OUT_OF_RANGE, "property buffer is too small");
  std::memcpy(value, &v, sizeof(v));
  return nullptr;
}

hrx_status_t unavailable_property(const char* what) {
  return fail(HRX_STATUS_UNAVAILABLE, what);
}

bool ranges_overlap(const void* a, const void* b, std::size_t bytes) {
  const auto left = reinterpret_cast<std::uintptr_t>(a);
  const auto right = reinterpret_cast<std::uintptr_t>(b);
  return left < right + bytes && right < left + bytes;
}

hrx_status_t copy_bytes(hrx_device_t device, void* dst, const void* src, std::size_t bytes) {
  if (bytes == 0) return nullptr;
  if (dst == nullptr || src == nullptr) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "copy pointer is null");
  }
  if (ranges_overlap(dst, src, bytes)) {
    std::memmove(dst, src, bytes);
    return nullptr;
  }
  if (device == nullptr || !device->gpu) {
    std::memcpy(dst, src, bytes);
    return nullptr;
  }
  if (!g_hsa.ready) return fail(HRX_STATUS_FAILED_PRECONDITION, "HSA runtime is not up");
  HsaAgent agent{device->agent};
  const int rc = g_hsa.copy(dst, agent, src, agent, bytes, 0, nullptr, HsaSignal{0});
  if (rc != kHsaOk) return fail_hsa("hsa_amd_memory_async_copy", rc);
  return nullptr;
}

hrx_status_t fill_bytes(void* dst, std::size_t offset, std::size_t size,
                        const void* pattern, std::size_t pattern_size) {
  if (dst == nullptr || pattern == nullptr || pattern_size == 0) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "fill pattern is empty");
  }
  if (size % pattern_size != 0) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "fill length is not a multiple of the pattern");
  }
  auto* out = static_cast<unsigned char*>(dst) + offset;
  for (std::size_t at = 0; at < size; at += pattern_size) {
    std::memcpy(out + at, pattern, pattern_size);
  }
  return nullptr;
}

hrx_status_t lock_memory(hrx_device_t device, void* host, std::size_t bytes, void** locked) {
  if (!device->gpu) {
    *locked = host;
    return nullptr;
  }
  const int rc = g_hsa.lock(host, bytes, nullptr, 0, locked);
  if (rc != kHsaOk) return fail_hsa("hsa_amd_memory_lock", rc);
  HsaAgent agent{device->agent};
  const int allow = g_hsa.allow(1, &agent, nullptr, *locked);
  if (allow != kHsaOk) {
    g_hsa.unlock(host);
    return fail_hsa("hsa_amd_agents_allow_access", allow);
  }
  return nullptr;
}

hrx_buffer_t new_buffer(hrx_device_t device, void* base, void* dev, std::size_t size,
                        bool owned, bool locked, hrx_memory_type_t type,
                        hrx_buffer_usage_t usage) {
  auto* buffer = new (std::nothrow) hrx_buffer_s;
  if (buffer == nullptr) return nullptr;
  hrx_device_retain(device);
  buffer->device = device;
  buffer->base = base;
  buffer->dev = dev;
  buffer->size = size;
  buffer->owned = owned;
  buffer->locked = locked;
  buffer->type = type;
  buffer->usage = usage;
  track_buffer(buffer);
  return buffer;
}

void release_cmd(Cmd* cmd) {
  if (cmd->src != nullptr) hrx_buffer_release(cmd->src);
  if (cmd->dst != nullptr) hrx_buffer_release(cmd->dst);
  if (cmd->wait_sem != nullptr) hrx_semaphore_release(cmd->wait_sem);
  if (cmd->executable != nullptr) hrx_executable_release(cmd->executable);
  for (hrx_buffer_t buffer : cmd->bound) {
    if (buffer != nullptr) hrx_buffer_release(buffer);
  }
  cmd->src = nullptr;
  cmd->dst = nullptr;
  cmd->wait_sem = nullptr;
  cmd->executable = nullptr;
  cmd->bound.clear();
}

hrx_status_t launch_bindings(hrx_executable_t executable, std::uint32_t ordinal,
                              const hrx_dispatch_config_t* config, const void* constants,
                              std::size_t constants_size, const hrx_buffer_ref_t* bindings,
                              std::size_t binding_count);

hrx_status_t execute_cmd(hrx_stream_t stream, const Cmd& cmd) {
  if (cmd.kind == CmdKind::Dispatch) {
    std::vector<hrx_buffer_ref_t> refs(cmd.bound.size());
    for (std::size_t i = 0; i < cmd.bound.size(); ++i) {
      refs[i].buffer = cmd.bound[i];
      refs[i].offset = cmd.bound_offset[i];
      refs[i].length = cmd.bound_length[i];
    }
    return launch_bindings(cmd.executable, cmd.ordinal, &cmd.config,
                           cmd.constants.empty() ? nullptr : cmd.constants.data(),
                           cmd.constants.size(), refs.empty() ? nullptr : refs.data(),
                           refs.size());
  }
  if (cmd.kind == CmdKind::Wait) {
    return hrx_semaphore_wait(cmd.wait_sem, cmd.wait_value, UINT64_MAX);
  }
  if (cmd.kind == CmdKind::Fill) {
    if (!in_range(cmd.dst->size, cmd.dst_offset, cmd.size)) {
      return fail(HRX_STATUS_OUT_OF_RANGE, "fill is outside the buffer");
    }
    return fill_bytes(cmd.dst->dev, cmd.dst_offset, cmd.size, cmd.pattern.data(),
                      cmd.pattern.size());
  }
  if (cmd.kind == CmdKind::Update) {
    if (!in_range(cmd.dst->size, cmd.dst_offset, cmd.size)) {
      return fail(HRX_STATUS_OUT_OF_RANGE, "update is outside the buffer");
    }
    if (cmd.pattern.size() != cmd.size) {
      return fail(HRX_STATUS_INTERNAL, "update lost its captured bytes");
    }
    auto* dst = static_cast<unsigned char*>(cmd.dst->dev) + cmd.dst_offset;
    return copy_bytes(stream->device, dst, cmd.pattern.data(), cmd.size);
  }
  if (!in_range(cmd.src->size, cmd.src_offset, cmd.size) ||
      !in_range(cmd.dst->size, cmd.dst_offset, cmd.size)) {
    return fail(HRX_STATUS_OUT_OF_RANGE, "copy is outside the buffer");
  }
  auto* dst = static_cast<unsigned char*>(cmd.dst->dev) + cmd.dst_offset;
  const auto* src = static_cast<const unsigned char*>(cmd.src->dev) + cmd.src_offset;
  return copy_bytes(stream->device, dst, src, cmd.size);
}

hrx_status_t flush_stream(hrx_stream_t stream) {
  std::vector<Cmd> cmds;
  {
    std::lock_guard lock(stream->mu);
    cmds.swap(stream->pending);
  }
  hrx_status_t status = nullptr;
  for (Cmd& cmd : cmds) {
    if (hrx_status_is_ok(status)) status = execute_cmd(stream, cmd);
    release_cmd(&cmd);
  }
  if (!hrx_status_is_ok(status)) return status;
  if (cmds.empty()) return nullptr;
  std::uint64_t target = 0;
  {
    std::lock_guard lock(stream->mu);
    target = ++stream->reserved;
  }
  return hrx_semaphore_signal(stream->timeline, target);
}

hrx_status_t wait_list(const hrx_semaphore_list_t* list) {
  if (list == nullptr || list->count == 0) return nullptr;
  if (list->semaphores == nullptr || list->values == nullptr) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "semaphore list is missing its arrays");
  }
  for (std::size_t i = 0; i < list->count; ++i) {
    hrx_status_t status =
        hrx_semaphore_wait(list->semaphores[i], list->values[i], UINT64_MAX);
    if (!hrx_status_is_ok(status)) return status;
  }
  return nullptr;
}

hrx_status_t signal_list(const hrx_semaphore_list_t* list) {
  if (list == nullptr || list->count == 0) return nullptr;
  if (list->semaphores == nullptr || list->values == nullptr) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "semaphore list is missing its arrays");
  }
  for (std::size_t i = 0; i < list->count; ++i) {
    hrx_status_t status = hrx_semaphore_signal(list->semaphores[i], list->values[i]);
    if (!hrx_status_is_ok(status)) return status;
  }
  return nullptr;
}

hrx_status_t require_device(hrx_device_t device) {
  if (device == nullptr || !device->alive) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "device is null");
  }
  return nullptr;
}

hrx_status_t check_dispatch(hrx_executable_t executable, std::uint32_t ordinal,
                            const hrx_dispatch_config_t* config, const void* constants,
                            std::size_t constants_size, const hrx_buffer_ref_t* bindings,
                            std::size_t binding_count) {
  if (!g_hsa.can_dispatch) {
    return fail(HRX_STATUS_UNIMPLEMENTED,
                "kernel dispatch is not implemented; hsa-runtime64.dll has no code-object loader");
  }
  if (executable == nullptr || config == nullptr) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "dispatch is missing an executable or a config");
  }
  if (ordinal >= executable->exports.size()) {
    return fail(HRX_STATUS_OUT_OF_RANGE, "export ordinal is outside this executable");
  }
  for (int i = 0; i < 3; ++i) {
    if (config->workgroup_count[i] == 0 || config->workgroup_size[i] == 0) {
      return fail(HRX_STATUS_INVALID_ARGUMENT, "workgroup count or size is zero");
    }
  }
  if (binding_count > 0 && bindings == nullptr) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "dispatch bindings are missing");
  }
  if (constants_size > 0 && constants == nullptr) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "dispatch constants are missing");
  }
  for (std::size_t i = 0; i < binding_count; ++i) {
    hrx_buffer_t buffer = bindings[i].buffer;
    if (buffer == nullptr || buffer->dev == nullptr) {
      return fail(HRX_STATUS_INVALID_ARGUMENT, "dispatch binding is empty");
    }
    if (bindings[i].length == 0) {
      if (bindings[i].offset > buffer->size) {
        return fail(HRX_STATUS_OUT_OF_RANGE, "dispatch binding starts past the buffer");
      }
    } else if (!in_range(buffer->size, bindings[i].offset, bindings[i].length)) {
      return fail(HRX_STATUS_OUT_OF_RANGE, "dispatch binding is outside the buffer");
    }
  }
  return nullptr;
}

hrx_status_t launch_bindings(hrx_executable_t executable, std::uint32_t ordinal,
                             const hrx_dispatch_config_t* config, const void* constants,
                             std::size_t constants_size, const hrx_buffer_ref_t* bindings,
                             std::size_t binding_count) {
  if (hrx_status_t status = check_dispatch(executable, ordinal, config, constants, constants_size,
                                           bindings, binding_count);
      !hrx_status_is_ok(status)) {
    return status;
  }
  std::vector<void*> bases(binding_count);
  std::vector<std::uint64_t> offsets(binding_count);
  for (std::size_t i = 0; i < binding_count; ++i) {
    bases[i] = bindings[i].buffer->dev;
    offsets[i] = bindings[i].offset;
  }
  const int rc = g_hsa.launch(
      executable->exports[ordinal].symbol, config->workgroup_count[0], config->workgroup_count[1],
      config->workgroup_count[2], config->workgroup_size[0], config->workgroup_size[1],
      config->workgroup_size[2], bases.empty() ? nullptr : bases.data(),
      offsets.empty() ? nullptr : offsets.data(), static_cast<std::uint32_t>(bases.size()),
      constants, constants_size);
  if (rc != kHsaOk) {
    std::string message = "hsa_amd_launch_kernel failed";
    if (g_hsa.last_error != nullptr) {
      const char* extra = g_hsa.last_error();
      if (extra != nullptr && extra[0] != '\0') {
        message += ": ";
        message += extra;
      }
    }
    return fail(HRX_STATUS_UNAVAILABLE, message.c_str());
  }
  return nullptr;
}

bool bytes_contain(const void* data, std::size_t size, const char* key) {
  if (key == nullptr || key[0] == '\0') return true;
  const std::size_t key_len = std::strlen(key);
  if (key_len > size) return false;
  const auto* bytes = static_cast<const unsigned char*>(data);
  for (std::size_t i = 0; i + key_len <= size; ++i) {
    if (std::memcmp(bytes + i, key, key_len) == 0) return true;
  }
  return false;
}

int collect_symbol(std::uint64_t, std::uint64_t symbol, void* data) {
  static_cast<std::vector<std::uint64_t>*>(data)->push_back(symbol);
  return 0;
}

}  // namespace

extern "C" {

hrx_host_allocator_t hrx_host_allocator_system(void) {
  return hrx_host_allocator_t{nullptr, nullptr};
}

void hrx_runtime_version(int* major, int* minor, int* patch) {
  if (major != nullptr) *major = HRX_VERSION_MAJOR;
  if (minor != nullptr) *minor = HRX_VERSION_MINOR;
  if (patch != nullptr) *patch = HRX_VERSION_PATCH;
}

hrx_status_t hrx_make_status(hrx_status_code_t code, const char* message) {
  return fail(code, message);
}

hrx_status_code_t hrx_status_code(hrx_status_t status) {
  if (status == nullptr) return HRX_STATUS_OK;
  return status->code;
}

hrx_status_t hrx_status_to_string(hrx_status_t status, char** out_message, size_t* out_length) {
  if (out_message == nullptr || out_length == nullptr) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "status string output is null");
  }
  *out_message = nullptr;
  *out_length = 0;
  if (status == nullptr) return nullptr;
  const std::size_t length = std::strlen(status->message);
  auto* copy = static_cast<char*>(std::malloc(length + 1));
  if (copy == nullptr) return &g_oom;
  std::memcpy(copy, status->message, length + 1);
  *out_message = copy;
  *out_length = length;
  return nullptr;
}

void hrx_status_free_message(char* message) { std::free(message); }

void hrx_status_ignore(hrx_status_t status) {
  if (status == nullptr || status == &g_oom) return;
  delete status;
}

hrx_status_t hrx_host_allocator_malloc(hrx_host_allocator_t, size_t byte_length, void** out_ptr) {
  if (out_ptr == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "out_ptr is null");
  if (byte_length == 0) {
    *out_ptr = nullptr;
    return nullptr;
  }
  void* ptr = std::calloc(1, byte_length);
  if (ptr == nullptr) return fail(HRX_STATUS_OUT_OF_MEMORY, "host malloc");
  *out_ptr = ptr;
  return nullptr;
}

hrx_status_t hrx_host_allocator_malloc_uninitialized(hrx_host_allocator_t allocator,
                                                    size_t byte_length, void** out_ptr) {
  hrx_status_t status = hrx_host_allocator_malloc(allocator, byte_length, out_ptr);
  return status;
}

hrx_status_t hrx_host_allocator_realloc(hrx_host_allocator_t, size_t byte_length, void** inout_ptr) {
  if (inout_ptr == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "inout_ptr is null");
  void* ptr = std::realloc(*inout_ptr, byte_length == 0 ? 1 : byte_length);
  if (ptr == nullptr) return fail(HRX_STATUS_OUT_OF_MEMORY, "host realloc");
  *inout_ptr = ptr;
  return nullptr;
}

hrx_status_t hrx_host_allocator_clone(hrx_host_allocator_t, const void* src, size_t byte_length,
                                     void** out_ptr) {
  if (out_ptr == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "out_ptr is null");
  if (byte_length == 0) {
    *out_ptr = nullptr;
    return nullptr;
  }
  if (src == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "clone source is null");
  void* ptr = std::malloc(byte_length);
  if (ptr == nullptr) return fail(HRX_STATUS_OUT_OF_MEMORY, "host clone");
  std::memcpy(ptr, src, byte_length);
  *out_ptr = ptr;
  return nullptr;
}

void hrx_host_allocator_free(hrx_host_allocator_t, void* ptr) { std::free(ptr); }

hrx_status_t hrx_host_allocator_malloc_aligned(hrx_host_allocator_t, size_t byte_length,
                                              size_t min_alignment, size_t offset, void** out_ptr) {
  if (out_ptr == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "out_ptr is null");
  if (min_alignment == 0 || (min_alignment & (min_alignment - 1)) != 0) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "alignment is not a power of two");
  }
  if (byte_length == 0) {
    *out_ptr = nullptr;
    return nullptr;
  }
  const std::size_t bytes = byte_length + min_alignment + sizeof(void*);
  auto* raw = static_cast<unsigned char*>(std::malloc(bytes));
  if (raw == nullptr) return fail(HRX_STATUS_OUT_OF_MEMORY, "aligned malloc");
  const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(raw + sizeof(void*) + offset);
  const std::uintptr_t aligned = (base + min_alignment - 1) & ~(min_alignment - 1);
  auto* user = reinterpret_cast<unsigned char*>(aligned - offset);
  reinterpret_cast<void**>(user)[-1] = raw;
  std::memset(user, 0, byte_length);
  *out_ptr = user;
  return nullptr;
}

hrx_status_t hrx_host_allocator_realloc_aligned(hrx_host_allocator_t allocator, size_t byte_length,
                                               size_t min_alignment, size_t offset, void** inout_ptr) {
  if (inout_ptr == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "inout_ptr is null");
  void* fresh = nullptr;
  hrx_status_t status =
      hrx_host_allocator_malloc_aligned(allocator, byte_length, min_alignment, offset, &fresh);
  if (!hrx_status_is_ok(status)) return status;
  if (*inout_ptr != nullptr && fresh != nullptr && byte_length != 0) {
    std::memcpy(fresh, *inout_ptr, byte_length);
  }
  hrx_host_allocator_free_aligned(allocator, *inout_ptr);
  *inout_ptr = fresh;
  return nullptr;
}

void hrx_host_allocator_free_aligned(hrx_host_allocator_t, void* ptr) {
  if (ptr == nullptr) return;
  std::free(reinterpret_cast<void**>(ptr)[-1]);
}

hrx_status_t hrx_runtime_set_device_event_sink(hrx_device_event_sink_t sink) {
  std::lock_guard lock(g_mu);
  if (g_gpu.up || g_cpu.up) {
    return fail(HRX_STATUS_FAILED_PRECONDITION, "device event sink must be set before initialize");
  }
  g_sink = sink;
  return nullptr;
}

hrx_status_t hrx_gpu_initialize_over(const char* topology_path, uint32_t flags) {
  std::lock_guard lock(g_mu);
  if (g_gpu.up) return fail(HRX_STATUS_ALREADY_EXISTS, "hrx_gpu_initialize");
  if (flags != 0) return fail(HRX_STATUS_INVALID_ARGUMENT, "hrx_gpu_initialize flags are not implemented");
  if (hrx_status_t status = ensure_hsa(); !hrx_status_is_ok(status)) return status;
  Topology topology;
  if (hrx_status_t status = parse_topology(topology_path, &topology); !hrx_status_is_ok(status)) {
    return status;
  }
  std::vector<AgentFacts> agents;
  if (hrx_status_t status = read_agents(&agents); !hrx_status_is_ok(status)) return status;
  std::vector<AgentFacts> selected;
  bool spanning = false;
  if (hrx_status_t status = select_gpus(agents, topology, &selected, &spanning);
      !hrx_status_is_ok(status)) {
    return status;
  }
  return publish(&g_gpu, selected, spanning);
}

hrx_status_t hrx_gpu_initialize(uint32_t flags) {
  return hrx_gpu_initialize_over(nullptr, flags);
}

hrx_status_t hrx_gpu_shutdown(void) {
  std::vector<hrx_device_t> devices;
  {
    std::lock_guard lock(g_mu);
    if (!g_gpu.up) return fail(HRX_STATUS_FAILED_PRECONDITION, "GPU accelerator is not up");
    devices.swap(g_gpu.devices);
    g_gpu.up = false;
  }
  drop_devices(&devices);
  return nullptr;
}

hrx_status_t hrx_gpu_device_count(int* count) {
  if (count == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "count is null");
  std::lock_guard lock(g_mu);
  if (!g_gpu.up) return fail(HRX_STATUS_FAILED_PRECONDITION, "GPU accelerator is not up");
  *count = static_cast<int>(g_gpu.devices.size());
  return nullptr;
}

hrx_status_t hrx_gpu_device_get(int index, hrx_device_t* device) {
  if (device == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "device is null");
  *device = nullptr;
  std::lock_guard lock(g_mu);
  if (!g_gpu.up) return fail(HRX_STATUS_FAILED_PRECONDITION, "GPU accelerator is not up");
  if (index < 0 || index >= static_cast<int>(g_gpu.devices.size())) {
    return fail(HRX_STATUS_OUT_OF_RANGE, "GPU device index");
  }
  *device = g_gpu.devices[static_cast<std::size_t>(index)];
  return nullptr;
}

hrx_status_t hrx_cpu_initialize(uint32_t flags) {
  std::lock_guard lock(g_mu);
  if (g_cpu.up) return fail(HRX_STATUS_ALREADY_EXISTS, "hrx_cpu_initialize");
  if (flags != 0) return fail(HRX_STATUS_INVALID_ARGUMENT, "hrx_cpu_initialize flags are not implemented");
  if (hrx_status_t status = ensure_hsa(); !hrx_status_is_ok(status)) return status;
  std::vector<AgentFacts> agents;
  if (hrx_status_t status = read_agents(&agents); !hrx_status_is_ok(status)) return status;
  std::vector<AgentFacts> cpus;
  for (const AgentFacts& facts : agents) {
    if (facts.kind == kDeviceCpu) cpus.push_back(facts);
  }
  if (cpus.empty()) return fail(HRX_STATUS_NOT_FOUND, "no CPU agent");
  return publish(&g_cpu, cpus, false);
}

hrx_status_t hrx_cpu_shutdown(void) {
  std::vector<hrx_device_t> devices;
  {
    std::lock_guard lock(g_mu);
    if (!g_cpu.up) return fail(HRX_STATUS_FAILED_PRECONDITION, "CPU accelerator is not up");
    devices.swap(g_cpu.devices);
    g_cpu.up = false;
  }
  drop_devices(&devices);
  return nullptr;
}

hrx_status_t hrx_cpu_device_count(int* count) {
  if (count == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "count is null");
  std::lock_guard lock(g_mu);
  if (!g_cpu.up) return fail(HRX_STATUS_FAILED_PRECONDITION, "CPU accelerator is not up");
  *count = static_cast<int>(g_cpu.devices.size());
  return nullptr;
}

hrx_status_t hrx_cpu_device_get(int index, hrx_device_t* device) {
  if (device == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "device is null");
  *device = nullptr;
  std::lock_guard lock(g_mu);
  if (!g_cpu.up) return fail(HRX_STATUS_FAILED_PRECONDITION, "CPU accelerator is not up");
  if (index < 0 || index >= static_cast<int>(g_cpu.devices.size())) {
    return fail(HRX_STATUS_OUT_OF_RANGE, "CPU device index");
  }
  *device = g_cpu.devices[static_cast<std::size_t>(index)];
  return nullptr;
}

hrx_status_t hrx_device_get_property(hrx_device_t device, hrx_device_property_t prop,
                                    void* value, size_t value_size) {
  if (hrx_status_t status = require_device(device); !hrx_status_is_ok(status)) return status;
  switch (prop) {
    case HRX_DEVICE_PROPERTY_NAME:
      return write_string(device->name, value, value_size);
    case HRX_DEVICE_PROPERTY_ARCHITECTURE:
      return write_string(device->arch, value, value_size);
    case HRX_DEVICE_PROPERTY_TOTAL_MEMORY:
      return unavailable_property("Windows HSA runtime does not publish total memory");
    case HRX_DEVICE_PROPERTY_COMPUTE_UNITS:
      if (!device->has_compute_units) {
        return unavailable_property("compute unit count was not published");
      }
      return write_u32(device->compute_units, value, value_size);
    case HRX_DEVICE_PROPERTY_MAX_WORKGROUP_SIZE:
      if (!device->has_workgroup) {
        return unavailable_property("max workgroup size was not published");
      }
      return write_u32(device->workgroup, value, value_size);
    case HRX_DEVICE_PROPERTY_WARP_SIZE:
      if (!device->has_wavefront) {
        return unavailable_property("wavefront size was not published");
      }
      return write_u32(device->wavefront, value, value_size);
    case HRX_DEVICE_PROPERTY_MAX_SHARED_MEMORY:
      return unavailable_property("Windows HSA runtime does not publish shared memory");
    case HRX_DEVICE_PROPERTY_CLOCK_RATE:
      return unavailable_property("Windows HSA runtime does not publish a clock");
    case HRX_DEVICE_PROPERTY_PCI_BUS_ID:
      return unavailable_property("Windows HSA runtime does not publish a PCI bus id through HRX");
    case HRX_DEVICE_PROPERTY_QUEUE_COUNT:
      return unavailable_property("Windows HSA runtime does not publish a queue count");
    default:
      return fail(HRX_STATUS_INVALID_ARGUMENT, "unknown device property");
  }
}

hrx_status_t hrx_device_synchronize(hrx_device_t device) {
  if (hrx_status_t status = require_device(device); !hrx_status_is_ok(status)) return status;
  std::vector<hrx_stream_t> streams;
  {
    std::lock_guard lock(device->mu);
    streams = device->streams;
    for (hrx_stream_t stream : streams) hrx_stream_retain(stream);
  }
  hrx_status_t status = nullptr;
  for (hrx_stream_t stream : streams) {
    if (hrx_status_is_ok(status)) status = hrx_stream_synchronize(stream);
    hrx_stream_release(stream);
  }
  return status;
}

hrx_status_t hrx_device_get_type(hrx_device_t device, hrx_accelerator_type_t* type) {
  if (hrx_status_t status = require_device(device); !hrx_status_is_ok(status)) return status;
  if (type == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "type is null");
  *type = device->gpu ? HRX_ACCELERATOR_GPU : HRX_ACCELERATOR_CPU;
  return nullptr;
}

void hrx_device_retain(hrx_device_t device) {
  if (device != nullptr) device->refs.fetch_add(1, std::memory_order_relaxed);
}

void hrx_device_release(hrx_device_t device) {
  if (device == nullptr) return;
  if (device->refs.fetch_sub(1, std::memory_order_acq_rel) != 1) return;
  if (device->allocator != nullptr) {
    hrx_allocator_t allocator = device->allocator;
    device->allocator = nullptr;
    hrx_allocator_release(allocator);
  }
  delete device;
}

hrx_allocator_t hrx_device_allocator(hrx_device_t device) {
  if (device == nullptr) return nullptr;
  return device->allocator;
}

void hrx_allocator_retain(hrx_allocator_t allocator) {
  if (allocator != nullptr) allocator->refs.fetch_add(1, std::memory_order_relaxed);
}

void hrx_allocator_release(hrx_allocator_t allocator) {
  if (allocator == nullptr) return;
  if (allocator->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) delete allocator;
}

hrx_status_t hrx_allocator_allocate_buffer(hrx_allocator_t allocator, hrx_buffer_params_t params,
                                          size_t size, hrx_buffer_t* buffer) {
  if (buffer == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "buffer is null");
  *buffer = nullptr;
  if (allocator == nullptr || allocator->device == nullptr || !allocator->device->alive) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "allocator is null");
  }
  if (size == 0) return fail(HRX_STATUS_INVALID_ARGUMENT, "allocation size is zero");
  // VirtualAlloc's granularity is 64 KiB. hipHostRegister of the caller's
  // exact length can still be rounded by the driver past that reservation
  // into the next region, and the next module load then faults. Own a 2 MiB
  // span and register all of it. The buffer's logical size stays `size`.
  constexpr std::size_t kReserveGrain = 2u << 20;
  const std::size_t reserve = (size + kReserveGrain - 1) & ~(kReserveGrain - 1);
  void* base = VirtualAlloc(nullptr, reserve, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if (base == nullptr) return fail(HRX_STATUS_OUT_OF_MEMORY, "VirtualAlloc");
  void* locked = nullptr;
  hrx_status_t status = lock_memory(allocator->device, base, reserve, &locked);
  if (!hrx_status_is_ok(status)) {
    VirtualFree(base, 0, MEM_RELEASE);
    return status;
  }
  hrx_buffer_t created = new_buffer(allocator->device, base, locked, size, true,
                                   allocator->device->gpu, params.type, params.usage);
  if (created == nullptr) {
    if (allocator->device->gpu) g_hsa.unlock(base);
    VirtualFree(base, 0, MEM_RELEASE);
    return fail(HRX_STATUS_OUT_OF_MEMORY, "buffer");
  }
  *buffer = created;
  return nullptr;
}

hrx_status_t hrx_allocator_import_buffer(hrx_allocator_t allocator, hrx_buffer_params_t params,
                                        void* host_ptr, size_t size, hrx_buffer_t* buffer) {
  if (buffer == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "buffer is null");
  *buffer = nullptr;
  if (allocator == nullptr || allocator->device == nullptr || host_ptr == nullptr || size == 0) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "import arguments");
  }
  void* locked = host_ptr;
  bool did_lock = false;
  if (allocator->device->gpu) {
    hrx_status_t status = lock_memory(allocator->device, host_ptr, size, &locked);
    if (!hrx_status_is_ok(status)) return status;
    did_lock = true;
  }
  hrx_buffer_t created = new_buffer(allocator->device, host_ptr, locked, size, false, did_lock,
                                   params.type, params.usage);
  if (created == nullptr) {
    if (did_lock) g_hsa.unlock(host_ptr);
    return fail(HRX_STATUS_OUT_OF_MEMORY, "import");
  }
  *buffer = created;
  return nullptr;
}

hrx_status_t hrx_semaphore_create(hrx_device_t device, uint64_t initial_value,
                                 hrx_semaphore_t* semaphore) {
  if (semaphore == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "semaphore is null");
  *semaphore = nullptr;
  if (hrx_status_t status = require_device(device); !hrx_status_is_ok(status)) return status;
  auto* created = new (std::nothrow) hrx_semaphore_s;
  if (created == nullptr) return fail(HRX_STATUS_OUT_OF_MEMORY, "semaphore");
  created->value.store(initial_value, std::memory_order_relaxed);
  *semaphore = created;
  return nullptr;
}

void hrx_semaphore_retain(hrx_semaphore_t semaphore) {
  if (semaphore != nullptr) semaphore->refs.fetch_add(1, std::memory_order_relaxed);
}

void hrx_semaphore_release(hrx_semaphore_t semaphore) {
  if (semaphore == nullptr) return;
  if (semaphore->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) delete semaphore;
}

hrx_status_t hrx_semaphore_query(hrx_semaphore_t semaphore, uint64_t* value) {
  if (semaphore == nullptr || value == nullptr) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "semaphore query");
  }
  *value = semaphore->value.load(std::memory_order_acquire);
  return nullptr;
}

hrx_status_t hrx_semaphore_wait(hrx_semaphore_t semaphore, uint64_t value, uint64_t timeout_ns) {
  if (semaphore == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "semaphore is null");
  const auto start = std::chrono::steady_clock::now();
  const bool forever = timeout_ns == UINT64_MAX;
  for (;;) {
    if (semaphore->value.load(std::memory_order_acquire) >= value) return nullptr;
    if (!forever) {
      const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
                               std::chrono::steady_clock::now() - start)
                               .count();
      if (timeout_ns == 0 || static_cast<std::uint64_t>(elapsed) >= timeout_ns) {
        return fail(HRX_STATUS_DEADLINE_EXCEEDED, "semaphore wait timed out");
      }
    }
    std::this_thread::sleep_for(std::chrono::microseconds(50));
  }
}

hrx_status_t hrx_semaphore_signal(hrx_semaphore_t semaphore, uint64_t value) {
  if (semaphore == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "semaphore is null");
  std::uint64_t current = semaphore->value.load(std::memory_order_relaxed);
  while (value > current &&
         !semaphore->value.compare_exchange_weak(current, value, std::memory_order_release,
                                                 std::memory_order_relaxed)) {
  }
  return nullptr;
}

hrx_status_t hrx_stream_create_on_queue(hrx_device_t device, uint32_t flags,
                                       hrx_queue_affinity_t queue_affinity, hrx_stream_t* stream) {
  if (stream == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "stream is null");
  *stream = nullptr;
  if (hrx_status_t status = require_device(device); !hrx_status_is_ok(status)) return status;
  if (hrx_status_t status = check_affinity(queue_affinity); !hrx_status_is_ok(status)) return status;
  auto* created = new (std::nothrow) hrx_stream_s;
  if (created == nullptr) return fail(HRX_STATUS_OUT_OF_MEMORY, "stream");
  hrx_device_retain(device);
  created->device = device;
  created->flags = flags;
  created->affinity = queue_affinity;
  hrx_status_t status = hrx_semaphore_create(device, 0, &created->timeline);
  if (!hrx_status_is_ok(status)) {
    hrx_device_release(device);
    delete created;
    return status;
  }
  {
    std::lock_guard lock(device->mu);
    device->streams.push_back(created);
  }
  *stream = created;
  return nullptr;
}

hrx_status_t hrx_stream_create(hrx_device_t device, uint32_t flags, hrx_stream_t* stream) {
  return hrx_stream_create_on_queue(device, flags, 0, stream);
}

void hrx_stream_retain(hrx_stream_t stream) {
  if (stream != nullptr) stream->refs.fetch_add(1, std::memory_order_relaxed);
}

void hrx_stream_release(hrx_stream_t stream) {
  if (stream == nullptr) return;
  if (stream->refs.fetch_sub(1, std::memory_order_acq_rel) != 1) return;
  std::vector<Cmd> cmds;
  {
    std::lock_guard lock(stream->mu);
    cmds.swap(stream->pending);
  }
  for (Cmd& cmd : cmds) release_cmd(&cmd);
  if (stream->device != nullptr) {
    std::lock_guard lock(stream->device->mu);
    auto& list = stream->device->streams;
    for (auto it = list.begin(); it != list.end(); ++it) {
      if (*it == stream) {
        list.erase(it);
        break;
      }
    }
  }
  hrx_semaphore_release(stream->timeline);
  hrx_device_release(stream->device);
  delete stream;
}

hrx_status_t hrx_stream_synchronize(hrx_stream_t stream) {
  if (stream == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "stream is null");
  if (hrx_status_t status = flush_stream(stream); !hrx_status_is_ok(status)) return status;
  std::uint64_t target = 0;
  {
    std::lock_guard lock(stream->mu);
    target = stream->reserved;
  }
  return hrx_semaphore_wait(stream->timeline, target, UINT64_MAX);
}

hrx_status_t hrx_stream_wait(hrx_stream_t stream) {
  if (stream == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "stream is null");
  std::uint64_t target = 0;
  hrx_semaphore_t timeline = nullptr;
  {
    std::lock_guard lock(stream->mu);
    target = stream->timeline->value.load(std::memory_order_acquire);
    timeline = stream->timeline;
    hrx_semaphore_retain(timeline);
  }
  hrx_status_t status = hrx_semaphore_wait(timeline, target, UINT64_MAX);
  hrx_semaphore_release(timeline);
  return status;
}

hrx_status_t hrx_stream_query(hrx_stream_t stream, bool* complete) {
  if (stream == nullptr || complete == nullptr) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "stream query");
  }
  std::lock_guard lock(stream->mu);
  const std::uint64_t done = stream->timeline->value.load(std::memory_order_acquire);
  *complete = stream->pending.empty() && done >= stream->reserved;
  return nullptr;
}

hrx_status_t hrx_stream_flush(hrx_stream_t stream) {
  if (stream == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "stream is null");
  return flush_stream(stream);
}

hrx_status_t hrx_stream_get_semaphore(hrx_stream_t stream, hrx_semaphore_t* semaphore) {
  if (stream == nullptr || semaphore == nullptr) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "stream semaphore");
  }
  *semaphore = stream->timeline;
  return nullptr;
}

hrx_status_t hrx_stream_get_device(hrx_stream_t stream, hrx_device_t* device) {
  if (stream == nullptr || device == nullptr) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "stream device");
  }
  *device = stream->device;
  return nullptr;
}

hrx_status_t hrx_stream_get_queue_affinity(hrx_stream_t stream, hrx_queue_affinity_t* queue_affinity) {
  if (stream == nullptr || queue_affinity == nullptr) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "queue affinity");
  }
  *queue_affinity = stream->affinity;
  return nullptr;
}

hrx_status_t hrx_stream_get_timeline_position(hrx_stream_t stream, hrx_timeline_point_t* position) {
  if (stream == nullptr || position == nullptr) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "timeline position");
  }
  position->semaphore = stream->timeline;
  position->value = stream->timeline->value.load(std::memory_order_acquire);
  return nullptr;
}

hrx_status_t hrx_stream_advance_timeline(hrx_stream_t stream, uint64_t* value) {
  if (stream == nullptr || value == nullptr) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "advance timeline");
  }
  std::lock_guard lock(stream->mu);
  *value = ++stream->reserved;
  return nullptr;
}

hrx_status_t hrx_stream_wait_on(hrx_stream_t stream, hrx_timeline_point_t position) {
  if (stream == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "stream is null");
  if (position.semaphore == nullptr || position.value == 0) return nullptr;
  Cmd cmd;
  cmd.kind = CmdKind::Wait;
  hrx_semaphore_retain(position.semaphore);
  cmd.wait_sem = position.semaphore;
  cmd.wait_value = position.value;
  std::lock_guard lock(stream->mu);
  stream->pending.push_back(std::move(cmd));
  return nullptr;
}

hrx_status_t hrx_buffer_allocate(hrx_stream_t stream, size_t size, hrx_memory_type_t mem_type,
                                hrx_buffer_usage_t usage, hrx_buffer_t* buffer) {
  if (buffer == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "buffer is null");
  *buffer = nullptr;
  if (stream == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "stream is null");
  if (hrx_status_t status = flush_stream(stream); !hrx_status_is_ok(status)) return status;
  hrx_buffer_params_t params{};
  params.type = mem_type;
  params.access = HRX_MEMORY_ACCESS_ALL;
  params.usage = usage;
  params.queue_affinity = stream->affinity;
  return hrx_allocator_allocate_buffer(stream->device->allocator, params, size, buffer);
}

void hrx_buffer_retain(hrx_buffer_t buffer) {
  if (buffer != nullptr) buffer->refs.fetch_add(1, std::memory_order_relaxed);
}

void hrx_buffer_release(hrx_buffer_t buffer) {
  if (buffer == nullptr) return;
  if (buffer->refs.fetch_sub(1, std::memory_order_acq_rel) != 1) return;
  untrack_buffer(buffer);
  if (buffer->locked && buffer->base != nullptr && g_hsa.ready) g_hsa.unlock(buffer->base);
  if (buffer->owned && buffer->base != nullptr) VirtualFree(buffer->base, 0, MEM_RELEASE);
  hrx_device_release(buffer->device);
  delete buffer;
}

hrx_status_t hrx_buffer_map(hrx_buffer_t buffer, hrx_map_flags_t, size_t offset, size_t size,
                           void** mapped_ptr) {
  if (buffer == nullptr || mapped_ptr == nullptr) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "map");
  }
  *mapped_ptr = nullptr;
  if (!in_range(buffer->size, offset, size)) {
    return fail(HRX_STATUS_OUT_OF_RANGE, "map is outside the buffer");
  }
  if (buffer->mapped) return fail(HRX_STATUS_ALREADY_EXISTS, "buffer is already mapped");
  buffer->mapped = true;
  *mapped_ptr = static_cast<unsigned char*>(buffer->dev) + offset;
  return nullptr;
}

hrx_status_t hrx_buffer_unmap(hrx_buffer_t buffer) {
  if (buffer == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "buffer is null");
  buffer->mapped = false;
  return nullptr;
}

hrx_status_t hrx_buffer_get_device_address(hrx_buffer_t buffer, uint64_t* address) {
  if (buffer == nullptr || address == nullptr) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "device address");
  }
  // Locked buffers are host pages. Kernels that load a pointer out of a table
  // (phase pointer tables, K/V fragment tables) must see the GPU address.
  // hipHostGetDevicePointer is applied to the registered base; callers add
  // the byte offset themselves.
  void* device = buffer->dev;
  if (buffer->locked && buffer->dev != nullptr && g_hsa.device_pointer != nullptr) {
    void* translated = nullptr;
    const int rc = g_hsa.device_pointer(&translated, buffer->dev);
    if (rc != kHsaOk || translated == nullptr) return fail_hsa("hsa_amd_get_device_pointer", rc);
    device = translated;
  }
  *address = reinterpret_cast<std::uintptr_t>(device);
  return nullptr;
}

hrx_status_t hrx_buffer_get_device_ptr(hrx_buffer_t buffer, void** device_ptr) {
  if (buffer == nullptr || device_ptr == nullptr) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "device pointer");
  }
  *device_ptr = buffer->dev;
  return nullptr;
}

hrx_status_t hrx_buffer_get_size(hrx_buffer_t buffer, size_t* size) {
  if (buffer == nullptr || size == nullptr) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "buffer size");
  }
  *size = buffer->size;
  return nullptr;
}

hrx_status_t hrx_synchronous_h2d(hrx_device_t device, const void* host_src, hrx_buffer_t dst,
                                size_t dst_offset, size_t size) {
  if (hrx_status_t status = require_device(device); !hrx_status_is_ok(status)) return status;
  if (host_src == nullptr || dst == nullptr) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "h2d");
  }
  if (!in_range(dst->size, dst_offset, size)) {
    return fail(HRX_STATUS_OUT_OF_RANGE, "h2d is outside the buffer");
  }
  auto* out = static_cast<unsigned char*>(dst->dev) + dst_offset;
  return copy_bytes(device, out, host_src, size);
}

hrx_status_t hrx_synchronous_d2h(hrx_device_t device, hrx_buffer_t src, size_t src_offset,
                                void* host_dst, size_t size) {
  if (hrx_status_t status = require_device(device); !hrx_status_is_ok(status)) return status;
  if (src == nullptr || host_dst == nullptr) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "d2h");
  }
  if (!in_range(src->size, src_offset, size)) {
    return fail(HRX_STATUS_OUT_OF_RANGE, "d2h is outside the buffer");
  }
  const auto* in = static_cast<const unsigned char*>(src->dev) + src_offset;
  return copy_bytes(device, host_dst, in, size);
}

hrx_status_t hrx_stream_fill_buffer(hrx_stream_t stream, hrx_buffer_t buffer, size_t offset,
                                   size_t size, const void* pattern, size_t pattern_size) {
  if (stream == nullptr || buffer == nullptr) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "fill");
  }
  Cmd cmd;
  cmd.kind = CmdKind::Fill;
  hrx_buffer_retain(buffer);
  cmd.dst = buffer;
  cmd.dst_offset = offset;
  cmd.size = size;
  if (pattern != nullptr && pattern_size != 0) {
    cmd.pattern.assign(static_cast<const unsigned char*>(pattern),
                       static_cast<const unsigned char*>(pattern) + pattern_size);
  }
  std::lock_guard lock(stream->mu);
  stream->pending.push_back(std::move(cmd));
  return nullptr;
}

hrx_status_t hrx_stream_copy_buffer(hrx_stream_t stream, hrx_buffer_t src, size_t src_offset,
                                   hrx_buffer_t dst, size_t dst_offset, size_t size) {
  if (stream == nullptr || src == nullptr || dst == nullptr) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "copy");
  }
  Cmd cmd;
  cmd.kind = CmdKind::Copy;
  hrx_buffer_retain(src);
  hrx_buffer_retain(dst);
  cmd.src = src;
  cmd.dst = dst;
  cmd.src_offset = src_offset;
  cmd.dst_offset = dst_offset;
  cmd.size = size;
  std::lock_guard lock(stream->mu);
  stream->pending.push_back(std::move(cmd));
  return nullptr;
}

hrx_status_t hrx_stream_update_buffer(hrx_stream_t stream, const void* host_data,
                                     size_t host_data_size, hrx_buffer_t dst, size_t dst_offset) {
  if (stream == nullptr || host_data == nullptr || dst == nullptr) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "update");
  }
  if (!in_range(dst->size, dst_offset, host_data_size)) {
    return fail(HRX_STATUS_OUT_OF_RANGE, "update is outside the buffer");
  }
  Cmd cmd;
  cmd.kind = CmdKind::Update;
  hrx_buffer_retain(dst);
  cmd.dst = dst;
  cmd.dst_offset = dst_offset;
  cmd.size = host_data_size;
  // The caller may free host_data before this stream flushes. Fragment
  // pointer tables are a local vector that dies when grow() returns, and the
  // GPU then stores through whatever bytes were left in that allocation.
  cmd.pattern.assign(static_cast<const unsigned char*>(host_data),
                     static_cast<const unsigned char*>(host_data) + host_data_size);
  std::lock_guard lock(stream->mu);
  stream->pending.push_back(std::move(cmd));
  return nullptr;
}

hrx_status_t hrx_queue_fill(hrx_device_t device, hrx_queue_affinity_t affinity,
                           const hrx_semaphore_list_t* wait_semaphores,
                           const hrx_semaphore_list_t* signal_semaphores, hrx_buffer_t buffer,
                           size_t offset, size_t size, const void* pattern, size_t pattern_size) {
  if (hrx_status_t status = require_device(device); !hrx_status_is_ok(status)) return status;
  if (hrx_status_t status = check_affinity(affinity); !hrx_status_is_ok(status)) return status;
  if (buffer == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "fill buffer is null");
  if (!in_range(buffer->size, offset, size)) {
    return fail(HRX_STATUS_OUT_OF_RANGE, "fill is outside the buffer");
  }
  if (hrx_status_t status = wait_list(wait_semaphores); !hrx_status_is_ok(status)) return status;
  if (hrx_status_t status = fill_bytes(buffer->dev, offset, size, pattern, pattern_size);
      !hrx_status_is_ok(status)) {
    return status;
  }
  return signal_list(signal_semaphores);
}

hrx_status_t hrx_queue_copy(hrx_device_t device, hrx_queue_affinity_t affinity,
                           const hrx_semaphore_list_t* wait_semaphores,
                           const hrx_semaphore_list_t* signal_semaphores, hrx_buffer_t src,
                           size_t src_offset, hrx_buffer_t dst, size_t dst_offset, size_t size) {
  if (hrx_status_t status = require_device(device); !hrx_status_is_ok(status)) return status;
  if (hrx_status_t status = check_affinity(affinity); !hrx_status_is_ok(status)) return status;
  if (src == nullptr || dst == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "copy buffer is null");
  if (!in_range(src->size, src_offset, size) || !in_range(dst->size, dst_offset, size)) {
    return fail(HRX_STATUS_OUT_OF_RANGE, "copy is outside the buffer");
  }
  if (hrx_status_t status = wait_list(wait_semaphores); !hrx_status_is_ok(status)) return status;
  auto* out = static_cast<unsigned char*>(dst->dev) + dst_offset;
  const auto* in = static_cast<const unsigned char*>(src->dev) + src_offset;
  if (hrx_status_t status = copy_bytes(device, out, in, size); !hrx_status_is_ok(status)) return status;
  return signal_list(signal_semaphores);
}

hrx_status_t hrx_queue_barrier(hrx_device_t device, hrx_queue_affinity_t affinity,
                              const hrx_semaphore_list_t* wait_semaphores,
                              const hrx_semaphore_list_t* signal_semaphores) {
  if (hrx_status_t status = require_device(device); !hrx_status_is_ok(status)) return status;
  if (hrx_status_t status = check_affinity(affinity); !hrx_status_is_ok(status)) return status;
  if (hrx_status_t status = wait_list(wait_semaphores); !hrx_status_is_ok(status)) return status;
  return signal_list(signal_semaphores);
}

hrx_status_t hrx_queue_dispatch(hrx_device_t device, hrx_queue_affinity_t affinity,
                               const hrx_semaphore_list_t* wait_semaphores,
                               const hrx_semaphore_list_t* signal_semaphores,
                               hrx_executable_t executable, uint32_t export_ordinal,
                               const hrx_dispatch_config_t* config, const void* constants,
                               size_t constants_size, const hrx_buffer_ref_t* bindings,
                               size_t binding_count, uint32_t) {
  if (hrx_status_t status = require_device(device); !hrx_status_is_ok(status)) return status;
  if (hrx_status_t status = check_affinity(affinity); !hrx_status_is_ok(status)) return status;
  if (hrx_status_t status = check_dispatch(executable, export_ordinal, config, constants,
                                           constants_size, bindings, binding_count);
      !hrx_status_is_ok(status)) {
    return status;
  }
  if (hrx_status_t status = wait_list(wait_semaphores); !hrx_status_is_ok(status)) return status;
  if (hrx_status_t status = launch_bindings(executable, export_ordinal, config, constants,
                                            constants_size, bindings, binding_count);
      !hrx_status_is_ok(status)) {
    return status;
  }
  return signal_list(signal_semaphores);
}

hrx_status_t hrx_stream_dispatch(hrx_stream_t stream, hrx_executable_t executable,
                                uint32_t export_ordinal, const hrx_dispatch_config_t* config,
                                const void* constants, size_t constants_size,
                                const hrx_buffer_ref_t* bindings, size_t binding_count, uint32_t) {
  if (stream == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "stream is null");
  if (hrx_status_t status = check_dispatch(executable, export_ordinal, config, constants,
                                           constants_size, bindings, binding_count);
      !hrx_status_is_ok(status)) {
    return status;
  }
  Cmd cmd;
  cmd.kind = CmdKind::Dispatch;
  hrx_executable_retain(executable);
  cmd.executable = executable;
  cmd.ordinal = export_ordinal;
  cmd.config = *config;
  if (constants_size > 0) {
    const auto* bytes = static_cast<const unsigned char*>(constants);
    cmd.constants.assign(bytes, bytes + constants_size);
  }
  cmd.bound.reserve(binding_count);
  cmd.bound_offset.reserve(binding_count);
  cmd.bound_length.reserve(binding_count);
  for (std::size_t i = 0; i < binding_count; ++i) {
    hrx_buffer_retain(bindings[i].buffer);
    cmd.bound.push_back(bindings[i].buffer);
    cmd.bound_offset.push_back(bindings[i].offset);
    cmd.bound_length.push_back(bindings[i].length);
  }
  std::lock_guard lock(stream->mu);
  stream->pending.push_back(std::move(cmd));
  return nullptr;
}

hrx_status_t hrx_queue_host_call(hrx_device_t device, hrx_queue_affinity_t affinity,
                                const hrx_semaphore_list_t* wait_semaphores,
                                const hrx_semaphore_list_t* signal_semaphores,
                                hrx_host_call_fn_t callback, void* user_data) {
  if (hrx_status_t status = require_device(device); !hrx_status_is_ok(status)) return status;
  if (hrx_status_t status = check_affinity(affinity); !hrx_status_is_ok(status)) return status;
  if (callback == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "host call is null");
  if (hrx_status_t status = wait_list(wait_semaphores); !hrx_status_is_ok(status)) return status;
  if (hrx_status_t status = callback(user_data); !hrx_status_is_ok(status)) return status;
  return signal_list(signal_semaphores);
}

hrx_status_t hrx_stream_execution_barrier(hrx_stream_t stream) {
  if (stream == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "stream is null");
  // No device pipeline exists. The barrier orders host-side copies on this stream.
  std::atomic_thread_fence(std::memory_order_seq_cst);
  return nullptr;
}

hrx_status_t hrx_executable_load_data(hrx_device_t device, const void* executable_data,
                                     size_t executable_data_size, const char* target_family,
                                     const char* target_key, hrx_executable_t* executable) {
  if (executable == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "executable is null");
  *executable = nullptr;
  if (hrx_status_t status = require_device(device); !hrx_status_is_ok(status)) return status;
  if (!device->gpu) return fail(HRX_STATUS_INVALID_ARGUMENT, "executable load needs a gpu device");
  if (executable_data == nullptr || executable_data_size == 0) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "executable image is empty");
  }
  if (target_family == nullptr ||
      (std::strcmp(target_family, "amdgpu") != 0 && std::strcmp(target_family, "hip") != 0)) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "executable target family is not amdgpu");
  }
  if (!g_hsa.can_dispatch) {
    return fail(HRX_STATUS_UNIMPLEMENTED,
                "executable load is not implemented; hsa-runtime64.dll has no code-object loader");
  }
  std::uint64_t reader = 0;
  const int opened =
      g_hsa.reader_create(executable_data, executable_data_size, &reader);
  if (opened != kHsaOk || reader == 0) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "code object is not an AMDGPU image");
  }
  if (!bytes_contain(executable_data, executable_data_size, target_key)) {
    g_hsa.reader_destroy(reader);
    return fail(HRX_STATUS_NOT_FOUND, "code object does not contain the requested architecture");
  }
  std::uint64_t handle = 0;
  const int created = g_hsa.exe_create(&handle);
  if (created != kHsaOk || handle == 0) {
    g_hsa.reader_destroy(reader);
    return fail_hsa("hsa_executable_create", created);
  }
  HsaAgent agent{device->agent};
  const int loaded = g_hsa.exe_load(handle, agent, reader, nullptr, nullptr);
  g_hsa.reader_destroy(reader);
  if (loaded != kHsaOk) {
    g_hsa.exe_destroy(handle);
    std::string message = "code object failed to load";
    if (g_hsa.last_error != nullptr) {
      const char* extra = g_hsa.last_error();
      if (extra != nullptr && extra[0] != '\0') {
        message += ": ";
        message += extra;
      }
    }
    return fail(HRX_STATUS_INVALID_ARGUMENT, message.c_str());
  }
  const int frozen = g_hsa.exe_freeze(handle, nullptr);
  if (frozen != kHsaOk) {
    g_hsa.exe_destroy(handle);
    return fail_hsa("hsa_executable_freeze", frozen);
  }
  std::vector<std::uint64_t> symbols;
  const int iterated = g_hsa.exe_iterate(handle, collect_symbol, &symbols);
  if (iterated != kHsaOk || symbols.empty()) {
    g_hsa.exe_destroy(handle);
    return fail(HRX_STATUS_INVALID_ARGUMENT, "code object advertises no kernels");
  }
  auto* created_exe = new (std::nothrow) hrx_executable_s;
  if (created_exe == nullptr) {
    g_hsa.exe_destroy(handle);
    return fail(HRX_STATUS_OUT_OF_MEMORY, "executable");
  }
  created_exe->handle = handle;
  created_exe->exports.reserve(symbols.size());
  for (std::uint64_t symbol : symbols) {
    const char* name = nullptr;
    if (g_hsa.symbol_info(symbol, 0, &name) != kHsaOk || name == nullptr || name[0] == '\0') {
      hrx_executable_release(created_exe);
      return fail(HRX_STATUS_INVALID_ARGUMENT, "kernel export has no name");
    }
    hrx_export_s exp;
    exp.name = name;
    exp.symbol = symbol;
    created_exe->exports.push_back(std::move(exp));
  }
  *executable = created_exe;
  return nullptr;
}

hrx_status_t hrx_executable_load_file(hrx_device_t device, const char* path,
                                     const char* target_family, const char* target_key,
                                     hrx_executable_t* executable) {
  if (executable != nullptr) *executable = nullptr;
  if (path == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "executable path is null");
  FILE* file = nullptr;
  if (fopen_s(&file, path, "rb") != 0 || file == nullptr) {
    return fail(HRX_STATUS_NOT_FOUND, "executable file was not found");
  }
  if (_fseeki64(file, 0, SEEK_END) != 0) {
    std::fclose(file);
    return fail(HRX_STATUS_INVALID_ARGUMENT, "executable file could not be read");
  }
  const long long bytes = _ftelli64(file);
  if (bytes < 0) {
    std::fclose(file);
    return fail(HRX_STATUS_INVALID_ARGUMENT, "executable file could not be read");
  }
  if (_fseeki64(file, 0, SEEK_SET) != 0) {
    std::fclose(file);
    return fail(HRX_STATUS_INVALID_ARGUMENT, "executable file could not be read");
  }
  std::vector<unsigned char> image(static_cast<std::size_t>(bytes));
  if (bytes > 0) {
    const std::size_t got = std::fread(image.data(), 1, image.size(), file);
    std::fclose(file);
    if (got != image.size()) {
      return fail(HRX_STATUS_INVALID_ARGUMENT, "executable file could not be read");
    }
  } else {
    std::fclose(file);
  }
  return hrx_executable_load_data(device, image.data(), image.size(), target_family, target_key,
                                  executable);
}

void hrx_executable_retain(hrx_executable_t executable) {
  if (executable != nullptr) executable->refs.fetch_add(1, std::memory_order_relaxed);
}

void hrx_executable_release(hrx_executable_t executable) {
  if (executable == nullptr) return;
  if (executable->refs.fetch_sub(1, std::memory_order_acq_rel) != 1) return;
  if (executable->handle != 0 && g_hsa.exe_destroy != nullptr) g_hsa.exe_destroy(executable->handle);
  delete executable;
}

hrx_status_t hrx_executable_export_count(hrx_executable_t executable, size_t* count) {
  if (executable == nullptr || count == nullptr) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "executable export count");
  }
  *count = executable->exports.size();
  return nullptr;
}

hrx_status_t hrx_executable_export_info(hrx_executable_t executable, uint32_t export_ordinal,
                                       hrx_executable_export_info_t* out_info) {
  if (executable == nullptr || out_info == nullptr) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "executable export info");
  }
  if (export_ordinal >= executable->exports.size()) {
    return fail(HRX_STATUS_OUT_OF_RANGE, "export ordinal is outside this executable");
  }
  const hrx_export_s& exp = executable->exports[export_ordinal];
  out_info->name = exp.name.c_str();
  out_info->flags = 0;
  // The note names the kernel. It does not split pointer bindings from the
  // trailing constants block, so those counts stay unpublished.
  out_info->constant_byte_length = 0;
  out_info->binding_count = 0;
  out_info->parameter_count = 0;
  out_info->workgroup_size[0] = 0;
  out_info->workgroup_size[1] = 0;
  out_info->workgroup_size[2] = 0;
  return nullptr;
}

hrx_status_t hrx_executable_lookup_export_by_name(hrx_executable_t executable, const char* name,
                                                 uint32_t* export_ordinal) {
  if (executable == nullptr || name == nullptr || export_ordinal == nullptr) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "executable export lookup");
  }
  for (std::uint32_t i = 0; i < executable->exports.size(); ++i) {
    if (executable->exports[i].name == name) {
      *export_ordinal = i;
      return nullptr;
    }
  }
  return fail(HRX_STATUS_NOT_FOUND, "executable has no export with that name");
}

hrx_status_t hrx_fence_create(size_t capacity, hrx_fence_t* fence) {
  if (fence == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "fence is null");
  *fence = nullptr;
  auto* created = new (std::nothrow) hrx_fence_s;
  if (created == nullptr) return fail(HRX_STATUS_OUT_OF_MEMORY, "fence");
  created->capacity = capacity;
  *fence = created;
  return nullptr;
}

hrx_status_t hrx_fence_create_at(hrx_semaphore_t semaphore, uint64_t value, hrx_fence_t* fence) {
  if (hrx_status_t status = hrx_fence_create(1, fence); !hrx_status_is_ok(status)) return status;
  hrx_status_t status = hrx_fence_insert(*fence, semaphore, value);
  if (!hrx_status_is_ok(status)) {
    hrx_fence_release(*fence);
    *fence = nullptr;
  }
  return status;
}

void hrx_fence_retain(hrx_fence_t fence) {
  if (fence != nullptr) fence->refs.fetch_add(1, std::memory_order_relaxed);
}

void hrx_fence_release(hrx_fence_t fence) {
  if (fence == nullptr) return;
  if (fence->refs.fetch_sub(1, std::memory_order_acq_rel) != 1) return;
  for (hrx_semaphore_t semaphore : fence->semaphores) hrx_semaphore_release(semaphore);
  delete fence;
}

hrx_status_t hrx_fence_insert(hrx_fence_t fence, hrx_semaphore_t semaphore, uint64_t value) {
  if (fence == nullptr || semaphore == nullptr) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "fence insert");
  }
  std::lock_guard lock(fence->mu);
  if (fence->capacity != 0 && fence->semaphores.size() >= fence->capacity) {
    return fail(HRX_STATUS_OUT_OF_RANGE, "fence is full");
  }
  hrx_semaphore_retain(semaphore);
  fence->semaphores.push_back(semaphore);
  fence->values.push_back(value);
  return nullptr;
}

hrx_status_t hrx_fence_extend(hrx_fence_t into_fence, hrx_fence_t from_fence) {
  if (into_fence == nullptr || from_fence == nullptr) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "fence extend");
  }
  std::lock_guard from_lock(from_fence->mu);
  for (std::size_t i = 0; i < from_fence->semaphores.size(); ++i) {
    hrx_status_t status =
        hrx_fence_insert(into_fence, from_fence->semaphores[i], from_fence->values[i]);
    if (!hrx_status_is_ok(status)) return status;
  }
  return nullptr;
}

hrx_status_t hrx_fence_signal(hrx_fence_t fence) {
  if (fence == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "fence is null");
  std::lock_guard lock(fence->mu);
  for (std::size_t i = 0; i < fence->semaphores.size(); ++i) {
    hrx_status_t status = hrx_semaphore_signal(fence->semaphores[i], fence->values[i]);
    if (!hrx_status_is_ok(status)) return status;
  }
  return nullptr;
}

hrx_status_t hrx_fence_wait(hrx_fence_t fence, uint64_t timeout_ns) {
  if (fence == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "fence is null");
  std::vector<hrx_semaphore_t> semaphores;
  std::vector<std::uint64_t> values;
  {
    std::lock_guard lock(fence->mu);
    semaphores = fence->semaphores;
    values = fence->values;
    for (hrx_semaphore_t semaphore : semaphores) hrx_semaphore_retain(semaphore);
  }
  hrx_status_t status = nullptr;
  for (std::size_t i = 0; i < semaphores.size(); ++i) {
    if (hrx_status_is_ok(status)) status = hrx_semaphore_wait(semaphores[i], values[i], timeout_ns);
    hrx_semaphore_release(semaphores[i]);
  }
  return status;
}

hrx_status_t hrx_buffer_view_create(hrx_buffer_t buffer, size_t shape_rank, const int64_t* shape,
                                   hrx_element_type_t element_type, hrx_encoding_type_t encoding_type,
                                   hrx_buffer_view_t* buffer_view) {
  if (buffer_view == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "buffer view is null");
  *buffer_view = nullptr;
  if (buffer == nullptr || (shape_rank != 0 && shape == nullptr)) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "buffer view");
  }
  auto* created = new (std::nothrow) hrx_buffer_view_s;
  if (created == nullptr) return fail(HRX_STATUS_OUT_OF_MEMORY, "buffer view");
  hrx_buffer_retain(buffer);
  created->buffer = buffer;
  created->shape.assign(shape, shape + shape_rank);
  created->element = element_type;
  created->encoding = encoding_type;
  *buffer_view = created;
  return nullptr;
}

void hrx_buffer_view_retain(hrx_buffer_view_t buffer_view) {
  if (buffer_view != nullptr) buffer_view->refs.fetch_add(1, std::memory_order_relaxed);
}

void hrx_buffer_view_release(hrx_buffer_view_t buffer_view) {
  if (buffer_view == nullptr) return;
  if (buffer_view->refs.fetch_sub(1, std::memory_order_acq_rel) != 1) return;
  hrx_buffer_release(buffer_view->buffer);
  delete buffer_view;
}

hrx_status_t hrx_buffer_view_rank(hrx_buffer_view_t buffer_view, size_t* rank) {
  if (buffer_view == nullptr || rank == nullptr) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "buffer view rank");
  }
  *rank = buffer_view->shape.size();
  return nullptr;
}

hrx_status_t hrx_buffer_view_dim(hrx_buffer_view_t buffer_view, size_t dim, int64_t* value) {
  if (buffer_view == nullptr || value == nullptr) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "buffer view dim");
  }
  if (dim >= buffer_view->shape.size()) return fail(HRX_STATUS_OUT_OF_RANGE, "buffer view dim");
  *value = buffer_view->shape[dim];
  return nullptr;
}

hrx_status_t hrx_allocator_query_virtual_memory(hrx_allocator_t, hrx_memory_type_t, bool* supported,
                                               size_t* min_page_size, size_t* recommended_page_size) {
  if (supported != nullptr) *supported = false;
  if (min_page_size != nullptr) *min_page_size = 0;
  if (recommended_page_size != nullptr) *recommended_page_size = 0;
  return fail(HRX_STATUS_UNIMPLEMENTED, "virtual memory is not implemented");
}

hrx_status_t hrx_allocator_virtual_memory_reserve(hrx_allocator_t, hrx_queue_affinity_t, size_t,
                                                 hrx_buffer_t* virtual_buffer) {
  if (virtual_buffer != nullptr) *virtual_buffer = nullptr;
  return fail(HRX_STATUS_UNIMPLEMENTED, "virtual memory is not implemented");
}

hrx_status_t hrx_allocator_virtual_memory_release(hrx_allocator_t, hrx_buffer_t) {
  return fail(HRX_STATUS_UNIMPLEMENTED, "virtual memory is not implemented");
}

hrx_status_t hrx_allocator_physical_memory_allocate(hrx_allocator_t, hrx_memory_type_t, size_t,
                                                   hrx_physical_memory_t* physical) {
  if (physical != nullptr) *physical = nullptr;
  return fail(HRX_STATUS_UNIMPLEMENTED, "virtual memory is not implemented");
}

hrx_status_t hrx_allocator_physical_memory_free(hrx_allocator_t, hrx_physical_memory_t) {
  return fail(HRX_STATUS_UNIMPLEMENTED, "virtual memory is not implemented");
}

hrx_status_t hrx_allocator_virtual_memory_map(hrx_allocator_t, hrx_buffer_t, size_t,
                                             hrx_physical_memory_t, size_t, size_t) {
  return fail(HRX_STATUS_UNIMPLEMENTED, "virtual memory is not implemented");
}

hrx_status_t hrx_allocator_virtual_memory_unmap(hrx_allocator_t, hrx_buffer_t, size_t, size_t) {
  return fail(HRX_STATUS_UNIMPLEMENTED, "virtual memory is not implemented");
}

hrx_status_t hrx_allocator_virtual_memory_protect(hrx_allocator_t, hrx_buffer_t, size_t, size_t,
                                                 hrx_memory_protection_t) {
  return fail(HRX_STATUS_UNIMPLEMENTED, "virtual memory is not implemented");
}

hrx_status_t hrx_allocator_virtual_memory_protect_scoped(hrx_allocator_t, hrx_buffer_t, size_t, size_t,
                                                        hrx_virtual_memory_access_scope_t,
                                                        hrx_memory_protection_t) {
  return fail(HRX_STATUS_UNIMPLEMENTED, "virtual memory is not implemented");
}

hrx_status_t hrx_event_create(hrx_device_t device, hrx_event_flags_t flags, hrx_event_t* out_event) {
  if (out_event == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "event is null");
  *out_event = nullptr;
  if (hrx_status_t status = require_device(device); !hrx_status_is_ok(status)) return status;
  auto* created = new (std::nothrow) hrx_event_s;
  if (created == nullptr) return fail(HRX_STATUS_OUT_OF_MEMORY, "event");
  hrx_device_retain(device);
  created->device = device;
  created->flags = flags;
  *out_event = created;
  return nullptr;
}

void hrx_event_retain(hrx_event_t event) {
  if (event != nullptr) event->refs.fetch_add(1, std::memory_order_relaxed);
}

void hrx_event_release(hrx_event_t event) {
  if (event == nullptr) return;
  if (event->refs.fetch_sub(1, std::memory_order_acq_rel) != 1) return;
  hrx_stream_release(event->stream);
  hrx_device_release(event->device);
  delete event;
}

hrx_status_t hrx_event_record(hrx_event_t event, hrx_stream_t stream) {
  if (event == nullptr || stream == nullptr) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "event record");
  }
  event->host_ns = host_ns();
  if (hrx_status_t status = flush_stream(stream); !hrx_status_is_ok(status)) return status;
  if (event->stream != nullptr) hrx_stream_release(event->stream);
  hrx_stream_retain(stream);
  event->stream = stream;
  event->timepoint = stream->timeline->value.load(std::memory_order_acquire);
  event->recorded = true;
  return nullptr;
}

hrx_status_t hrx_event_query(hrx_event_t event, bool* complete) {
  if (event == nullptr || complete == nullptr) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "event query");
  }
  if (!event->recorded || event->stream == nullptr) {
    *complete = false;
    return nullptr;
  }
  const std::uint64_t done = event->stream->timeline->value.load(std::memory_order_acquire);
  *complete = done >= event->timepoint;
  return nullptr;
}

hrx_status_t hrx_event_synchronize(hrx_event_t event) {
  if (event == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "event is null");
  if (!event->recorded || event->stream == nullptr) {
    return fail(HRX_STATUS_FAILED_PRECONDITION, "event was not recorded");
  }
  return hrx_semaphore_wait(event->stream->timeline, event->timepoint, UINT64_MAX);
}

hrx_status_t hrx_event_elapsed_time(hrx_event_t start, hrx_event_t stop, float* ms) {
  if (start == nullptr || stop == nullptr || ms == nullptr) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "elapsed time");
  }
  if ((start->flags & HRX_EVENT_FLAG_DISABLE_TIMING) != 0 ||
      (stop->flags & HRX_EVENT_FLAG_DISABLE_TIMING) != 0) {
    return fail(HRX_STATUS_UNAVAILABLE, "event timing is disabled");
  }
  if (!start->recorded || !stop->recorded) {
    return fail(HRX_STATUS_FAILED_PRECONDITION, "event was not recorded");
  }
  const double delta_ns = static_cast<double>(stop->host_ns - start->host_ns);
  *ms = static_cast<float>(delta_ns / 1.0e6);
  return nullptr;
}

hrx_status_t hrx_stream_wait_event(hrx_stream_t stream, hrx_event_t event) {
  if (stream == nullptr || event == nullptr) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "wait event");
  }
  if (!event->recorded || event->stream == nullptr) {
    return fail(HRX_STATUS_FAILED_PRECONDITION, "event was not recorded");
  }
  hrx_timeline_point_t point;
  point.semaphore = event->stream->timeline;
  point.value = event->timepoint;
  return hrx_stream_wait_on(stream, point);
}

hrx_status_t hrx_device_memory_info(hrx_device_t device, size_t* free_bytes, size_t* total_bytes) {
  if (hrx_status_t status = require_device(device); !hrx_status_is_ok(status)) return status;
  if (free_bytes == nullptr || total_bytes == nullptr) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "memory info output is null");
  }
  std::uint64_t available = 0;
  HsaAgent agent{device->agent};
  if (!agent_info(agent, kInfoMemAvail, &available, sizeof(available))) {
    return fail(HRX_STATUS_UNAVAILABLE, "Windows HSA runtime did not publish available memory");
  }
  *free_bytes = static_cast<std::size_t>(available);
  // The DLL's only memory attribute is bytes available. Zero here means the
  // total was not published. HRX_DEVICE_PROPERTY_TOTAL_MEMORY stays unavailable
  // so this zero is not stored as a measured total.
  *total_bytes = 0;
  return nullptr;
}

hrx_status_t hrx_device_can_access_peer(hrx_device_t device_a, hrx_device_t device_b, bool* can_access) {
  if (can_access == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "can_access is null");
  *can_access = false;
  if (hrx_status_t status = require_device(device_a); !hrx_status_is_ok(status)) return status;
  if (hrx_status_t status = require_device(device_b); !hrx_status_is_ok(status)) return status;
  if (device_a == device_b) {
    *can_access = true;
    return nullptr;
  }
  if (!device_b->has_pool || !g_hsa.ready) {
    return fail(HRX_STATUS_UNAVAILABLE, "peer pool is not published");
  }
  int access = 0;
  HsaAgent agent{device_a->agent};
  HsaPool pool{device_b->pool};
  const int rc = g_hsa.agent_pool_info(agent, pool, 0, &access);
  if (rc != kHsaOk) return fail_hsa("hsa_amd_agent_memory_pool_get_info", rc);
  *can_access = access == 1;
  return nullptr;
}

hrx_status_t hrx_host_memory_register(hrx_device_t device, void* host_ptr, size_t size, uint32_t) {
  if (hrx_status_t status = require_device(device); !hrx_status_is_ok(status)) return status;
  if (host_ptr == nullptr || size == 0) return fail(HRX_STATUS_INVALID_ARGUMENT, "host register");
  void* locked = nullptr;
  return lock_memory(device, host_ptr, size, &locked);
}

hrx_status_t hrx_host_memory_unregister(hrx_device_t device, void* host_ptr) {
  if (hrx_status_t status = require_device(device); !hrx_status_is_ok(status)) return status;
  if (host_ptr == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "host unregister");
  if (!device->gpu) return nullptr;
  const int rc = g_hsa.unlock(host_ptr);
  if (rc != kHsaOk) return fail_hsa("hsa_amd_memory_unlock", rc);
  return nullptr;
}

hrx_status_t hrx_buffer_lookup(hrx_device_t device, const void* device_ptr, hrx_buffer_t* buffer,
                              size_t* offset) {
  if (buffer == nullptr || offset == nullptr) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "buffer lookup");
  }
  *buffer = nullptr;
  *offset = 0;
  if (hrx_status_t status = require_device(device); !hrx_status_is_ok(status)) return status;
  const auto address = reinterpret_cast<std::uintptr_t>(device_ptr);
  std::lock_guard lock(g_buffers_mu);
  for (hrx_buffer_t candidate : g_buffers) {
    if (candidate->device != device || candidate->dev == nullptr) continue;
    const auto base = reinterpret_cast<std::uintptr_t>(candidate->dev);
    if (address >= base && address < base + candidate->size) {
      hrx_buffer_retain(candidate);
      *buffer = candidate;
      *offset = static_cast<std::size_t>(address - base);
      return nullptr;
    }
  }
  return fail(HRX_STATUS_NOT_FOUND, "no buffer owns that address");
}

hrx_status_t hrx_stream_copy_h2d(hrx_stream_t stream, const void* host_src, hrx_buffer_t dst,
                                size_t dst_offset, size_t size) {
  return hrx_stream_update_buffer(stream, host_src, size, dst, dst_offset);
}

hrx_status_t hrx_stream_copy_d2h(hrx_stream_t stream, hrx_buffer_t src, size_t src_offset,
                                void* host_dst, size_t size) {
  if (stream == nullptr || src == nullptr || host_dst == nullptr) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "d2h");
  }
  if (hrx_status_t status = flush_stream(stream); !hrx_status_is_ok(status)) return status;
  return hrx_synchronous_d2h(stream->device, src, src_offset, host_dst, size);
}

hrx_status_t hrx_mem_pool_create(hrx_device_t device, const hrx_mem_pool_props_t* props,
                                hrx_mem_pool_t* out_pool) {
  if (out_pool == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "pool is null");
  *out_pool = nullptr;
  if (hrx_status_t status = require_device(device); !hrx_status_is_ok(status)) return status;
  auto* created = new (std::nothrow) hrx_mem_pool_s;
  if (created == nullptr) return fail(HRX_STATUS_OUT_OF_MEMORY, "memory pool");
  hrx_device_retain(device);
  created->device = device;
  if (props != nullptr) created->props = *props;
  *out_pool = created;
  return nullptr;
}

void hrx_mem_pool_retain(hrx_mem_pool_t pool) {
  if (pool != nullptr) pool->refs.fetch_add(1, std::memory_order_relaxed);
}

void hrx_mem_pool_release(hrx_mem_pool_t pool) {
  if (pool == nullptr) return;
  if (pool->refs.fetch_sub(1, std::memory_order_acq_rel) != 1) return;
  hrx_device_release(pool->device);
  delete pool;
}

hrx_status_t hrx_mem_pool_get_attribute(hrx_mem_pool_t pool, hrx_mem_pool_attr_t attr,
                                       uint64_t* out_value) {
  if (pool == nullptr || out_value == nullptr) {
    return fail(HRX_STATUS_INVALID_ARGUMENT, "pool attribute");
  }
  std::lock_guard lock(pool->mu);
  switch (attr) {
    case HRX_MEM_POOL_ATTR_RELEASE_THRESHOLD:
      *out_value = pool->release_threshold;
      return nullptr;
    case HRX_MEM_POOL_ATTR_RESERVED_MEM_CURRENT:
      *out_value = pool->reserved;
      return nullptr;
    case HRX_MEM_POOL_ATTR_RESERVED_MEM_HIGH:
      *out_value = pool->reserved_high;
      return nullptr;
    case HRX_MEM_POOL_ATTR_USED_MEM_CURRENT:
      *out_value = pool->used;
      return nullptr;
    case HRX_MEM_POOL_ATTR_USED_MEM_HIGH:
      *out_value = pool->used_high;
      return nullptr;
    default:
      *out_value = 0;
      return nullptr;
  }
}

hrx_status_t hrx_mem_pool_set_attribute(hrx_mem_pool_t pool, hrx_mem_pool_attr_t attr, uint64_t value) {
  if (pool == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "pool is null");
  std::lock_guard lock(pool->mu);
  if (attr == HRX_MEM_POOL_ATTR_RELEASE_THRESHOLD) pool->release_threshold = value;
  if (attr == HRX_MEM_POOL_ATTR_RESERVED_MEM_HIGH && value == 0) pool->reserved_high = 0;
  if (attr == HRX_MEM_POOL_ATTR_USED_MEM_HIGH && value == 0) pool->used_high = 0;
  return nullptr;
}

hrx_status_t hrx_mem_pool_trim(hrx_mem_pool_t pool, size_t) {
  if (pool == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "pool is null");
  return nullptr;
}

hrx_status_t hrx_mem_pool_release_unused(hrx_mem_pool_t pool) {
  if (pool == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "pool is null");
  return nullptr;
}

void hrx_mem_pool_record_logical_allocation(hrx_mem_pool_t pool, size_t size) {
  if (pool == nullptr) return;
  std::lock_guard lock(pool->mu);
  pool->used += static_cast<std::uint64_t>(size);
  pool->reserved += static_cast<std::uint64_t>(size);
  if (pool->used > pool->used_high) pool->used_high = pool->used;
  if (pool->reserved > pool->reserved_high) pool->reserved_high = pool->reserved;
}

void hrx_mem_pool_record_logical_free(hrx_mem_pool_t pool, size_t size) {
  if (pool == nullptr) return;
  std::lock_guard lock(pool->mu);
  const auto bytes = static_cast<std::uint64_t>(size);
  if (bytes > pool->used) pool->used = 0;
  else pool->used -= bytes;
}

hrx_status_t hrx_mem_pool_allocate_buffer(hrx_mem_pool_t pool, size_t size, hrx_buffer_t* buffer) {
  if (pool == nullptr) return fail(HRX_STATUS_INVALID_ARGUMENT, "pool is null");
  hrx_buffer_params_t params{};
  params.type = HRX_MEMORY_TYPE_DEVICE_LOCAL;
  params.access = HRX_MEMORY_ACCESS_ALL;
  params.usage = HRX_BUFFER_USAGE_DEFAULT;
  hrx_status_t status =
      hrx_allocator_allocate_buffer(pool->device->allocator, params, size, buffer);
  if (hrx_status_is_ok(status)) hrx_mem_pool_record_logical_allocation(pool, size);
  return status;
}

hrx_status_t hrx_graph_create(hrx_device_t, uint32_t, hrx_graph_t* out_graph) {
  if (out_graph != nullptr) *out_graph = nullptr;
  return fail(HRX_STATUS_UNIMPLEMENTED, "graphs are not implemented");
}
void hrx_graph_retain(hrx_graph_t) {}
void hrx_graph_release(hrx_graph_t) {}
hrx_status_t hrx_graph_add_empty_node(hrx_graph_t, const hrx_graph_node_t*, size_t, hrx_graph_node_t*) {
  return fail(HRX_STATUS_UNIMPLEMENTED, "graphs are not implemented");
}
hrx_status_t hrx_graph_add_kernel_node(hrx_graph_t, const hrx_graph_node_t*, size_t,
                                      const hrx_graph_kernel_node_attrs_t*, hrx_graph_node_t*) {
  return fail(HRX_STATUS_UNIMPLEMENTED, "graphs are not implemented");
}
hrx_status_t hrx_graph_add_copy_ptr_node(hrx_graph_t, const hrx_graph_node_t*, size_t,
                                        const hrx_graph_copy_ptr_node_attrs_t*, hrx_graph_node_t*) {
  return fail(HRX_STATUS_UNIMPLEMENTED, "graphs are not implemented");
}
hrx_status_t hrx_graph_add_fill_ptr_node(hrx_graph_t, const hrx_graph_node_t*, size_t,
                                        const hrx_graph_fill_ptr_node_attrs_t*, hrx_graph_node_t*) {
  return fail(HRX_STATUS_UNIMPLEMENTED, "graphs are not implemented");
}
hrx_status_t hrx_graph_add_copy_buffer_node(hrx_graph_t, const hrx_graph_node_t*, size_t,
                                           const hrx_graph_copy_buffer_node_attrs_t*,
                                           hrx_graph_node_t*) {
  return fail(HRX_STATUS_UNIMPLEMENTED, "graphs are not implemented");
}
hrx_status_t hrx_graph_add_fill_buffer_node(hrx_graph_t, const hrx_graph_node_t*, size_t,
                                           const hrx_graph_fill_buffer_node_attrs_t*,
                                           hrx_graph_node_t*) {
  return fail(HRX_STATUS_UNIMPLEMENTED, "graphs are not implemented");
}
hrx_status_t hrx_graph_add_host_call_node(hrx_graph_t, const hrx_graph_node_t*, size_t,
                                         const hrx_graph_host_call_node_attrs_t*, hrx_graph_node_t*) {
  return fail(HRX_STATUS_UNIMPLEMENTED, "graphs are not implemented");
}
hrx_status_t hrx_graph_add_dependencies(hrx_graph_t, const hrx_graph_node_t*, const hrx_graph_node_t*,
                                       size_t) {
  return fail(HRX_STATUS_UNIMPLEMENTED, "graphs are not implemented");
}
hrx_status_t hrx_graph_size(hrx_graph_t, size_t*) {
  return fail(HRX_STATUS_UNIMPLEMENTED, "graphs are not implemented");
}
hrx_status_t hrx_graph_get_nodes(hrx_graph_t, hrx_graph_node_t*, size_t*) {
  return fail(HRX_STATUS_UNIMPLEMENTED, "graphs are not implemented");
}
hrx_status_t hrx_graph_instantiate(hrx_graph_t, uint32_t, hrx_graph_exec_t* out_exec) {
  if (out_exec != nullptr) *out_exec = nullptr;
  return fail(HRX_STATUS_UNIMPLEMENTED, "graphs are not implemented");
}
void hrx_graph_exec_retain(hrx_graph_exec_t) {}
void hrx_graph_exec_release(hrx_graph_exec_t) {}
hrx_status_t hrx_graph_exec_launch(hrx_graph_exec_t, hrx_stream_t) {
  return fail(HRX_STATUS_UNIMPLEMENTED, "graphs are not implemented");
}
hrx_status_t hrx_graph_exec_update(hrx_graph_exec_t, hrx_graph_t) {
  return fail(HRX_STATUS_UNIMPLEMENTED, "graphs are not implemented");
}
hrx_status_t hrx_stream_begin_capture(hrx_stream_t, hrx_capture_mode_t) {
  return fail(HRX_STATUS_UNIMPLEMENTED, "stream capture is not implemented");
}
hrx_status_t hrx_stream_end_capture(hrx_stream_t, hrx_graph_t* out_graph) {
  if (out_graph != nullptr) *out_graph = nullptr;
  return fail(HRX_STATUS_UNIMPLEMENTED, "stream capture is not implemented");
}
hrx_capture_status_t hrx_stream_capture_status(hrx_stream_t) { return HRX_CAPTURE_STATUS_NONE; }
bool hrx_stream_is_capturing(hrx_stream_t) { return false; }

}  // extern "C"
