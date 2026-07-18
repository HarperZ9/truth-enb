#ifndef TRUTH_BLOOM_FXH
#define TRUTH_BLOOM_FXH

#include "TruthQuality.fxh"

float3 TruthBloomSoftKnee(float3 color, float threshold)
{
    float luminance = dot(max(color, 0.0), float3(0.2126, 0.7152, 0.0722));
    float knee = max(threshold * 0.5, 0.0001);
    float soft = saturate((luminance - threshold + knee) / (2.0 * knee));
    float contribution = max(luminance - threshold, 0.0) + (soft * soft * knee);
    return color * (contribution / max(luminance, 0.0001));
}

float3 TruthApplyBloom(float2 uv, float3 hdr_source)
{
    if (TruthBloomIntensity <= 0.0)
    {
        return hdr_source;
    }

    float2 pixel_size = max(ScreenSize.zw, 0.000001.xx);
    float3 accumulated = 0.0;
    float total_weight = 0.0;
    [unroll]
    for (int offset_index = -6; offset_index <= 6; ++offset_index)
    {
        if (abs(offset_index) <= int(TruthQualityBloomRadius))
        {
            float normalized_offset =
                float(offset_index) / max(float(TruthQualityBloomRadius), 1.0);
            float weight = 1.0 - (0.75 * abs(normalized_offset));
            float2 offset = pixel_size * float2(float(offset_index), normalized_offset);
            float3 sample_color = max(
                TextureColor.SampleLevel(Sampler0, uv + offset, 0.0).rgb,
                0.0);
            accumulated += TruthBloomSoftKnee(
                sample_color, max(TruthBloomThresholdShape, 0.0001)) * weight;
            total_weight += weight;
        }
    }
    float3 bloom = accumulated / max(total_weight, 0.0001);
    return TruthFiniteOrBlack(lerp(hdr_source, bloom, saturate(TruthBloomIntensity)));
}

#endif
