// Proves libhrx against the Windows HSA runtime already on this machine.
// A matching copy is the success. Pass a compiled lse_vadd code object as
// argv[1] to launch it. A 16-byte image is not a kernel.

#include "hrx_runtime.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

int g_fails = 0;

void expect(bool cond, const char* what) {
  if (cond) {
    std::printf("ok  %s\n", what);
    return;
  }
  std::printf("FAIL %s\n", what);
  ++g_fails;
}

void expect_code(hrx_status_t status, hrx_status_code_t code, const char* what) {
  const hrx_status_code_t got = hrx_status_code(status);
  if (got != code) {
    char* message = nullptr;
    size_t length = 0;
    hrx_status_t formatted = hrx_status_to_string(status, &message, &length);
    std::printf("FAIL %s (code %d, wanted %d) %s\n", what,
                static_cast<int>(got), static_cast<int>(code),
                message != nullptr ? message : "");
    hrx_status_free_message(message);
    hrx_status_ignore(formatted);
    ++g_fails;
  } else {
    std::printf("ok  %s\n", what);
  }
  hrx_status_ignore(status);
}

std::string property_string(hrx_device_t device, hrx_device_property_t prop) {
  char buffer[256] = {};
  hrx_status_t status = hrx_device_get_property(device, prop, buffer, sizeof(buffer));
  if (!hrx_status_is_ok(status)) {
    hrx_status_ignore(status);
    return {};
  }
  buffer[sizeof(buffer) - 1] = '\0';
  return std::string(buffer);
}

}  // namespace

int main(int argc, char** argv) {
  int major = -1;
  int minor = -1;
  int patch = -1;
  hrx_runtime_version(&major, &minor, &patch);
  std::printf("hrx %d.%d.%d\n", major, minor, patch);

  expect_code(hrx_gpu_initialize(0), HRX_STATUS_OK, "gpu initialize");
  expect_code(hrx_gpu_initialize(0), HRX_STATUS_ALREADY_EXISTS,
              "second initialize is already exists");

  int count = -1;
  expect_code(hrx_gpu_device_count(&count), HRX_STATUS_OK, "device count call");
  expect(count == 1, "one gpu");

  hrx_device_t device = nullptr;
  expect_code(hrx_gpu_device_get(0, &device), HRX_STATUS_OK, "device get");
  expect(device != nullptr, "device handle");

  const std::string name = property_string(device, HRX_DEVICE_PROPERTY_NAME);
  const std::string arch =
      property_string(device, HRX_DEVICE_PROPERTY_ARCHITECTURE);
  std::printf("gpu name: %s\n", name.c_str());
  std::printf("gpu arch: %s\n", arch.c_str());
  expect(arch.find("gfx1150") != std::string::npos, "architecture is gfx1150");
  expect(name.find("(Node ") != std::string::npos, "name carries an hsa node");
  expect(name.find(arch) != std::string::npos, "name contains the architecture");

  std::uint32_t cus = 0;
  std::uint32_t threads = 0;
  std::uint32_t wave = 0;
  expect_code(hrx_device_get_property(device, HRX_DEVICE_PROPERTY_COMPUTE_UNITS,
                                      &cus, sizeof(cus)),
              HRX_STATUS_OK, "compute units");
  expect_code(hrx_device_get_property(
                  device, HRX_DEVICE_PROPERTY_MAX_WORKGROUP_SIZE, &threads,
                  sizeof(threads)),
              HRX_STATUS_OK, "max workgroup");
  expect_code(hrx_device_get_property(device, HRX_DEVICE_PROPERTY_WARP_SIZE,
                                      &wave, sizeof(wave)),
              HRX_STATUS_OK, "warp size");
  std::printf("compute_units: %u\n", cus);
  std::printf("max_workgroup: %u\n", threads);
  std::printf("warp: %u\n", wave);
  expect(cus == 16, "16 compute units");
  expect(threads != 0, "workgroup is published");
  expect(wave != 0, "warp is published");

  std::uint32_t queues = 0;
  expect_code(hrx_device_get_property(device, HRX_DEVICE_PROPERTY_QUEUE_COUNT,
                                      &queues, sizeof(queues)),
              HRX_STATUS_UNAVAILABLE, "queue count stays unpublished");
  std::uint64_t total = 0;
  expect_code(hrx_device_get_property(device, HRX_DEVICE_PROPERTY_TOTAL_MEMORY,
                                      &total, sizeof(total)),
              HRX_STATUS_UNAVAILABLE, "total memory stays unpublished");
  std::uint32_t shared = 0;
  expect_code(hrx_device_get_property(device, HRX_DEVICE_PROPERTY_MAX_SHARED_MEMORY,
                                      &shared, sizeof(shared)),
              HRX_STATUS_UNAVAILABLE, "shared memory stays unpublished");
  std::uint32_t clock = 0;
  expect_code(hrx_device_get_property(device, HRX_DEVICE_PROPERTY_CLOCK_RATE,
                                      &clock, sizeof(clock)),
              HRX_STATUS_UNAVAILABLE, "clock stays unpublished");

  std::size_t free_bytes = 0;
  std::size_t total_bytes = 1;
  expect_code(hrx_device_memory_info(device, &free_bytes, &total_bytes),
              HRX_STATUS_OK, "memory info");
  std::printf("free_bytes: %zu\n", free_bytes);
  std::printf("memory_info total out-param: %zu (HSA does not publish a total)\n",
              total_bytes);
  expect(free_bytes > 0, "free memory is non-zero");
  expect(total_bytes == 0, "total out-param is unpublished");

  bool self = false;
  expect_code(hrx_device_can_access_peer(device, device, &self), HRX_STATUS_OK,
              "peer self");
  expect(self, "device can access itself");

  hrx_stream_t stream = nullptr;
  expect_code(hrx_stream_create_on_queue(device, 0, 1, &stream), HRX_STATUS_OK,
              "stream on queue bit 0");
  hrx_queue_affinity_t affinity = 99;
  expect_code(hrx_stream_get_queue_affinity(stream, &affinity), HRX_STATUS_OK,
              "read affinity");
  expect(affinity == 1, "affinity bit 0");
  hrx_stream_t rejected = nullptr;
  expect_code(hrx_stream_create_on_queue(device, 0, 2, &rejected),
              HRX_STATUS_OUT_OF_RANGE, "queue bit 1 is out of range");
  expect(rejected == nullptr, "rejected stream was not created");

  hrx_semaphore_t semaphore = nullptr;
  expect_code(hrx_semaphore_create(device, 0, &semaphore), HRX_STATUS_OK,
              "semaphore");
  hrx_semaphore_list_t signals = {};
  std::uint64_t signal_value = 1;
  signals.semaphores = &semaphore;
  signals.values = &signal_value;
  signals.count = 1;
  expect_code(hrx_queue_barrier(device, 1, nullptr, &signals), HRX_STATUS_OK,
              "barrier on queue bit 0");
  expect_code(hrx_semaphore_wait(semaphore, 1, UINT64_MAX), HRX_STATUS_OK,
              "barrier signaled");
  expect_code(hrx_queue_barrier(device, 2, nullptr, &signals),
              HRX_STATUS_OUT_OF_RANGE, "barrier on queue bit 1 is refused");
  hrx_semaphore_release(semaphore);

  hrx_buffer_t src = nullptr;
  hrx_buffer_t dst = nullptr;
  expect_code(hrx_buffer_allocate(stream, 16, HRX_MEMORY_TYPE_DEVICE_LOCAL,
                                  HRX_BUFFER_USAGE_TRANSFER, &src),
              HRX_STATUS_OK, "allocate src");
  expect_code(hrx_buffer_allocate(stream, 16, HRX_MEMORY_TYPE_DEVICE_LOCAL,
                                  HRX_BUFFER_USAGE_TRANSFER, &dst),
              HRX_STATUS_OK, "allocate dst");

  unsigned char pattern[16];
  for (int i = 0; i < 16; ++i) pattern[i] = static_cast<unsigned char>(0xA0 + i);
  expect_code(hrx_stream_update_buffer(stream, pattern, 16, dst, 0),
              HRX_STATUS_OK, "queue update");
  expect_code(hrx_stream_copy_buffer(stream, dst, 0, src, 0, 16), HRX_STATUS_OK,
              "queue copy");
  expect_code(hrx_stream_synchronize(stream), HRX_STATUS_OK, "synchronize copy");

  void* mapped = nullptr;
  expect_code(hrx_buffer_map(src, HRX_MAP_READ, 0, 16, &mapped), HRX_STATUS_OK,
              "map src");
  expect(mapped != nullptr && std::memcmp(mapped, pattern, 16) == 0,
         "16 bytes match");
  expect_code(hrx_buffer_unmap(src), HRX_STATUS_OK, "unmap src");

  hrx_executable_t bad_image = nullptr;
  expect_code(hrx_executable_load_data(device, pattern, 16, "hip", "gfx1150", &bad_image),
              HRX_STATUS_INVALID_ARGUMENT, "a 16-byte image is not a code object");
  expect(bad_image == nullptr, "rejected load leaves no executable");
  expect_code(hrx_stream_dispatch(stream, nullptr, 0, nullptr, nullptr, 0, nullptr, 0, 0),
              HRX_STATUS_INVALID_ARGUMENT, "dispatch needs an executable");

  if (argc > 1) {
    std::vector<unsigned char> image;
    FILE* file = nullptr;
    if (fopen_s(&file, argv[1], "rb") != 0 || file == nullptr) {
      expect(false, "open code object");
    } else {
      if (_fseeki64(file, 0, SEEK_END) == 0) {
        const long long n = _ftelli64(file);
        if (n > 0 && _fseeki64(file, 0, SEEK_SET) == 0) {
          image.resize(static_cast<std::size_t>(n));
          const std::size_t got = std::fread(image.data(), 1, image.size(), file);
          if (got != image.size()) image.clear();
        }
      }
      std::fclose(file);
    }
    hrx_executable_t exe = nullptr;
    expect_code(hrx_executable_load_data(device, image.data(), image.size(), "amdgpu", "gfx1150",
                                         &exe),
                HRX_STATUS_OK, "load lse_vadd");
    if (exe != nullptr) {
      std::uint32_t ordinal = 0;
      expect_code(hrx_executable_lookup_export_by_name(exe, "lse_vadd", &ordinal), HRX_STATUS_OK,
                  "lookup lse_vadd");
      constexpr unsigned kCount = 1024;
      std::vector<float> host_a(kCount);
      std::vector<float> host_b(kCount);
      for (unsigned i = 0; i < kCount; ++i) {
        host_a[i] = static_cast<float>(i);
        host_b[i] = 1.0f;
      }
      hrx_buffer_t buf_a = nullptr;
      hrx_buffer_t buf_b = nullptr;
      hrx_buffer_t buf_c = nullptr;
      const std::size_t bytes = kCount * sizeof(float);
      expect_code(hrx_buffer_allocate(stream, bytes, HRX_MEMORY_TYPE_DEVICE_LOCAL,
                                      HRX_BUFFER_USAGE_TRANSFER, &buf_a),
                  HRX_STATUS_OK, "allocate a");
      expect_code(hrx_buffer_allocate(stream, bytes, HRX_MEMORY_TYPE_DEVICE_LOCAL,
                                      HRX_BUFFER_USAGE_TRANSFER, &buf_b),
                  HRX_STATUS_OK, "allocate b");
      expect_code(hrx_buffer_allocate(stream, bytes, HRX_MEMORY_TYPE_DEVICE_LOCAL,
                                      HRX_BUFFER_USAGE_TRANSFER, &buf_c),
                  HRX_STATUS_OK, "allocate c");
      expect_code(hrx_stream_update_buffer(stream, host_a.data(), bytes, buf_a, 0), HRX_STATUS_OK,
                  "upload a");
      expect_code(hrx_stream_update_buffer(stream, host_b.data(), bytes, buf_b, 0), HRX_STATUS_OK,
                  "upload b");
      hrx_dispatch_config_t config{};
      config.workgroup_count[0] = kCount / 256;
      config.workgroup_count[1] = 1;
      config.workgroup_count[2] = 1;
      config.workgroup_size[0] = 256;
      config.workgroup_size[1] = 1;
      config.workgroup_size[2] = 1;
      const unsigned count = kCount;
      hrx_buffer_ref_t refs[3] = {
          {buf_a, 0, bytes},
          {buf_b, 0, bytes},
          {buf_c, 0, bytes},
      };
      expect_code(hrx_stream_dispatch(stream, exe, ordinal, &config, &count, sizeof(count), refs, 3,
                                      0),
                  HRX_STATUS_OK, "dispatch lse_vadd");
      expect_code(hrx_stream_synchronize(stream), HRX_STATUS_OK, "synchronize dispatch");
      void* mapped = nullptr;
      expect_code(hrx_buffer_map(buf_c, HRX_MAP_READ, 0, bytes, &mapped), HRX_STATUS_OK, "map c");
      bool match = mapped != nullptr;
      if (match) {
        const auto* out = static_cast<const float*>(mapped);
        for (unsigned i = 0; i < kCount; ++i) {
          if (out[i] != host_a[i] + host_b[i]) {
            match = false;
            std::printf("mismatch at %u: got %f\n", i, static_cast<double>(out[i]));
            break;
          }
        }
      }
      expect(match, "vadd results match");
      expect_code(hrx_buffer_unmap(buf_c), HRX_STATUS_OK, "unmap c");
      hrx_buffer_release(buf_a);
      hrx_buffer_release(buf_b);
      hrx_buffer_release(buf_c);
      hrx_executable_release(exe);
    }
  }

  hrx_buffer_release(src);
  hrx_buffer_release(dst);
  hrx_stream_release(stream);
  expect_code(hrx_gpu_shutdown(), HRX_STATUS_OK, "shutdown");

  expect_code(hrx_gpu_initialize_over("*", 0), HRX_STATUS_OK,
              "initialize over every gpu");
  count = -1;
  expect_code(hrx_gpu_device_count(&count), HRX_STATUS_OK, "count after over");
  expect(count == 1, "over still sees one gpu");
  expect_code(hrx_gpu_shutdown(), HRX_STATUS_OK, "shutdown after over");

  expect_code(hrx_gpu_initialize_over("9", 0), HRX_STATUS_NOT_FOUND,
              "missing ordinal is not found");

  if (g_fails != 0) {
    std::printf("%d check(s) failed\n", g_fails);
    return 1;
  }
  std::printf("hrx-win-smoke passed\n");
  return 0;
}
