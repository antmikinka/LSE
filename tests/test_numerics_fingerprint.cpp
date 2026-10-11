// ID-5 numerics fingerprint — the T3 vehicle (11-cycle2-test-plan §7.3).
//
// A fixed-seed committed fixture (splitmix64, seeds below — identical bytes on
// every platform) drives GEMV / RMSNorm / rope-apply / softmax / attention-step
// through the engine's CPU reference path (the CPU backend executes graphs via
// the interpreter, src/backends/cpu/cpu_backend.cpp:170), then:
//
//   * prints the authoritative HASH-EQ / TOLERANCE partition (evidence: every
//     log and CI artifact carries it),
//   * prints SHA256 per output tensor and max_rel vs embedded goldens,
//   * HASH-EQ members must hash to the embedded cross-platform value: these
//     kernels use only IEEE-exact ops (+ - * /, correctly-rounded sqrt) with
//     fixed accumulation order and fp64 reduction accumulators, so Windows
//     clang-cl and Linux clang++ (same LLVM major, both -ffp-contract=off)
//     MUST produce identical bytes. Transcendental-freedom verified in
//     src/graph/interpreter.cpp: eval_matmul :443 (fp64 accum, + * cast),
//     eval_rms_norm :241 (fp64 accum, 1.0f/std::sqrt — the 05 ID-5 decision
//     rule's exact form), eval_rope :909 (+ - * on fixed cos/sin tables).
//   * TOLERANCE members (softmax, attention_step — std::exp inside) are
//     compared per-platform against embedded goldens with a max_rel bound;
//     cross-OS hash equality is never asserted for them (L8 physics line).
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "harness.hpp"

#include "lse/core/sha256.hpp"
#include "lse/graph/graph.hpp"
#include "lse/graph/interpreter.hpp"
#include "lse/graph/ops.hpp"

using namespace lse;
using namespace lse::graph;

namespace {

// --- the authoritative partition (source of truth, §7.3) ---------------------
enum class Set { kHashEq, kTolerance };
struct KernelEntry {
  const char* name;
  Set set;
};
constexpr KernelEntry kPartition[] = {
    {"gemv", Set::kHashEq},
    {"rms_norm", Set::kHashEq},
    {"rope_apply", Set::kHashEq},
    {"softmax", Set::kTolerance},
    {"attention_step", Set::kTolerance},
};
const char* set_name(Set s) {
  return s == Set::kHashEq ? "HASH_EQ" : "TOLERANCE";
}

// --- fixed-seed fixture -------------------------------------------------------
std::uint64_t splitmix64(std::uint64_t& s) {
  s += 0x9E3779B97F4A7C15ull;
  std::uint64_t z = s;
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}
float det_float(std::uint64_t& s) {
  const std::uint32_t bits =
      static_cast<std::uint32_t>(splitmix64(s) >> 40);  // 24-bit mantissa fill
  return (static_cast<float>(bits) * (1.0f / 16777216.0f)) * 2.0f - 1.0f;
}
std::vector<float> fixture(std::size_t n, std::uint64_t seed) {
  std::vector<float> v(n);
  std::uint64_t s = seed;
  for (auto& x : v) x = det_float(s);
  return v;
}

// --- CPU-reference execution (test_gdn_rate's upload pattern) ------------------
Scheduler& scheduler() {
  auto* s = default_scheduler();
  if (!s) std::abort();
  s->set_mode(Scheduler::Mode::kHostOnly);
  return *s;
}
Array upload(Shape shape, const std::vector<float>& vals) {
  auto& s = scheduler();
  Array a = Array::zeros(shape, DType::kF32);
  LSE_EXPECT_OK(interpreter::ensure_output_buffer(*a.node(), s.backend()));
  for (std::size_t i = 0; i < a.shape().elem_count(); ++i) {
    interpreter::store_element(*a.node(), i, vals[i]);
  }
  a.node()->materialized = true;
  a.node()->host_dirty = true;
  a.node()->device_dirty = false;
  LSE_EXPECT_OK(interpreter::sync_to_device(*a.node(), s.backend()));
  Array leaf = Array::from_buffer(a.node()->buffer, shape, DType::kF32);
  leaf.node()->member = a.node()->member;
  leaf.node()->host_mirror = a.node()->host_mirror;
  leaf.node()->host_dirty = a.node()->host_dirty;
  leaf.node()->device_dirty = a.node()->device_dirty;
  return leaf;
}
std::vector<float> materialize(Array a) {
  std::vector<float> out(a.shape().elem_count(), 0.0f);
  const auto st = a.to_host(out.data(), out.size() * sizeof(float));
  LSE_EXPECT_OK(st);
  return out;
}

// NOTE: the span count is in ELEMENTS (float*), not bytes — as_bytes() then
// yields exactly size()*4 bytes. An earlier form passed size()*sizeof(float)
// as the count, hashing 4× past the vector end (per-process heap garbage →
// nondeterministic SHAs). Keep it element-counted.
std::string sha_of(const std::vector<float>& v) {
  return lse::sha256(std::as_bytes(std::span(v.data(), v.size())));
}
double max_rel(const std::vector<float>& got, const std::vector<float>& gold) {
  double worst = 0.0;
  for (std::size_t i = 0; i < got.size() && i < gold.size(); ++i) {
    const double denom = std::fabs(static_cast<double>(gold[i]));
    const double rel =
        std::fabs(static_cast<double>(got[i]) - static_cast<double>(gold[i])) /
        (denom > 1e-30 ? denom : 1.0);
    if (rel > worst) worst = rel;
  }
  return worst;
}
std::vector<float> from_bits(const std::uint32_t* bits, std::size_t n) {
  std::vector<float> v(n);
  for (std::size_t i = 0; i < n; ++i) v[i] = std::bit_cast<float>(bits[i]);
  return v;
}

// --- embedded cross-platform constants ---------------------------------------
// HASH-EQ expected SHA256 (must be byte-identical on Windows-clang-cl and
// Linux-clang++-21, both -ffp-contract=off — the T3 controlled pair):
constexpr const char* kExpectedShaGemv =
    "b2bba4cfd170a6ea8de1529b9d8611af119c04961bb4348fab8f3d38368d80e9";
constexpr const char* kExpectedShaRms =
    "43270ae2dcd6469336b60fea798aa24132f6837ca273be1a20d4f8eae2678c8d";
constexpr const char* kExpectedShaRope =
    "03404a9a8a8f8c66338df23865bbf1ab2878e5e0003ea9967c4f3c81a4abe2b8";

// TOLERANCE goldens (f32 bit patterns) + bound. Goldens were materialized on
// the Windows reference build; the bound absorbs per-platform libm exp drift
// (typically ~1 ulp) with orders of magnitude of headroom.
constexpr double kToleranceMaxRel = 1e-5;
constexpr std::uint32_t kGoldenSoftmax[16] = {
    0x3D8B2417u, 0x3E41B8A4u, 0x3D8A7B0Du, 0x3E578D5Fu,
    0x3E22F962u, 0x3D5801DEu, 0x3DAB9823u, 0x3E2D2482u,
    0x3E39A74Du, 0x3E042275u, 0x3D598869u, 0x3D601717u,
    0x3E52794Au, 0x3E087268u, 0x3DE817A3u, 0x3E04D6DAu,
};
constexpr std::uint32_t kGoldenSdpa[32] = {
    0x3E9A72B8u, 0xBF0DD43Au, 0xBF62523Fu, 0xBF138BEAu,
    0xBDB7121Fu, 0xBF50A951u, 0x3C42E02Fu, 0xBE51BAEBu,
    0x3E9A6F66u, 0xBF11BD01u, 0xBF62C7F0u, 0xBF154864u,
    0xBDD64A45u, 0xBF4FF206u, 0xBC1447AAu, 0xBE3D89C2u,
    0xBD9FF407u, 0xBD41641Du, 0xBE9535A6u, 0xBE6964A2u,
    0x3EF9DB19u, 0xBEC969F7u, 0xBE4E86A3u, 0xBE7FED56u,
    0x3E86794Cu, 0xBEAF4256u, 0xBED1A81Bu, 0xBE997D7Cu,
    0x3F016F3Fu, 0xBF145C7Cu, 0x3BCFF8D5u, 0xBF00A3A5u,
};

}  // namespace

LSE_TEST(partition_is_printed_for_every_artifact) {
  std::printf("       fingerprint partition:");
  for (const auto& e : kPartition)
    std::printf(" %s=%s", e.name, set_name(e.set));
  std::printf("\n");
  // The membership table itself is the source of truth (§7.3): 3 HASH-EQ,
  // 2 TOLERANCE, names exactly as the gate scripts scrape them.
  int hash_eq = 0, tolerance = 0;
  for (const auto& e : kPartition)
    (e.set == Set::kHashEq ? hash_eq : tolerance)++;
  LSE_EXPECT_EQ(hash_eq, 3);
  LSE_EXPECT_EQ(tolerance, 2);
}

LSE_TEST(hash_eq_kernels_are_bit_reproducible) {
  // GEMV: [1,64] @ [64,32] — fp64 accumulation, + * cast only.
  const auto av = fixture(64, 0x6E3C0001);
  const auto bv = fixture(64 * 32, 0x6E3C0002);
  const auto gemv = materialize(matmul(upload({1, 64}, av), upload({64, 32}, bv)));
  const std::string sha_gemv = sha_of(gemv);

  // RMSNorm: x [4,64], w [64], eps 1e-5 — fp64 accum + 1.0f/std::sqrt.
  const auto xv = fixture(4 * 64, 0x6E3C0003);
  const auto wv = fixture(64, 0x6E3C0004);
  const auto rms = materialize(
      rms_norm(upload({4, 64}, xv), upload({64}, wv), 1e-5f));
  const std::string sha_rms = sha_of(rms);

  // rope apply: x [B=1,H=2,T=4,D=8] with FIXED cos/sin tables [max_T=8,R=8]
  // (fixture data, not libm-built — the apply itself is + - * only).
  const auto rx = fixture(1 * 2 * 4 * 8, 0x6E3C0005);
  const auto rc = fixture(8 * 8, 0x6E3C0006);
  const auto rs = fixture(8 * 8, 0x6E3C0007);
  const auto roped = materialize(rope(upload({1, 2, 4, 8}, rx),
                                      upload({8, 8}, rc), upload({8, 8}, rs),
                                      /*offset=*/0));
  const std::string sha_rope = sha_of(roped);

  std::printf("       fingerprint kernel=gemv set=HASH_EQ sha256=%s\n",
              sha_gemv.c_str());
  std::printf("       fingerprint kernel=rms_norm set=HASH_EQ sha256=%s\n",
              sha_rms.c_str());
  std::printf("       fingerprint kernel=rope_apply set=HASH_EQ sha256=%s\n",
              sha_rope.c_str());

  LSE_EXPECT(sha_gemv == kExpectedShaGemv);
  LSE_EXPECT(sha_rms == kExpectedShaRms);
  LSE_EXPECT(sha_rope == kExpectedShaRope);
}

LSE_TEST(tolerance_kernels_stay_within_max_rel_of_goldens) {
  // softmax over [2,8] — std::exp inside (eval_softmax :103).
  const auto sv = fixture(2 * 8, 0x6E3C0008);
  const auto sm = materialize(softmax(upload({2, 8}, sv), -1));
  const std::string sha_sm = sha_of(sm);

  // attention step: sdpa q [1,2,2,8] vs k/v [1,2,4,8], scale 0.35, unmasked —
  // exp inside the scores.
  const auto qv = fixture(1 * 2 * 2 * 8, 0x6E3C0009);
  const auto kv = fixture(1 * 2 * 4 * 8, 0x6E3C000A);
  const auto vv = fixture(1 * 2 * 4 * 8, 0x6E3C000B);
  const auto att = materialize(sdpa(upload({1, 2, 2, 8}, qv),
                                    upload({1, 2, 4, 8}, kv),
                                    upload({1, 2, 4, 8}, vv), 0.35f,
                                    MaskKind::kNone));
  const std::string sha_att = sha_of(att);

  const auto gold_sm = from_bits(kGoldenSoftmax, 16);
  const auto gold_att = from_bits(kGoldenSdpa, 32);
  const double rel_sm = max_rel(sm, gold_sm);
  const double rel_att = max_rel(att, gold_att);

  // Evidence lines: the TOLERANCE max_rel table every artifact must carry.
  std::printf("       fingerprint kernel=softmax set=TOLERANCE sha256=%s "
              "max_rel=%.3e\n", sha_sm.c_str(), rel_sm);
  std::printf("       fingerprint kernel=attention_step set=TOLERANCE sha256=%s "
              "max_rel=%.3e\n", sha_att.c_str(), rel_att);

  LSE_EXPECT_EQ(sm.size(), 16u);
  LSE_EXPECT_EQ(att.size(), 32u);
  if (kGoldenSoftmax[0] == 0 && kGoldenSdpa[0] == 0) {
    // Goldens not embedded yet: print paste-ready bit patterns, then fail
    // loudly — a placeholder golden must never pass silently.
    std::printf("       EMBED softmax golden:");
    for (const float f : sm)
      std::printf(" 0x%08Xu,", std::bit_cast<std::uint32_t>(f));
    std::printf("\n       EMBED sdpa golden:");
    for (const float f : att)
      std::printf(" 0x%08Xu,", std::bit_cast<std::uint32_t>(f));
    std::printf("\n");
    LSE_EXPECT(false && "TOLERANCE goldens are still placeholders");
    return;
  }
  LSE_EXPECT(rel_sm <= kToleranceMaxRel);
  LSE_EXPECT(rel_att <= kToleranceMaxRel);
}

LSE_TEST_MAIN()
