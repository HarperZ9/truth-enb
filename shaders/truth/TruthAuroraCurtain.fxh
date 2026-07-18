#ifndef TRUTH_AURORA_CURTAIN_FXH
#define TRUTH_AURORA_CURTAIN_FXH

#include "TruthQuality.fxh"

static const uint TruthAuroraCurtainSamples = TruthQualityAuroraSamples;

static const float TruthAuroraPi = 3.14159265358979323846;
static const float TruthAuroraTwoPi = 2.0 * TruthAuroraPi;
static const float TruthAuroraBaseHeight = 8.0;
static const float TruthAuroraTopHeight = 16.0;
static const float TruthAuroraMaximumRayDistance = 180.0;
static const float TruthAuroraMinimumRayElevation = 0.0001;
static const float TruthAuroraPathLengthFloor = 0.08;
static const float TruthAuroraMaximumStepPathWeight = 0.55;

struct TruthAuroraCurtainInput
{
    float3 camera_position;
    float3 view_direction;
    float phase;
    float2 wind;
    float activity;
    float night_factor;
};

struct TruthAuroraCurtainOutput
{
    float mask;
    float3 intrinsic_radiance;
    uint samples;
};

uint TruthAuroraMixBits(uint value)
{
    value ^= value >> 16u;
    value *= 0x7FEB352Du;
    value ^= value >> 15u;
    value *= 0x846CA68Bu;
    value ^= value >> 16u;
    return value;
}

float TruthAuroraLatticeHash2D(int2 lattice)
{
    uint2 bits = asuint(lattice);
    uint combined = (bits.x * 0xA24BAED5u)
        ^ (bits.y * 0x9FB21C65u)
        ^ 0x91E10DA5u;
    return float(TruthAuroraMixBits(combined) & 0x00FFFFFFu) / 16777215.0;
}

float TruthAuroraSmooth(float value)
{
    return value * value * (3.0 - (2.0 * value));
}

float TruthAuroraSmoothStep(float lower, float upper, float value)
{
    return TruthAuroraSmooth(saturate((value - lower) / (upper - lower)));
}

float TruthAuroraValueNoise2D(float2 sample_position)
{
    float2 base = floor(sample_position);
    int2 lattice = int2(base);
    float2 blend = float2(
        TruthAuroraSmooth(sample_position.x - base.x),
        TruthAuroraSmooth(sample_position.y - base.y));
    float lower = lerp(
        TruthAuroraLatticeHash2D(lattice),
        TruthAuroraLatticeHash2D(lattice + int2(1, 0)),
        blend.x);
    float upper = lerp(
        TruthAuroraLatticeHash2D(lattice + int2(0, 1)),
        TruthAuroraLatticeHash2D(lattice + int2(1, 1)),
        blend.x);
    return lerp(lower, upper, blend.y);
}

float TruthAuroraCompactWindow(float value, float center, float half_width)
{
    float distance = abs((value - center) / half_width);
    return TruthAuroraSmooth(saturate(1.0 - distance));
}

float3 TruthEvaluateAuroraDepositionProfile(float normalized_height)
{
    float height = saturate(normalized_height);
    float green = TruthAuroraCompactWindow(height, 0.32, 0.30);
    float blue = saturate(
        (0.84 * TruthAuroraCompactWindow(height, 0.29, 0.27))
        + (0.16 * TruthAuroraCompactWindow(height, 0.46, 0.34)));
    float red = TruthAuroraCompactWindow(height, 0.72, 0.55);
    return float3(red, green, blue);
}

float2 TruthEvaluateAuroraFieldCoordinates(
    float world_x,
    float world_y,
    float phase_sine,
    float phase_cosine,
    float2 wind)
{
    const float basis_cosine = 0.96105546;
    const float basis_sine = 0.27635565;
    float along = (basis_cosine * world_x) + (basis_sine * world_y);
    float across = (-basis_sine * world_x) + (basis_cosine * world_y);
    float phase_arc = 1.0 - phase_cosine;
    return float2(
        along
        + (0.78 * wind.x * phase_sine)
        + (0.31 * wind.y * phase_arc),
        across
        + (0.64 * wind.y * phase_sine)
        - (0.24 * wind.x * phase_arc));
}

float3 TruthEvaluateAuroraFieldContext(
    float2 coordinates,
    float phase_sine,
    float phase_cosine)
{
    return float3(
        TruthAuroraValueNoise2D(float2(
            (0.055 * coordinates.x) + (0.42 * phase_sine) + 11.3,
            (0.045 * coordinates.y) + (0.42 * phase_cosine) - 7.1)) - 0.5,
        TruthAuroraValueNoise2D(float2(
            (0.13 * coordinates.x) - (0.37 * phase_cosine) - 5.8,
            (0.10 * coordinates.y) + (0.37 * phase_sine) + 9.6)) - 0.5,
        TruthAuroraValueNoise2D(float2(
            (0.24 * coordinates.x) + (0.75 * phase_sine) + 4.8,
            (0.035 * coordinates.y) + (0.75 * phase_cosine) - 3.1)));
}

float TruthAuroraCurtainCenter(
    float2 coordinates,
    float3 context,
    float phase_sine,
    float phase_cosine)
{
    float center = 10.45
        + (0.0060 * coordinates.x * coordinates.x)
        + (1.15 * sin((0.115 * coordinates.x) + (0.55 * phase_sine)))
        + (0.52 * sin((0.34 * coordinates.x) - (0.42 * phase_cosine)))
        + (1.75 * context.x)
        + (0.58 * context.y);
    return center;
}

float TruthAuroraPrimaryWidth(float2 coordinates, float phase_sine)
{
    return 1.05 + (0.20 * (1.0 + sin(
        (0.085 * coordinates.x)
        + (0.065 * coordinates.y)
        + (0.25 * phase_sine)
        + 3.2)));
}

float TruthEvaluateAuroraCoarseSupport(
    float2 coordinates,
    float3 context,
    float phase_sine,
    float phase_cosine,
    float margin)
{
    float center = TruthAuroraCurtainCenter(
        coordinates, context, phase_sine, phase_cosine);
    float primary_distance = (coordinates.y - center)
        / TruthAuroraPrimaryWidth(coordinates, phase_sine);
    float secondary_center = center + 4.35
        + (0.62 * sin((0.19 * coordinates.x) + 1.4 - phase_sine));
    float secondary_distance = (coordinates.y - secondary_center) / 1.55;
    float sheet = saturate(
        TruthAuroraCompactWindow(primary_distance, 0.0, margin)
        + (0.52 * TruthAuroraCompactWindow(
            secondary_distance, 0.0, margin)));
    return TruthAuroraCompactWindow(coordinates.x, 0.0, 110.0) * sheet;
}

float2 TruthEvaluateAuroraElectronFlux(
    float2 coordinates,
    float3 context,
    float phase_sine,
    float phase_cosine)
{
    float center = TruthAuroraCurtainCenter(
        coordinates, context, phase_sine, phase_cosine);
    float primary_distance = (coordinates.y - center)
        / TruthAuroraPrimaryWidth(coordinates, phase_sine);
    float primary_sheet = TruthAuroraCompactWindow(
        primary_distance, 0.0, 2.75);
    float secondary_center = center + 4.35
        + (0.62 * sin((0.19 * coordinates.x) + 1.4 - phase_sine));
    float secondary_distance = (coordinates.y - secondary_center) / 1.55;
    float secondary_sheet = TruthAuroraCompactWindow(
        secondary_distance, 0.0, 2.75);
    float along_envelope = TruthAuroraCompactWindow(
        coordinates.x, 0.0, 82.0);
    float coarse_wave = 0.5 + (0.5 * sin(
        (0.63 * coordinates.x)
        + (0.17 * coordinates.y)
        + (3.2 * context.x)
        + (0.55 * phase_sine)));
    float fine_wave = 0.5 + (0.5 * sin(
        (2.75 * coordinates.x)
        + (1.9 * context.y)
        + (0.8 * phase_sine)));
    float filament_signal = (0.42 * context.z)
        + (0.34 * coarse_wave)
        + (0.24 * fine_wave);
    float filaments = 0.50
        + (0.50 * TruthAuroraSmoothStep(0.30, 0.78, filament_signal));
    float sheet = saturate(primary_sheet + (0.38 * secondary_sheet));
    float persistent_sheet = saturate(
        primary_sheet + (0.52 * secondary_sheet));
    return saturate(float2(
        along_envelope * sheet * filaments,
        along_envelope
            * persistent_sheet
            * (0.70 + (0.30 * context.z))));
}

TruthAuroraCurtainOutput TruthEvaluateAuroraCurtain(
    TruthAuroraCurtainInput input)
{
    TruthAuroraCurtainOutput output;
    output.mask = 0.0;
    output.intrinsic_radiance = float3(0.0, 0.0, 0.0);
    output.samples = 0u;
    if (input.activity == 0.0
        || input.night_factor == 0.0
        || input.view_direction.z <= TruthAuroraMinimumRayElevation
        || input.camera_position.z >= TruthAuroraTopHeight)
    {
        return output;
    }

    float wrapped_phase = input.phase >= 1.0 ? 0.0 : input.phase;
    float phase_angle = wrapped_phase * TruthAuroraTwoPi;
    float phase_sine = sin(phase_angle);
    float phase_cosine = cos(phase_angle);
    float lower_height = max(TruthAuroraBaseHeight, input.camera_position.z);
    float height_span = TruthAuroraTopHeight - lower_height;
    float height_step = height_span / float(TruthAuroraCurtainSamples);
    float path_denominator = max(input.view_direction.z, TruthAuroraPathLengthFloor);
    float normalized_step_path = min(
        height_step
            / ((TruthAuroraTopHeight - TruthAuroraBaseHeight) * path_denominator),
        TruthAuroraMaximumStepPathWeight);
    float horizon_fade = TruthAuroraSmoothStep(
        0.035, 0.14, input.view_direction.z);

    const float coarse_height_fractions[3] = {0.0, 0.5, 1.0};
    float2 coarse_coordinates[3];
    uint coarse_coordinate_count = 0u;
    [unroll]
    for (uint coarse_index = 0u; coarse_index < 3u; ++coarse_index)
    {
        float coarse_height = lower_height
            + (height_span * coarse_height_fractions[coarse_index]);
        float coarse_distance = (coarse_height - input.camera_position.z)
            / input.view_direction.z;
        if (coarse_distance <= TruthAuroraMaximumRayDistance)
        {
            float3 coarse_world_position = input.camera_position
                + (input.view_direction * coarse_distance);
            coarse_coordinates[coarse_coordinate_count++] =
                TruthEvaluateAuroraFieldCoordinates(
                    coarse_world_position.x,
                    coarse_world_position.y,
                    phase_sine,
                    phase_cosine,
                    input.wind);
        }
    }
    if (coarse_coordinate_count == 0u)
    {
        return output;
    }

    float unwarped_support = 0.0;
    [loop]
    for (uint support_index = 0u;
         support_index < coarse_coordinate_count;
         ++support_index)
    {
        unwarped_support = max(
            unwarped_support,
            TruthEvaluateAuroraCoarseSupport(
                coarse_coordinates[support_index],
                float3(0.0, 0.0, 0.0),
                phase_sine,
                phase_cosine,
                6.0));
    }
    if (unwarped_support == 0.0)
    {
        return output;
    }

    float2 representative =
        coarse_coordinates[coarse_coordinate_count / 2u];
    float3 field_context = TruthEvaluateAuroraFieldContext(
        representative, phase_sine, phase_cosine);
    float warped_support = 0.0;
    [loop]
    for (uint warped_index = 0u;
         warped_index < coarse_coordinate_count;
         ++warped_index)
    {
        warped_support = max(
            warped_support,
            TruthEvaluateAuroraCoarseSupport(
                coarse_coordinates[warped_index],
                field_context,
                phase_sine,
                phase_cosine,
                4.0));
    }
    if (warped_support == 0.0)
    {
        return output;
    }

    float mask = 0.0;
    float3 radiance = float3(0.0, 0.0, 0.0);

    [unroll]
    for (uint index = 0u; index < TruthAuroraCurtainSamples; ++index)
    {
        float unit_height = (float(index) + 0.5)
            / float(TruthAuroraCurtainSamples);
        float height = lower_height + (height_span * unit_height);
        float distance = (height - input.camera_position.z)
            / input.view_direction.z;
        if (distance <= TruthAuroraMaximumRayDistance)
        {
            float3 world_position = input.camera_position
                + (input.view_direction * distance);
            float2 coordinates = TruthEvaluateAuroraFieldCoordinates(
                world_position.x,
                world_position.y,
                phase_sine,
                phase_cosine,
                input.wind);
            float2 flux = TruthEvaluateAuroraElectronFlux(
                coordinates,
                field_context,
                phase_sine,
                phase_cosine);
            float normalized_altitude = (height - TruthAuroraBaseHeight)
                / (TruthAuroraTopHeight - TruthAuroraBaseHeight);
            float3 deposition = TruthEvaluateAuroraDepositionProfile(
                normalized_altitude);
            float blue_flux = (0.82 * flux.x) + (0.18 * flux.y);
            mask += max(flux.x, 0.55 * flux.y)
                * max(deposition.r, max(deposition.g, deposition.b))
                * normalized_step_path;
            radiance.r += 0.075
                * deposition.r
                * flux.y
                * normalized_step_path;
            radiance.g += 0.310
                * deposition.g
                * flux.x
                * normalized_step_path;
            radiance.b += 0.100
                * deposition.b
                * blue_flux
                * normalized_step_path;
        }
    }

    float visibility = horizon_fade * input.night_factor;
    output.mask = saturate(0.65 * mask * visibility);
    output.intrinsic_radiance = saturate(
        radiance * visibility * input.activity);
    output.samples = TruthAuroraCurtainSamples;
    return output;
}

#endif
