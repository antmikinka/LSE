// hipwin — the Windows GPU backend. It speaks the HIP runtime
// (hipMalloc's host-mapped sibling, hipModuleLoadData, hipModuleLaunchKernel).
// It does not load libhrx. When amd_comgr is linked, toolchains() is the HIP
// emitter and that compiler. Loom is not a Windows toolchain.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "lse/backend/backend.hpp"
#include "lse/backends/hrx/device_info.hpp"

#ifndef LSE_HIPWIN_COMGR
#define LSE_HIPWIN_COMGR 0
#endif

#if LSE_HIPWIN_COMGR
#include <array>

#include "lse/backends/hrx/hipc/comgr_compiler.hpp"
#include "lse/backends/hrx/hipc/hip_emitter.hpp"
#endif

namespace lse::backend {

class HipWinBackend : public Backend<HipWinBackend> {
 public:
  static constexpr std::string_view kName = "hipwin";

  HipWinBackend();
  ~HipWinBackend();

  Status init_impl(int device_ordinal);
  void shutdown_impl() noexcept;
  const DeviceInfo& device_info_impl() const noexcept { return info_; }

  static Result<std::vector<DeviceDescriptor>> enumerate_devices();

  Result<DeviceBuffer> allocate_impl(std::size_t bytes, MemoryClass cls, Stream stream);
  void deallocate_impl(DeviceBuffer& buf) noexcept;
  Result<std::size_t> sample_free_memory_impl() const;

  Status copy_h2d_impl(const void* src, DeviceBuffer& dst, std::size_t bytes,
                       std::size_t dst_offset);
  Status copy_d2h_impl(const DeviceBuffer& src, void* dst, std::size_t bytes,
                       std::size_t src_offset);
  Status copy_ordered_impl(const DeviceBuffer& src, DeviceBuffer& dst, std::size_t bytes,
                           std::size_t src_offset, std::size_t dst_offset);
  // Same-device copies go through copy_peer: copy_d2d is that call. The
  // bytes stay in the coherent mapping; overlapping ranges use memmove.
  Status copy_peer_impl(const DeviceBuffer& src, DeviceBuffer& dst, std::size_t bytes,
                        std::size_t src_offset, std::size_t dst_offset);
  Status write_ordered_impl(DeviceBuffer& dst, const void* src, std::size_t bytes,
                            std::size_t dst_offset);
  Status zero_ordered_impl(DeviceBuffer& dst, std::size_t bytes, std::size_t dst_offset);

  Result<KernelHandle> load_executable_impl(std::string_view name,
                                            std::span<const std::byte> code_object);
  Result<std::vector<KernelHandle>> load_executables_impl(
      std::span<const std::string> names, std::span<const std::byte> code_object);
  Status launch_impl(const KernelHandle& kernel, const LaunchDims& dims,
                     const DispatchArgs& args, const DispatchTarget& target);
  Status synchronize_impl();
  Result<void*> device_pointer_impl(const DeviceBuffer& buf) const;

#if LSE_HIPWIN_COMGR
  std::span<const graph::KernelToolchain> toolchains_impl() const noexcept {
    return toolchains_;
  }
#else
  std::span<const graph::KernelToolchain> toolchains_impl() const noexcept { return {}; }
#endif

 private:
  struct Module {
    void* module = nullptr;  // hipModule_t
    std::vector<void*> functions;
  };

  DeviceInfo info_{};
  // extension points here. Family ISA facts come from the arch table; board
  // rows stay empty for a part the table does not name.
  AmdDeviceInfo amd_{};
  int ordinal_ = 0;
  bool up_ = false;
  std::vector<std::unique_ptr<Module>> modules_;
#if LSE_HIPWIN_COMGR
  // Points into this object. The backend is constructed in place and not moved.
  HipEmitter emitter_;
  ComgrCompiler compiler_;
  std::array<graph::KernelToolchain, 1> toolchains_{
      graph::KernelToolchain{graph::Dialect::kHip, &emitter_, &compiler_}};
#endif

  void* device_address(const DeviceBuffer& buf) const noexcept;
  Status bind_device() const;
  Status transfer(void* dst, const void* src, std::size_t bytes);
};

}  // namespace lse::backend
