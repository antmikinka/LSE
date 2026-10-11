// ID-12 anti-constant-folding canary — FP-canary layer C (11-cycle2-test-plan
// §7.4). Layers A/B grep the flag pin and the generated build rules; this test
// proves -ffp-contract=off is EFFECTIVE in a compiled, running binary: a*b+c
// must round twice (to 0.0f below), never once through an FMA.
//
// The operands are runtime-opaque (a volatile seeded from getenv), so the
// compiler cannot constant-fold the expression away and quietly satisfy the
// assertion with compile-time arithmetic in the folded model.
//
// The mul+add expressions live in helpers carrying __attribute__((target(
// "avx,fma"))). Reason (found by the mutation run below, kept honest): the
// project builds for the BASELINE x86-64 ISA (no /arch: or -march anywhere in
// cmake/), where the backend has no FMA instruction to contract INTO — so a
// canary compiled like ordinary engine code cannot distinguish pinned from
// unpinned flags. Giving the helpers FMA-capable codegen makes contraction
// possible, and then the ONLY thing preventing the fusion is the build-system
// pin: remove the pin and `plain` becomes 2^-24 / 2^-68 (red), keep it and
// `plain` stays +0.0 (green). The attributes are function-scoped, so the rest
// of the binary stays baseline-safe; if the pin is ever lost AND the engine
// gains an arch bump, this canary is what catches the semantics change.
//
// Acceptance (mutation test, run once, transcript in
// windows-enablement/logs/): removing `/clang:-ffp-contract=off` from
// cmake/LSECompilerFlags.cmake and rebuilding MUST turn this test red —
// the contracted `plain` becomes 2^-24 (float) / 2^-68 (double) instead of 0.
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "harness.hpp"

namespace {

#if (defined(__clang__) || defined(__GNUC__)) && \
    (defined(__x86_64__) || defined(__i386__))
// clang and gcc both honor the x86 target attribute. clang-cl accepts it too,
// but does NOT define __GNUC__ (macro-dump verified: __clang__, __x86_64__,
// _M_X64 only) — hence the __clang__ arm. Disassembly probe (clang-cl 21.1.8,
// /O2, no /arch): attributed a*b+c → vfmadd213ss by default, → vmulss+vaddss
// with /clang:-ffp-contract=off. The attribute is what makes this canary
// sensitive to the pin at all.
#define LSE_CANARY_CODEGEN __attribute__((target("avx,fma")))
#else
// Non-x86 or non-GNU-ABI compiler: the canary still checks std::fma vs the
// separate-op expression, but contraction sensitivity depends on the target.
#define LSE_CANARY_CODEGEN
#endif

LSE_CANARY_CODEGEN float mul_add_f32(float a, float b, float c) {
  return a * b + c;  // MUST NOT become an fma while the pin is in place
}
LSE_CANARY_CODEGEN float fused_f32(float a, float b, float c) {
  return std::fma(a, b, c);
}
LSE_CANARY_CODEGEN double mul_add_f64(double a, double b, double c) {
  return a * b + c;
}
LSE_CANARY_CODEGEN double fused_f64(double a, double b, double c) {
  return std::fma(a, b, c);
}

// Volatile reads the optimizer cannot see through; the value is 0.0 in every
// run (the env var never exists in CI), but its existence forces the sums
// below to execute at runtime on the chosen constants.
float opaque_bump_f() {
  const char* e = std::getenv("LSE_FP_CANARY_NONEXISTENT_ENV");
  volatile float bump = (e != nullptr) ? 1e-30f : 0.0f;
  return bump;
}
double opaque_bump_d() {
  const char* e = std::getenv("LSE_FP_CANARY_NONEXISTENT_ENV");
  volatile double bump = (e != nullptr) ? 1e-300 : 0.0;
  return bump;
}

}  // namespace

LSE_TEST(float_mul_add_stays_uncontracted) {
  // a = b = 1 + 2^-12; the exact product is 1 + 2^-11 + 2^-24, whose 2^-24
  // tail is a half-ulp below float precision and rounds to even away:
  // fl(a*b) = 1 + 2^-11. With c = -(1 + 2^-11):
  //   uncontracted fl(fl(a*b) + c) = +0.0f
  //   fused        fma(a, b, c)    = 2^-24   (exact product kept)
  const float bump = opaque_bump_f();  // 0.0f; rounds away below the ulp
  const float a = 1.0f + 0x1p-12f + bump;
  const float b = a;
  const float c = -(1.0f + 0x1p-11f) - bump;

  const float plain = mul_add_f32(a, b, c);
  const float fused = fused_f32(a, b, c);

  std::printf("  fp-canary f32: plain=%a fused=%a (expected 0x0p+0 / 0x1p-24)\n",
              (double)plain, (double)fused);
  LSE_EXPECT(fused == 0x1p-24f);  // the discriminator value is real
  LSE_EXPECT(plain == 0.0f);      // fails iff a*b+c was contracted
  LSE_EXPECT(plain != fused);
}

LSE_TEST(double_mul_add_stays_uncontracted) {
  // Same construction in fp64: a = b = 1 + 2^-34, exact product
  // 1 + 2^-33 + 2^-68 rounds to 1 + 2^-33; c = -(1 + 2^-33).
  //   uncontracted = +0.0; fma = 2^-68.
  const double bump = opaque_bump_d();
  const double a = 1.0 + 0x1p-34 + bump;
  const double b = a;
  const double c = -(1.0 + 0x1p-33) - bump;

  const double plain = mul_add_f64(a, b, c);
  const double fused = fused_f64(a, b, c);

  std::printf("  fp-canary f64: plain=%a fused=%a (expected 0x0p+0 / 0x1p-68)\n",
              plain, fused);
  LSE_EXPECT(fused == 0x1p-68);
  LSE_EXPECT(plain == 0.0);
  LSE_EXPECT(plain != fused);
}

LSE_TEST_MAIN()
