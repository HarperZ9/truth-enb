#ifndef TRUTH_SKY_FIELDS_FXH
#define TRUTH_SKY_FIELDS_FXH

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

    if (input.night_factor == 0.0)
    {
        output.aurora_mask = 0.0;
        output.aurora_intrinsic_radiance = float3(0.0, 0.0, 0.0);
        return output;
    }

    float horizontal_length = max(length(input.view_direction.xy), 0.001);
    float side = input.view_direction.x / horizontal_length;
    float forward = input.view_direction.y / horizontal_length;
    float arc_gate = TruthSkySmoothStep(-0.42, 0.02, forward);
    float fold_noise = TruthSkyValueNoise3D(float3(
        (2.25 * side) + (0.42 * loop_offset.x) + 4.8,
        (1.65 * forward) + (0.42 * loop_offset.y) - 7.2,
        1.4 + (0.55 * loop_offset.z)));
    float curtain_center = 0.70
        + (0.085 * sin((2.8 * side) + (0.7 * forward)
                       + (0.55 * phase_sine)))
        + (0.052 * sin((6.3 * side) - (1.1 * forward)
                       - (0.45 * phase_cosine)))
        + (0.055 * (fold_noise - 0.5));
    float lower_edge = curtain_center - 0.37 - (0.035 * fold_noise);
    float upper_edge = curtain_center + 0.22 + (0.020 * fold_noise);
    float lower_falloff = TruthSkySmoothStep(
        lower_edge,
        curtain_center - 0.045,
        input.view_direction.z);
    float upper_falloff = 1.0 - TruthSkySmoothStep(
        curtain_center + 0.045,
        upper_edge,
        input.view_direction.z);
    float ray_primary = TruthSkyValueNoise3D(float3(
        (8.4 * side) + loop_offset.x + 10.7,
        (8.4 * forward) + loop_offset.y - 3.9,
        2.6 + loop_offset.z));
    float ray_fine = TruthSkyValueNoise3D(float3(
        (17.2 * side) + (1.7 * loop_offset.x) - 6.1,
        (17.2 * forward) + (1.7 * loop_offset.y) + 12.4,
        -4.3 + (1.3 * loop_offset.z)));
    float vertical_rays = 0.48
        + (0.52 * ((0.68 * ray_primary) + (0.32 * ray_fine)));
    float folded_sheet = 0.72
        + (0.28 * (1.0 - abs((2.0 * fold_noise) - 1.0)));
    output.aurora_mask = saturate(
        arc_gate
        * lower_falloff
        * upper_falloff
        * vertical_rays
        * folded_sheet
        * input.night_factor);

    if (input.aurora_activity == 0.0 || output.aurora_mask == 0.0)
    {
        output.aurora_intrinsic_radiance = float3(0.0, 0.0, 0.0);
    }
    else
    {
        float upper_fringe = TruthSkySmoothStep(
            curtain_center - 0.10,
            curtain_center + 0.22,
            input.view_direction.z);
        float fringe_blend = 0.68 * upper_fringe;
        float altitude_gain = 0.39
            + (0.81 * TruthSkySmoothStep(
                curtain_center - 0.16,
                curtain_center + 0.12,
                input.view_direction.z));
        float3 color = lerp(float3(0.07, 0.82, 0.32),
                            float3(0.28, 0.42, 0.78),
                            fringe_blend);
        output.aurora_intrinsic_radiance = color
            * output.aurora_mask
            * input.aurora_activity
            * altitude_gain;
    }
    return output;
}

#endif
