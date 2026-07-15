#ifndef TRUTH_SKY_FIELDS_FXH
#define TRUTH_SKY_FIELDS_FXH

static const float TruthSkyPi = 3.14159265358979323846;
static const float TruthSkyTwoPi = 2.0 * TruthSkyPi;
static const float TruthSkyCloudCoverageSlope = 3.75;
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

float TruthSkyLatticeHash(int2 lattice)
{
    uint2 bits = asuint(lattice);
    uint rotated_y = (bits.y << 16u) | (bits.y >> 16u);
    uint mixed = TruthSkyMixBits(bits.x ^ rotated_y ^ 0x9E3779B9u);
    return float(mixed & 0x00FFFFFFu) / 16777215.0;
}

float TruthSkySmooth(float value)
{
    return value * value * (3.0 - (2.0 * value));
}

float TruthSkyValueNoise(float2 sample_position)
{
    float2 base = floor(sample_position);
    int2 lattice = int2(base);
    float2 blend = float2(TruthSkySmooth(sample_position.x - base.x),
                          TruthSkySmooth(sample_position.y - base.y));
    float lower = lerp(TruthSkyLatticeHash(lattice),
                       TruthSkyLatticeHash(lattice + int2(1, 0)),
                       blend.x);
    float upper = lerp(TruthSkyLatticeHash(lattice + int2(0, 1)),
                       TruthSkyLatticeHash(lattice + int2(1, 1)),
                       blend.x);
    return lerp(lower, upper, blend.y);
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
    float2 loop_offset = float2(
        (0.82 * input.wind.x * phase_sine) + (0.31 * input.wind.y * phase_arc),
        (0.82 * input.wind.y * phase_sine) - (0.31 * input.wind.x * phase_arc));

    float2 spatial = float2(
        (2.4 * input.view_direction.x) + (0.73 * input.view_direction.z),
        (2.4 * input.view_direction.y) - (0.41 * input.view_direction.z)) + loop_offset;
    float body_low = TruthSkyValueNoise(0.85 * spatial);
    float body_fold = TruthSkyValueNoise((1.72 * spatial) + float2(11.3, -7.1));
    float cloud_body = saturate((0.67 * body_low) + (0.33 * body_fold));
    float detail_primary = TruthSkyValueNoise((6.7 * spatial) + float2(-5.4, 9.2));
    float detail_fine = TruthSkyValueNoise((13.9 * spatial) + float2(3.8, -12.6));
    float detail_noise = saturate((0.55 * detail_primary) + (0.45 * detail_fine));
    float detail_erosion = detail_noise * TruthSkyCloudErosionScale;
    float occupied_body = saturate(
        (cloud_body + input.cloud_coverage - 1.0) * TruthSkyCloudCoverageSlope);
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

    float curtain_center = (0.22 * sin((4.0 * input.view_direction.x)
                                       + (0.65 * phase_sine)))
        + (0.10 * sin((9.0 * input.view_direction.x) - (0.45 * phase_cosine)));
    float curtain_distance = abs(input.view_direction.y - curtain_center);
    float curtain = saturate(1.0 - (curtain_distance / 0.55));
    curtain *= curtain;
    float horizon_gate = TruthSkySmoothStep(-0.05, 0.28, input.view_direction.z);
    float zenith_gate = 1.0 - TruthSkySmoothStep(0.82, 1.0, input.view_direction.z);
    float fold = 0.38 + (0.62 * abs(sin(
        (17.0 * input.view_direction.x)
        + (3.0 * input.view_direction.y)
        + phase_angle
        + (2.0 * cloud_body))));
    output.aurora_mask = saturate(
        curtain * horizon_gate * zenith_gate * fold * input.night_factor);

    if (input.aurora_activity == 0.0 || output.aurora_mask == 0.0)
    {
        output.aurora_intrinsic_radiance = float3(0.0, 0.0, 0.0);
    }
    else
    {
        float hue = 0.5 + (0.5 * sin((6.5 * input.view_direction.x)
                                    - (2.0 * input.view_direction.y)
                                    + phase_angle));
        float3 color = lerp(float3(0.08, 0.78, 0.42),
                            float3(0.38, 0.42, 0.88),
                            hue);
        output.aurora_intrinsic_radiance = color
            * output.aurora_mask
            * input.aurora_activity;
    }
    return output;
}

#endif
