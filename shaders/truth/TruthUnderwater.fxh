#ifndef TRUTH_UNDERWATER_FXH
#define TRUTH_UNDERWATER_FXH

float3 TruthEvaluateUnderwater(
    float2 uv,
    float3 scene,
    float linear_depth)
{
    if (TruthUnderwaterIntensity <= 0.0)
    {
        return scene;
    }

    float distance_through_water = max(linear_depth, 0.0)
        * lerp(0.004, 0.04, saturate(TruthUnderwaterDensityShape));
    float3 absorption = float3(0.42, 0.16, 0.08);
    float3 transmittance = exp2(
        -absorption * distance_through_water * 1.4426950408889634);
    float3 water_radiance = float3(0.015, 0.075, 0.095);
    float3 medium = (scene * transmittance)
        + (water_radiance * (1.0 - transmittance));
    return TruthFiniteOrBlack(
        lerp(scene, medium, saturate(TruthUnderwaterIntensity)));
}

#endif
