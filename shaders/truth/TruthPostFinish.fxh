#ifndef TRUTH_POST_FINISH_FXH
#define TRUTH_POST_FINISH_FXH

float TruthFinishHash(float2 pixel)
{
    float3 value = frac(float3(pixel.xyx) * 0.1031);
    value += dot(value, value.yzx + 33.33);
    return frac((value.x + value.y) * value.z);
}

float3 TruthTriangularDither(float2 uv, float3 color)
{
    float2 pixel = floor(uv * max(ScreenSize.xy, 1.0.xx));
    float triangular = TruthFinishHash(pixel)
        - TruthFinishHash(pixel + float2(17.0, 53.0));
    return saturate(color + (triangular / 255.0));
}

float3 TruthFinishLdr(float2 uv, float3 display_color)
{
    if (TruthPostpassIntensity <= 0.0)
    {
        return display_color;
    }

    float2 centered = uv - 0.5;
    float vignette = saturate(1.0 - dot(centered, centered) * 0.55);
    float3 finished = lerp(
        display_color,
        display_color * vignette,
        0.18 * saturate(TruthPostpassIntensity));
    float grain = (TruthFinishHash(
        floor(uv * max(ScreenSize.xy, 1.0.xx)) + 71.0) - 0.5)
        * (TruthPostpassGrainShape / 255.0);
    finished = saturate(finished + grain);
    return TruthTriangularDither(uv, finished);
}

#endif
