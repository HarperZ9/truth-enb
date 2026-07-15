#ifndef TRUTH_SKY_FIELDS_FXH
#define TRUTH_SKY_FIELDS_FXH

#include "TruthAuroraCurtain.fxh"

static const float TruthSkyPi = 3.14159265358979323846;
static const float TruthSkyTwoPi = 2.0 * TruthSkyPi;
static const float TruthSkyCloudErosionScale = 0.22;

struct TruthSkyFieldInput
{
    float3 view_direction;
    float phase;
    float2 wind;
    float cloud_coverage;
    float cloud_density;
    float weather_density;
    float aurora_activity;
    float night_factor;
    float3 camera_position;
};

struct TruthSkyFieldOutput
{
    float cloud_body;
    float cloud_detail_erosion;
    float cloud_density;
    float aurora_mask;
    float3 aurora_intrinsic_radiance;
};

uint TruthSkyMixBits(uint value)
{
    value ^= value >> 16u;
    value *= 0x7FEB352Du;
    value ^= value >> 15u;
    value *= 0x846CA68Bu;
    value ^= value >> 16u;
    return value;
}

float TruthSkyLatticeHash3D(int3 lattice)
{
    uint3 bits = asuint(lattice);
    uint combined = (bits.x * 0x8DA6B343u)
        ^ (bits.y * 0xD8163841u)
        ^ (bits.z * 0xCB1AB31Fu)
        ^ 0x9E3779B9u;
    uint mixed = TruthSkyMixBits(combined);
    return float(mixed & 0x00FFFFFFu) / 16777215.0;
}

float TruthSkySmooth(float value)
{
    return value * value * (3.0 - (2.0 * value));
}

float TruthSkyValueNoise3D(float3 sample_position)
{
    float3 base = floor(sample_position);
    int3 lattice = int3(base);
    float3 blend = float3(
        TruthSkySmooth(sample_position.x - base.x),
        TruthSkySmooth(sample_position.y - base.y),
        TruthSkySmooth(sample_position.z - base.z));

    float lower_near = lerp(
        TruthSkyLatticeHash3D(lattice),
        TruthSkyLatticeHash3D(lattice + int3(1, 0, 0)),
        blend.x);
    float upper_near = lerp(
        TruthSkyLatticeHash3D(lattice + int3(0, 1, 0)),
        TruthSkyLatticeHash3D(lattice + int3(1, 1, 0)),
        blend.x);
    float near_plane = lerp(lower_near, upper_near, blend.y);

    float lower_far = lerp(
        TruthSkyLatticeHash3D(lattice + int3(0, 0, 1)),
        TruthSkyLatticeHash3D(lattice + int3(1, 0, 1)),
        blend.x);
    float upper_far = lerp(
        TruthSkyLatticeHash3D(lattice + int3(0, 1, 1)),
        TruthSkyLatticeHash3D(lattice + int3(1, 1, 1)),
        blend.x);
    float far_plane = lerp(lower_far, upper_far, blend.y);
    return lerp(near_plane, far_plane, blend.z);
}

float3 TruthSkyScaleAndOffset(float3 sample_point, float scale, float3 offset)
{
    return (sample_point * scale) + offset;
}

float TruthSkySmoothStep(float lower, float upper, float value)
{
    return TruthSkySmooth(saturate((value - lower) / (upper - lower)));
}

TruthSkyFieldOutput TruthEvaluateSkyFields(TruthSkyFieldInput input)
{
    float wrapped_phase = input.phase >= 1.0 ? 0.0 : input.phase;
    float phase_angle = wrapped_phase * TruthSkyTwoPi;
    float phase_sine = sin(phase_angle);
    float phase_cosine = cos(phase_angle);
    float phase_arc = 1.0 - phase_cosine;
    float3 loop_offset = float3(
        (0.48 * input.wind.x * phase_sine) + (0.19 * input.wind.y * phase_arc),
        (0.48 * input.wind.y * phase_sine) - (0.19 * input.wind.x * phase_arc),
        (0.17 * (input.wind.x + input.wind.y) * phase_sine)
            + (0.09 * (input.wind.x - input.wind.y) * phase_arc));
    float3 direction_space = float3(
        (1.62 * input.view_direction.x) + loop_offset.x,
        (1.62 * input.view_direction.y) + loop_offset.y,
        (2.35 * input.view_direction.z) + loop_offset.z);
    float3 domain_warp = float3(
        TruthSkyValueNoise3D(TruthSkyScaleAndOffset(
            direction_space, 0.72, float3(17.1, -4.7, 8.3))) - 0.5,
        TruthSkyValueNoise3D(TruthSkyScaleAndOffset(
            direction_space, 0.72, float3(-9.2, 13.6, 2.8))) - 0.5,
        TruthSkyValueNoise3D(TruthSkyScaleAndOffset(
            direction_space, 0.72, float3(5.4, 7.9, -11.5))) - 0.5);
    float3 warped = direction_space
        + (domain_warp * float3(0.58, 0.58, 0.38));
    float body_broad = TruthSkyValueNoise3D(TruthSkyScaleAndOffset(
        warped, 0.62, float3(1.7, -3.2, 5.1)));
    float body_strata = TruthSkyValueNoise3D(TruthSkyScaleAndOffset(
        warped, 1.24, float3(-6.4, 8.8, 2.3)));
    float body_breakup = TruthSkyValueNoise3D(TruthSkyScaleAndOffset(
        warped, 2.48, float3(12.9, 4.6, -7.7)));
    float cloud_body = saturate(
        0.075
        + (0.52 * body_broad)
        + (0.31 * body_strata)
        + (0.17 * body_breakup));
    float detail_primary = TruthSkyValueNoise3D(TruthSkyScaleAndOffset(
        warped, 5.1, float3(-5.4, 9.2, 3.1)));
    float detail_fine = TruthSkyValueNoise3D(TruthSkyScaleAndOffset(
        warped, 10.3, float3(3.8, -12.6, 7.4)));
    float detail_noise = saturate((0.64 * detail_primary) + (0.36 * detail_fine));
    float detail_erosion = detail_noise * TruthSkyCloudErosionScale;
    float coverage_threshold = lerp(0.72, 0.30, input.cloud_coverage);
    float coverage_gate = TruthSkySmoothStep(0.0, 0.20, input.cloud_coverage);
    float occupied_body = input.cloud_coverage == 0.0
        ? 0.0
        : TruthSkySmoothStep(
            coverage_threshold - 0.12,
            coverage_threshold + 0.42,
            cloud_body) * coverage_gate;
    float weather_scale = 0.35 + (0.65 * input.weather_density);

    TruthSkyFieldOutput output;
    output.cloud_body = cloud_body;
    output.cloud_detail_erosion = detail_erosion;
    output.cloud_density = saturate(
        max(occupied_body - detail_erosion, 0.0) * input.cloud_density * weather_scale);

    TruthAuroraCurtainInput aurora_input;
    aurora_input.camera_position = input.camera_position;
    aurora_input.view_direction = input.view_direction;
    aurora_input.phase = input.phase;
    aurora_input.wind = input.wind;
    aurora_input.activity = input.aurora_activity;
    aurora_input.night_factor = input.night_factor;
    TruthAuroraCurtainOutput aurora = TruthEvaluateAuroraCurtain(aurora_input);
    output.aurora_mask = aurora.mask;
    output.aurora_intrinsic_radiance = aurora.intrinsic_radiance;
    return output;
}

#endif
