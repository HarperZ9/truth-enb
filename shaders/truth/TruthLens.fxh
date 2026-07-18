#ifndef TRUTH_LENS_FXH
#define TRUTH_LENS_FXH

#include "TruthQuality.fxh"

float3 TruthApplyLens(float2 uv, float3 bloom, float3 scene)
{
    if (TruthLensIntensity <= 0.0 || TruthQualityLensGhosts == 0u)
    {
        return scene;
    }

    float2 centered = uv - 0.5;
    float radial = dot(centered, centered);
    float3 ghost = 0.0;
    [unroll]
    for (uint ghost_index = 0u; ghost_index < 3u; ++ghost_index)
    {
        if (ghost_index < TruthQualityLensGhosts)
        {
            float scale = 0.12 + (0.08 * float(ghost_index));
            float response = exp2(-radial * (8.0 + 4.0 * float(ghost_index)));
            ghost += bloom * (response * scale);
        }
    }
    float3 veiling_glare = bloom * (0.04 * saturate(1.0 - radial));
    return TruthFiniteOrBlack(
        scene + ((ghost + veiling_glare) * saturate(TruthLensIntensity)));
}

#endif
