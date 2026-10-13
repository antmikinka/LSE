// Loads hsa-runtime64 and prints the GPU agent, then copies 16 bytes through
// the HSA subset. Not an engine run.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

struct Agent {
  std::uint64_t handle;
};
struct Signal {
  std::uint64_t handle;
};

extern "C" {
__declspec(dllimport) int hsa_init(void);
__declspec(dllimport) int hsa_iterate_agents(int (*cb)(Agent, void*), void*);
__declspec(dllimport) int hsa_agent_get_info(Agent, int, void*);
__declspec(dllimport) int hsa_amd_memory_async_copy(void*, Agent, const void*, Agent, std::size_t,
                                                   std::uint32_t, const Signal*, Signal);
__declspec(dllimport) int hsa_signal_create(std::int64_t, std::uint32_t, const Agent*, Signal*);
__declspec(dllimport) int hsa_signal_destroy(Signal);
__declspec(dllimport) std::int64_t hsa_signal_wait_scacquire(Signal, int, std::int64_t,
                                                            std::uint64_t, int);
}

namespace {
constexpr int kInfoName = 0;
constexpr int kInfoDevice = 17;
constexpr int kInfoCu = 0xA002;
constexpr int kInfoMem = 0xA015;
constexpr int kDeviceGpu = 1;

int g_gpu = 0;

int visit(Agent agent, void*) {
  int type = -1;
  if (hsa_agent_get_info(agent, kInfoDevice, &type) != 0) {
    std::printf("agent %llu type unread\n", static_cast<unsigned long long>(agent.handle));
    return 0;
  }
  char name[64] = {};
  hsa_agent_get_info(agent, kInfoName, name);
  std::printf("agent %llu type=%d name=%s\n", static_cast<unsigned long long>(agent.handle), type,
              name);
  if (type == kDeviceGpu) {
    g_gpu = 1;
    std::uint32_t cus = 0;
    std::uint64_t free_b = 0;
    const int cu_st = hsa_agent_get_info(agent, kInfoCu, &cus);
    const int mem_st = hsa_agent_get_info(agent, kInfoMem, &free_b);
    std::printf("  cu_status=%d compute_units=%u mem_status=%d free=%llu\n", cu_st, cus, mem_st,
                static_cast<unsigned long long>(free_b));
    if (cu_st != 0 || cus == 0 || mem_st != 0 || free_b == 0 || name[0] == '\0') return 1;
  }
  return 0;
}

int copy_check(Agent gpu) {
  unsigned char src[16];
  unsigned char dst[16];
  for (int i = 0; i < 16; ++i) src[i] = static_cast<unsigned char>(i + 1);
  std::memset(dst, 0, sizeof(dst));
  Signal completion{};
  if (hsa_signal_create(1, 0, nullptr, &completion) != 0) {
    std::printf("hsa_signal_create failed\n");
    return 1;
  }
  const int copied = hsa_amd_memory_async_copy(dst, gpu, src, gpu, sizeof(src), 0, nullptr,
                                               completion);
  // The copy stores 0 before it returns. Condition 2 is "less than 1".
  // A timeout other than forever returns the current value without spinning.
  const std::int64_t waited = hsa_signal_wait_scacquire(completion, 2, 1, 1, 1);
  hsa_signal_destroy(completion);
  const int same = std::memcmp(src, dst, sizeof(src)) == 0;
  std::printf("copy_status=%d wait=%lld bytes_match=%d\n", copied, static_cast<long long>(waited),
              same);
  return copied == 0 && waited == 0 && same ? 0 : 1;
}
}  // namespace

int main() {
  const int st = hsa_init();
  std::printf("hsa_init=%d\n", st);
  if (st != 0) return 1;
  const int walk = hsa_iterate_agents(&visit, nullptr);
  std::printf("hsa_iterate_agents=%d gpu=%d\n", walk, g_gpu);
  if (walk != 0 || g_gpu == 0) return 1;
  const Agent gpu{2};
  return copy_check(gpu);
}
