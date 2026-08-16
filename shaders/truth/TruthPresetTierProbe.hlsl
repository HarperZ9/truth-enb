// TruthPresetTierProbe.hlsl - compile-time contract probe for TruthQuality.fxh.
//
// Define TRUTH_EXPECTED_TIER and compile this probe. TruthQuality.fxh must
// resolve TRUTH_QUALITY_TIER from the command line, from the installed preset
// override include, or from the guarded Balanced fallback.

#ifndef TRUTH_EXPECTED_TIER
#error TRUTH_EXPECTED_TIER must be defined for TruthPresetTierProbe.hlsl
#endif

#include "TruthQuality.fxh"

#if TRUTH_QUALITY_TIER != TRUTH_EXPECTED_TIER
#error TRUTH_QUALITY_TIER did not resolve to TRUTH_EXPECTED_TIER
#endif

RWStructuredBuffer<uint> TruthPresetTierProbeOutput : register(u0);

[numthreads(1, 1, 1)]
void TruthPresetTierProbeMain(uint3 dispatch_id : SV_DispatchThreadID)
{
    if (any(dispatch_id != uint3(0, 0, 0)))
    {
        return;
    }

    TruthPresetTierProbeOutput[0] = TruthQualityTier;
}
