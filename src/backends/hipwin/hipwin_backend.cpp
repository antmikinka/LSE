// Windows GPU backend. Allocations are host-mapped HIP memory: `ptr` is a
// host address the interpreter can read, and `handle` is the device pointer
// hipHostGetDevicePointer returns. This GPU does not have unified addressing,
// so a hipMalloc pointer is not a host address.
//
// A launch passes one device pointer per binding, then LseConstants by value.
// The HIP emitter always declares that struct (unsigned int _unused when the
// layout is empty). The last kernelParam points at the struct bytes. The
// launch is synchronized before that argument storage dies.

#include "hipwin_backend.hpp"

#include "lse/backends/hrx/arch_database.hpp"

#include <hip/hip_runtime_api.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace lse::backend {

namespace {

bool wgp_paired(const char* arch) {
  return arch != nullptr &&
         (std::strncmp(arch, "gfx11", 5) == 0 || std::strncmp(arch, "gfx12", 5) == 0);
}

// HIP's multiprocessor on RDNA is the WGP. DeviceInfo records that two CUs
// share one. The count this backend publishes is CUs.
std::uint32_t compute_units(const hipDeviceProp_t& prop) {
  if (prop.multiProcessorCount <= 0) return 0;
  const auto mp = static_cast<std::uint32_t>(prop.multiProcessorCount);
  return wgp_paired(prop.gcnArchName) ? mp * 2u : mp;
}

std::uint16_t narrow_u16(std::uint32_t v) {
  return v > 0xFFFFu ? std::uint16_t{0} : static_cast<std::uint16_t>(v);
}

std::uint32_t lds_bytes(const hipDeviceProp_t& prop) {
  if (prop.sharedMemPerBlock > 0xFFFFFFFFu) return 0;
  return static_cast<std::uint32_t>(prop.sharedMemPerBlock);
}

std::uint16_t positive_u16(int v) {
  if (v <= 0 || v > 65535) return 0;
  return static_cast<std::uint16_t>(v);
}

Status hip_fail(const char* what, hipError_t err) {
  const char* text = hipGetErrorString(err);
  return LSE_ERROR(kDeviceError, "hipwin ", what, ": ",
                   text != nullptr ? text : "unknown HIP error");
}

void note(std::string* declined, const char* item) {
  if (!declined->empty()) *declined += "; ";
  *declined += item;
}

std::string uuid_text(const hipUUID& uuid) {
  bool ascii = true;
  for (int i = 0; i < 16; ++i) {
    const auto b = static_cast<unsigned char>(uuid.bytes[i]);
    if (b < 0x20 || b > 0x7e) ascii = false;
  }
  // This runtime returns the id as 16 ASCII characters, not 16 raw bytes.
  if (ascii) return std::string(uuid.bytes, uuid.bytes + 16);
  std::string text(32, '0');
  static constexpr char kHex[] = "0123456789abcdef";
  for (int i = 0; i < 16; ++i) {
    const auto b = static_cast<unsigned char>(uuid.bytes[i]);
    const auto at = static_cast<std::size_t>(i) * 2u;
    text[at] = kHex[b >> 4];
    text[at + 1] = kHex[b & 0x0fu];
  }
  return text;
}

std::string pci_path_of(const hipDeviceProp_t& prop) {
  if (prop.pciDomainID < 0 || prop.pciBusID < 0 || prop.pciDeviceID < 0) return {};
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%04x:%02x:%02x.0",
                static_cast<unsigned>(prop.pciDomainID),
                static_cast<unsigned>(prop.pciBusID),
                static_cast<unsigned>(prop.pciDeviceID));
  return buf;
}

struct MappedFree {
  int ordinal = 0;
  void operator()(void* p) const noexcept {
    if (p == nullptr) return;
    (void)hipSetDevice(ordinal);
    (void)hipHostFree(p);
  }
};

DeviceDescriptor describe_device(int ordinal, int count, const hipDeviceProp_t& prop) {
  DeviceDescriptor d;
  d.backend = std::string(HipWinBackend::kName);
  d.ordinal = ordinal;
  if (prop.name[0] != '\0') d.product = DeviceFact<std::string>::queried(prop.name);
  if (prop.gcnArchName[0] != '\0') {
    d.arch = DeviceFact<std::string>::queried(prop.gcnArchName);
  } else {
    note(&d.declined, "gcnArchName was empty");
  }
  d.total_memory = DeviceFact<std::size_t>::queried(prop.totalGlobalMem);
  const std::uint32_t cus = compute_units(prop);
  if (cus != 0) d.compute_units = DeviceFact<std::uint32_t>::queried(cus);
  const std::uint16_t threads = positive_u16(prop.maxThreadsPerBlock);
  if (threads != 0) {
    d.max_threads_per_workgroup = DeviceFact<std::uint32_t>::queried(threads);
  }
  const std::uint16_t wave = positive_u16(prop.warpSize);
  if (wave != 0) d.wavefront_size = DeviceFact<std::uint32_t>::queried(wave);
  const std::uint32_t lds = lds_bytes(prop);
  if (lds != 0) d.lds_bytes_per_workgroup = DeviceFact<std::uint32_t>::queried(lds);
  d.unified_memory = DeviceFact<bool>::queried(false);
  note(&d.declined, "hardware queue count: HIP does not report it");

  hipUUID uuid{};
  if (hipDeviceGetUuid(&uuid, ordinal) == hipSuccess) {
    d.uuid = DeviceFact<std::string>::queried(uuid_text(uuid));
  } else {
    note(&d.declined, "uuid: hipDeviceGetUuid failed");
  }
  const std::string pci = pci_path_of(prop);
  if (!pci.empty()) d.pci_path = DeviceFact<std::string>::queried(pci);
  else note(&d.declined, "pci path: HIP reported a negative id");

  if (hipSetDevice(ordinal) == hipSuccess) {
    std::size_t free_b = 0;
    std::size_t total_b = 0;
    if (hipMemGetInfo(&free_b, &total_b) == hipSuccess) {
      d.free_memory = DeviceFact<std::size_t>::queried(free_b);
    } else {
      note(&d.declined, "free memory: hipMemGetInfo failed");
    }
  } else {
    note(&d.declined, "free memory: hipSetDevice failed");
  }

  d.peers.assign(static_cast<std::size_t>(count > 0 ? count : 0), PeerAccess::kUnknown);
  if (ordinal >= 0 && ordinal < count) d.peers[static_cast<std::size_t>(ordinal)] = PeerAccess::kSelf;
  return d;
}

}  // namespace

HipWinBackend::HipWinBackend() = default;

HipWinBackend::~HipWinBackend() { shutdown_impl(); }

Status HipWinBackend::init_impl(int device_ordinal) {
  if (up_) {
    if (ordinal_ == device_ordinal) return OkStatus();
    return LSE_ERROR(kAlreadyExists, "hipwin is already bound to another device");
  }
  int count = 0;
  hipError_t err = hipGetDeviceCount(&count);
  if (err != hipSuccess) return hip_fail("hipGetDeviceCount", err);
  if (count < 1) return LSE_ERROR(kNotFound, "hipwin: HIP reports no devices");
  if (device_ordinal < 0 || device_ordinal >= count || device_ordinal > 255) {
    return LSE_ERROR(kInvalidArgument, "hipwin device ordinal ",
                     std::to_string(device_ordinal), " is outside 0..",
                     std::to_string(count - 1));
  }
  err = hipSetDevice(device_ordinal);
  if (err != hipSuccess) return hip_fail("hipSetDevice", err);
  hipDeviceProp_t prop{};
  err = hipGetDeviceProperties(&prop, device_ordinal);
  if (err != hipSuccess) return hip_fail("hipGetDeviceProperties", err);
  if (prop.gcnArchName[0] == '\0') {
    return LSE_ERROR(kDeviceError, "hipwin: HIP reported a device with no gcnArchName");
  }

  info_ = DeviceInfo{};
  info_.name = prop.name;
  info_.arch = prop.gcnArchName;
  info_.total_memory = prop.totalGlobalMem;
  info_.lds_bytes_per_workgroup = lds_bytes(prop);
  info_.compute_units = narrow_u16(compute_units(prop));
  info_.max_threads_per_workgroup = positive_u16(prop.maxThreadsPerBlock);
  info_.wavefront_size = positive_u16(prop.warpSize);
  info_.max_waves_per_cu = 0;
  info_.cus_per_lds_pool = wgp_paired(prop.gcnArchName) ? std::uint8_t{2} : std::uint8_t{1};
  info_.ordinal = static_cast<std::uint8_t>(device_ordinal);
  // Host-mapped bytes are one allocation with two addresses. Uploads are
  // still copies: the host address and the device address are not the same
  // pointer on this GPU.
  info_.unified_memory = false;
  // quant_linear and the other AMD kernels refuse a device with no AMD
  // extension. HIP answered the live counts above; the family row fills the
  // ISA facts (matrix core, dot4, load width) and does not invent a board.
  apply_arch_defaults(info_, amd_);
  info_.extension_id = AmdDeviceInfo::kExtensionId;
  info_.extension = &amd_;
  ordinal_ = device_ordinal;
  up_ = true;
  return OkStatus();
}

void HipWinBackend::shutdown_impl() noexcept {
  for (auto& mod : modules_) {
    if (mod == nullptr || mod->module == nullptr) continue;
    (void)hipSetDevice(ordinal_);
    (void)hipModuleUnload(reinterpret_cast<hipModule_t>(mod->module));
    mod->module = nullptr;
  }
  modules_.clear();
  up_ = false;
}

Result<std::vector<DeviceDescriptor>> HipWinBackend::enumerate_devices() {
  int count = 0;
  const hipError_t err = hipGetDeviceCount(&count);
  if (err != hipSuccess) return hip_fail("hipGetDeviceCount", err);
  if (count < 1) return std::vector<DeviceDescriptor>{};
  std::vector<DeviceDescriptor> out;
  out.reserve(static_cast<std::size_t>(count));
  for (int i = 0; i < count; ++i) {
    hipDeviceProp_t prop{};
    const hipError_t prop_err = hipGetDeviceProperties(&prop, i);
    if (prop_err != hipSuccess) return hip_fail("hipGetDeviceProperties", prop_err);
    out.push_back(describe_device(i, count, prop));
  }
  return out;
}

Status HipWinBackend::bind_device() const {
  if (!up_) return LSE_ERROR(kInternal, "hipwin is not initialized");
  const hipError_t err = hipSetDevice(ordinal_);
  if (err != hipSuccess) return hip_fail("hipSetDevice", err);
  return OkStatus();
}

void* HipWinBackend::device_address(const DeviceBuffer& buf) const noexcept {
  if (buf.handle == 0) return nullptr;
  return reinterpret_cast<void*>(buf.handle + static_cast<std::uint64_t>(buf.offset));
}

Result<DeviceBuffer> HipWinBackend::allocate_impl(std::size_t bytes, MemoryClass, Stream) {
  if (!up_) return LSE_ERROR(kInternal, "hipwin is not initialized");
  if (bytes == 0) return LSE_ERROR(kInvalidArgument, "zero-size allocation");
  LSE_RETURN_IF_ERROR(bind_device());
  void* host = nullptr;
  const unsigned int coherent = hipHostMallocMapped | hipHostMallocCoherent;
  hipError_t err = hipHostMalloc(&host, bytes, coherent);
  if (err != hipSuccess) err = hipHostMalloc(&host, bytes, hipHostMallocMapped);
  if (err != hipSuccess || host == nullptr) {
    const char* text = hipGetErrorString(err);
    return LSE_ERROR(kOutOfMemory, "hipHostMalloc failed for ", std::to_string(bytes),
                     " bytes: ", text != nullptr ? text : "unknown HIP error");
  }
  void* dev = nullptr;
  err = hipHostGetDevicePointer(&dev, host, 0);
  if (err != hipSuccess || dev == nullptr) {
    (void)hipHostFree(host);
    return hip_fail("hipHostGetDevicePointer", err);
  }
  DeviceBuffer buf;
  buf.ptr = host;
  buf.size_bytes = bytes;
  buf.handle = reinterpret_cast<std::uint64_t>(dev);
  buf.storage = std::shared_ptr<void>(host, MappedFree{ordinal_});
  return buf;
}

void HipWinBackend::deallocate_impl(DeviceBuffer& buf) noexcept {
  if (buf.storage) {
    buf.storage.reset();
  } else if (buf.ptr != nullptr) {
    (void)hipSetDevice(ordinal_);
    (void)hipHostFree(buf.ptr);
  }
  buf = {};
}

Result<std::size_t> HipWinBackend::sample_free_memory_impl() const {
  LSE_RETURN_IF_ERROR(bind_device());
  std::size_t free_b = 0;
  std::size_t total_b = 0;
  const hipError_t err = hipMemGetInfo(&free_b, &total_b);
  if (err != hipSuccess) return hip_fail("hipMemGetInfo", err);
  (void)total_b;
  return free_b;
}

Status HipWinBackend::transfer(void* dst, const void* src, std::size_t bytes) {
  if (bytes == 0) return OkStatus();
  if (dst == nullptr || src == nullptr) {
    return LSE_ERROR(kInvalidArgument, "hipwin copy has a null side");
  }
  LSE_RETURN_IF_ERROR(bind_device());
  const hipError_t err = hipMemcpy(dst, src, bytes, hipMemcpyDefault);
  if (err != hipSuccess) return hip_fail("hipMemcpy", err);
  return OkStatus();
}

Status HipWinBackend::copy_h2d_impl(const void* src, DeviceBuffer& dst, std::size_t bytes,
                                   std::size_t dst_offset) {
  if (src == nullptr || !dst.valid()) {
    return LSE_ERROR(kInvalidArgument, "null buffer in copy_h2d");
  }
  if (dst_offset > dst.size_bytes || bytes > dst.size_bytes - dst_offset) {
    return LSE_ERROR(kOutOfRange, "copy_h2d writes past the end of the buffer");
  }
  auto* dev = static_cast<std::byte*>(device_address(dst));
  if (dev == nullptr) return LSE_ERROR(kInvalidArgument, "hipwin buffer has no device address");
  return transfer(dev + dst_offset, src, bytes);
}

Status HipWinBackend::copy_d2h_impl(const DeviceBuffer& src, void* dst, std::size_t bytes,
                                   std::size_t src_offset) {
  if (dst == nullptr || !src.valid()) {
    return LSE_ERROR(kInvalidArgument, "null buffer in copy_d2h");
  }
  if (src_offset > src.size_bytes || bytes > src.size_bytes - src_offset) {
    return LSE_ERROR(kOutOfRange, "copy_d2h reads past the end of the buffer");
  }
  auto* dev = static_cast<std::byte*>(device_address(src));
  if (dev == nullptr) return LSE_ERROR(kInvalidArgument, "hipwin buffer has no device address");
  return transfer(dst, dev + src_offset, bytes);
}

Status HipWinBackend::copy_ordered_impl(const DeviceBuffer& src, DeviceBuffer& dst,
                                       std::size_t bytes, std::size_t src_offset,
                                       std::size_t dst_offset) {
  if (!src.valid() || !dst.valid()) {
    return LSE_ERROR(kInvalidArgument, "null buffer in copy_ordered");
  }
  if (src_offset > src.size_bytes || bytes > src.size_bytes - src_offset ||
      dst_offset > dst.size_bytes || bytes > dst.size_bytes - dst_offset) {
    return LSE_ERROR(kOutOfRange, "copy_ordered exceeds a buffer");
  }
  auto* from = static_cast<std::byte*>(device_address(src));
  auto* to = static_cast<std::byte*>(device_address(dst));
  if (from == nullptr || to == nullptr) {
    return LSE_ERROR(kInvalidArgument, "hipwin buffer has no device address");
  }
  return transfer(to + dst_offset, from + src_offset, bytes);
}

Status HipWinBackend::copy_peer_impl(const DeviceBuffer& src, DeviceBuffer& dst,
                                    std::size_t bytes, std::size_t src_offset,
                                    std::size_t dst_offset) {
  if (!src.valid() || !dst.valid()) {
    return LSE_ERROR(kInvalidArgument, "null buffer in copy_peer");
  }
  if (src_offset > src.size_bytes || bytes > src.size_bytes - src_offset ||
      dst_offset > dst.size_bytes || bytes > dst.size_bytes - dst_offset) {
    return LSE_ERROR(kOutOfRange, "copy_peer exceeds a buffer");
  }
  if (bytes == 0) return OkStatus();
  // Coherent host mappings of one allocation can overlap. hipMemcpy does not
  // define that case; memmove on the host addresses is the same bytes.
  if (src.ptr != nullptr && dst.ptr != nullptr) {
    const auto* from = static_cast<const std::byte*>(src.ptr) + src.offset + src_offset;
    auto* to = static_cast<std::byte*>(dst.ptr) + dst.offset + dst_offset;
    const auto* lo = from < to ? from : static_cast<const std::byte*>(to);
    const auto* hi = from < to ? static_cast<const std::byte*>(to) : from;
    if (static_cast<std::size_t>(hi - lo) < bytes) {
      std::memmove(to, from, bytes);
      return OkStatus();
    }
  }
  return copy_ordered_impl(src, dst, bytes, src_offset, dst_offset);
}

Status HipWinBackend::write_ordered_impl(DeviceBuffer& dst, const void* src, std::size_t bytes,
                                        std::size_t dst_offset) {
  return copy_h2d_impl(src, dst, bytes, dst_offset);
}

Status HipWinBackend::zero_ordered_impl(DeviceBuffer& dst, std::size_t bytes,
                                       std::size_t dst_offset) {
  if (!dst.valid()) return LSE_ERROR(kInvalidArgument, "null buffer in zero_ordered");
  if (dst_offset > dst.size_bytes || bytes > dst.size_bytes - dst_offset) {
    return LSE_ERROR(kOutOfRange, "zero_ordered writes past the end of the buffer");
  }
  if (bytes == 0) return OkStatus();
  auto* dev = static_cast<std::byte*>(device_address(dst));
  if (dev == nullptr) return LSE_ERROR(kInvalidArgument, "hipwin buffer has no device address");
  LSE_RETURN_IF_ERROR(bind_device());
  const hipError_t err = hipMemset(dev + dst_offset, 0, bytes);
  if (err != hipSuccess) return hip_fail("hipMemset", err);
  return OkStatus();
}

Result<std::vector<KernelHandle>> HipWinBackend::load_executables_impl(
    std::span<const std::string> names, std::span<const std::byte> code_object) {
  if (!up_) return LSE_ERROR(kInternal, "hipwin is not initialized");
  if (names.empty() || code_object.empty()) {
    return LSE_ERROR(kInvalidArgument, "hipwin load needs a name and a code object");
  }
  LSE_RETURN_IF_ERROR(bind_device());
  hipModule_t module = nullptr;
  const hipError_t loaded = hipModuleLoadData(&module, code_object.data());
  if (loaded != hipSuccess || module == nullptr) return hip_fail("hipModuleLoadData", loaded);

  auto owned = std::make_unique<Module>();
  owned->module = module;
  std::vector<KernelHandle> handles;
  handles.reserve(names.size());
  for (std::size_t i = 0; i < names.size(); ++i) {
    hipFunction_t fn = nullptr;
    const hipError_t got = hipModuleGetFunction(&fn, module, names[i].c_str());
    if (got != hipSuccess || fn == nullptr) {
      (void)hipModuleUnload(module);
      owned->module = nullptr;
      const char* text = hipGetErrorString(got);
      return LSE_ERROR(kNotFound, "hipwin has no export '", names[i], "': ",
                       text != nullptr ? text : "unknown HIP error");
    }
    owned->functions.push_back(fn);
    KernelHandle handle;
    handle.export_ordinal = static_cast<std::uint32_t>(i);
    handle.name = names[i];
    handles.push_back(std::move(handle));
  }
  modules_.push_back(std::move(owned));
  const auto id = static_cast<std::uint64_t>(modules_.size());
  for (KernelHandle& handle : handles) handle.executable = id;
  return handles;
}

Result<KernelHandle> HipWinBackend::load_executable_impl(std::string_view name,
                                                        std::span<const std::byte> code_object) {
  const std::string one{name};
  const std::span<const std::string> names(&one, 1);
  auto handles = load_executables_impl(names, code_object);
  if (!handles.ok()) return handles.status();
  return handles->front();
}

Status HipWinBackend::launch_impl(const KernelHandle& kernel, const LaunchDims& dims,
                                 const DispatchArgs& args, const DispatchTarget& target) {
  if (!up_) return LSE_ERROR(kInternal, "hipwin is not initialized");
  if (!target.work.whole()) {
    return LSE_ERROR(kUnimplemented, "hipwin cannot split a dispatch's work range");
  }
  if (target.stream.index != 0) {
    return LSE_ERROR(kInvalidArgument, "hipwin has one stream");
  }
  for (std::uint32_t dim : dims.workgroup_count) {
    if (dim == 0) return LSE_ERROR(kInvalidArgument, "hipwin workgroup count is zero");
  }
  for (std::uint32_t dim : dims.workgroup_size) {
    if (dim == 0) return LSE_ERROR(kInvalidArgument, "hipwin workgroup size is zero");
  }
  if (kernel.executable == 0 || kernel.executable > modules_.size()) {
    return LSE_ERROR(kInvalidArgument, "hipwin kernel handle is not loaded");
  }
  Module& mod = *modules_[static_cast<std::size_t>(kernel.executable - 1)];
  if (kernel.export_ordinal >= mod.functions.size() ||
      mod.functions[kernel.export_ordinal] == nullptr) {
    return LSE_ERROR(kInvalidArgument, "hipwin export ordinal is not in this module");
  }

  std::vector<void*> slots;
  slots.reserve(args.bindings.size());
  for (const BufferRef& ref : args.bindings) {
    if (ref.buffer == nullptr || !ref.buffer->valid()) {
      return LSE_ERROR(kInvalidArgument, "hipwin launch binding is empty");
    }
    if (ref.offset > ref.buffer->size_bytes) {
      return LSE_ERROR(kOutOfRange, "hipwin launch binding starts past the buffer");
    }
    auto* dev = static_cast<std::byte*>(device_address(*ref.buffer));
    if (dev == nullptr) return LSE_ERROR(kInvalidArgument, "hipwin buffer has no device address");
    slots.push_back(dev + ref.offset);
  }
  // By-value LseConstants. An empty span is still the emitter's
  // `unsigned int _unused`. The words are 8-aligned so a later 8-byte field
  // is not read from an unaligned address. Padding between a 4-byte field and
  // an 8-byte field is the layout's own problem: ConstantsLayout packs with
  // no padding, and this copy does not insert any.
  alignas(8) unsigned char unused_constants[8] = {};
  std::vector<std::uint64_t> constant_words;
  void* constants_arg = unused_constants;
  if (!args.constants.empty()) {
    const std::size_t words =
        (args.constants.size() + sizeof(std::uint64_t) - 1) / sizeof(std::uint64_t);
    constant_words.assign(words, 0);
    std::memcpy(constant_words.data(), args.constants.data(), args.constants.size());
    constants_arg = constant_words.data();
  }
  std::vector<void*> params;
  params.reserve(slots.size() + 1);
  for (std::size_t i = 0; i < slots.size(); ++i) {
    params.push_back(static_cast<void*>(&slots[i]));
  }
  params.push_back(constants_arg);

  LSE_RETURN_IF_ERROR(bind_device());
  const hipError_t launched = hipModuleLaunchKernel(
      reinterpret_cast<hipFunction_t>(mod.functions[kernel.export_ordinal]),
      dims.workgroup_count[0], dims.workgroup_count[1], dims.workgroup_count[2],
      dims.workgroup_size[0], dims.workgroup_size[1], dims.workgroup_size[2], 0, nullptr,
      params.empty() ? nullptr : params.data(), nullptr);
  if (launched != hipSuccess) return hip_fail("hipModuleLaunchKernel", launched);
  // The argument slots and the constants copy die with this call. The launch
  // has to finish reading them first.
  const hipError_t synced = hipDeviceSynchronize();
  if (synced != hipSuccess) return hip_fail("hipDeviceSynchronize", synced);
  return OkStatus();
}

Status HipWinBackend::synchronize_impl() {
  if (!up_) return LSE_ERROR(kInternal, "hipwin is not initialized");
  LSE_RETURN_IF_ERROR(bind_device());
  const hipError_t err = hipDeviceSynchronize();
  if (err != hipSuccess) return hip_fail("hipDeviceSynchronize", err);
  return OkStatus();
}

Result<void*> HipWinBackend::device_pointer_impl(const DeviceBuffer& buf) const {
  void* dev = device_address(buf);
  if (dev == nullptr) return LSE_ERROR(kInvalidArgument, "hipwin buffer has no device address");
  return dev;
}

}  // namespace lse::backend

LSE_REGISTER_BACKEND("hipwin", ::lse::backend::HipWinBackend)
