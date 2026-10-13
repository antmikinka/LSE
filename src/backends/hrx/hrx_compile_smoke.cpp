// Compiles one HIP kernel with the HRX comgr toolchain, loads it through
// hrx_executable_load_data, and dispatches it through HrxBackend. The source
// is fresh: nothing is read from the kernel cache.
#include "lse/backends/hrx/hrx_backend.hpp"

#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <vector>

namespace {

constexpr unsigned kCount = 1u << 20;

constexpr char kSource[] = R"hip(
#include <hip/hip_runtime.h>

struct LseConstants {
  unsigned int count;
};

extern "C" __global__ void lse_vadd(
    const float* __restrict__ a,
    const float* __restrict__ b,
    float* __restrict__ c,
    LseConstants k) {
  const unsigned int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i >= k.count) return;
  c[i] = a[i] + b[i];
}
)hip";

void fail(const char* what, const lse::Status& status) {
  std::fprintf(stderr, "%s: %s\n", what, status.to_string().c_str());
}

}  // namespace

int main() {
  lse::backend::HrxBackend backend;
  if (const lse::Status opened = backend.init(0); !opened.ok()) {
    fail("init", opened);
    return 1;
  }

  const std::span<const lse::graph::KernelToolchain> toolchains = backend.toolchains();
  std::printf("toolchains=%zu arch=%s\n", toolchains.size(),
              backend.device_info().arch.c_str());
  const lse::graph::KernelToolchain* hip = nullptr;
  for (const lse::graph::KernelToolchain& toolchain : toolchains) {
    if (toolchain.dialect == lse::graph::Dialect::kHip && toolchain.compiler != nullptr &&
        toolchain.compiler->available()) {
      hip = &toolchain;
      break;
    }
  }
  if (hip == nullptr) {
    std::fprintf(stderr, "hrx did not declare an available HIP compiler\n");
    return 1;
  }
  std::printf("identity=%s\n", hip->compiler->identity().c_str());

  auto compiled = hip->compiler->compile(kSource, backend.device_info().arch);
  if (!compiled.ok()) {
    fail("compile", compiled.status());
    return 1;
  }
  std::printf("code_bytes=%zu census=%zu\n", compiled->code.size(), compiled->census.size());
  if (compiled->code.size() < 64) {
    std::fprintf(stderr, "code object is too small to be a kernel\n");
    return 1;
  }

  auto handle = backend.load_executable(
      "lse_vadd", std::span<const std::byte>(compiled->code.data(), compiled->code.size()));
  if (!handle.ok()) {
    fail("load", handle.status());
    return 1;
  }

  std::vector<float> a(kCount);
  std::vector<float> b(kCount);
  for (unsigned i = 0; i < kCount; ++i) {
    a[i] = static_cast<float>(i);
    b[i] = 1.0f;
  }
  auto da = backend.upload(a.data(), a.size() * sizeof(float));
  auto db = backend.upload(b.data(), b.size() * sizeof(float));
  auto dc = backend.allocate(kCount * sizeof(float));
  if (!da.ok() || !db.ok() || !dc.ok()) {
    if (!da.ok()) fail("upload a", da.status());
    if (!db.ok()) fail("upload b", db.status());
    if (!dc.ok()) fail("allocate c", dc.status());
    return 1;
  }

  const lse::backend::BufferRef bindings[3] = {
      {&*da, 0, da->size_bytes},
      {&*db, 0, db->size_bytes},
      {&*dc, 0, dc->size_bytes},
  };
  const unsigned count = kCount;
  lse::backend::DispatchArgs args;
  args.bindings = bindings;
  args.constants = std::span<const std::byte>(reinterpret_cast<const std::byte*>(&count),
                                             sizeof(count));

  lse::backend::LaunchDims dims;
  dims.workgroup_size[0] = 256;
  dims.workgroup_count[0] = (kCount + 255u) / 256u;

  if (const lse::Status launched = backend.launch(*handle, dims, args); !launched.ok()) {
    fail("launch", launched);
    return 1;
  }

  auto out = backend.download<float>(*dc, kCount);
  if (!out.ok()) {
    fail("download", out.status());
    return 1;
  }
  unsigned mismatches = 0;
  unsigned first = 0;
  for (unsigned i = 0; i < kCount; ++i) {
    if ((*out)[i] != a[i] + b[i]) {
      if (mismatches == 0) first = i;
      ++mismatches;
    }
  }
  std::printf("mismatches=%u\n", mismatches);
  if (mismatches != 0) {
    std::printf("first_mismatch=%u got=%g want=%g\n", first, (*out)[first],
                static_cast<double>(a[first] + b[first]));
    return 1;
  }
  std::printf("smoke_exit=0\n");
  return 0;
}
