#ifndef TRUTH_DEPTH_OF_FIELD_FXH
#define TRUTH_DEPTH_OF_FIELD_FXH

#include "TruthQuality.fxh"

float3 TruthApplyDepthOfField(float2 uv, float3 scene, float linear_depth)
{
#if TRUTH_QUALITY_TIER == 0
    return scene;
#else
    float focus_distance = max(FocusInfo.x, 0.0001);
    float focus_range = max(FocusInfo.y, 0.0001);
    float signed_coc = (linear_depth - focus_distance) / focus_range;
    float coc = saturate(abs(signed_coc))
        * saturate(TruthDepthOfFieldFocusShape);
    if (coc <= 0.0001 || TruthQualityDOFRings == 0u)
    {
        return scene;
    }

    static const float TruthTau = 6.28318530717958647692;
    float2 pixel_size = max(ScreenSize.zw, 0.000001.xx);
    float3 accumulated = scene;
    float total_weight = 1.0;
    [loop]
    for (uint ring = 1u; ring <= TruthQualityDOFRings; ++ring)
    {
        float ring_fraction = float(ring) / float(TruthQualityDOFRings);
        [unroll]
        for (uint sample_index = 0u; sample_index < 6u; ++sample_index)
        {
            float angle = TruthTau * (float(sample_index) / 6.0)
                + (0.5 * ring_fraction);
            float2 offset = float2(cos(angle), sin(angle))
                * pixel_size * (ring_fraction * coc * 8.0);
            accumulated += max(
                TextureColor.SampleLevel(Sampler0, uv + offset, 0.0).rgb,
                0.0);
            total_weight += 1.0;
        }
    }
    return TruthFiniteOrBlack(accumulated / total_weight);
#endif
}

#endif
