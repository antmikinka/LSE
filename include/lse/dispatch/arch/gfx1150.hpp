// gfx1150 (Radeon 890M, RDNA 3.5): 16 CUs, wave32, the gfx11 WMMA fragment.
// A rule appears here only once its kernel has been run on the part, except
// the wide GEMM tile: that one is the fragment-width choice, not a timing.
// The generic wide tile is 64x64 wave tiles. A gfx11 fragment is twice an
// RDNA4 fragment, so that tile holds 16 accumulators and loomc stops the
// engine (budget 256, peak 432, spill-traffic-register-exhausted). The
// 64x32 wave tile is the one gfx1151 measured at 184 VGPRs with no scratch.
// This part has not been timed.
#pragma once

#include "lse/dispatch/arch/tuning_types.hpp"

namespace lse::dispatch::arch::gfx1150 {

inline constexpr std::string_view kArch = "gfx1150";

inline constexpr Q4GemmTile kQ4GemmWide{256, 128, 4, 4};

inline constexpr Tuning kTuning{
    .arch = kArch,
    .q4_gemm_wide = kQ4GemmWide,
};

}  // namespace lse::dispatch::arch::gfx1150
