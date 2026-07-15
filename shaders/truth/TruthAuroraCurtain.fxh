#ifndef TRUTH_AURORA_CURTAIN_FXH
#define TRUTH_AURORA_CURTAIN_FXH

#ifndef TRUTH_AURORA_QUALITY
#define TRUTH_AURORA_QUALITY 2
#endif

#if TRUTH_AURORA_QUALITY == 0
static const uint TruthAuroraCurtainSamples = 1u;
#elif TRUTH_AURORA_QUALITY == 1
static const uint TruthAuroraCurtainSamples = 4u;
#elif TRUTH_AURORA_QUALITY == 2
static const uint TruthAuroraCurtainSamples = 7u;
#elif TRUTH_AURORA_QUALITY == 3
static const uint TruthAuroraCurtainSamples = 10u;
#else
#error TRUTH_AURORA_QUALITY must be 0, 1, 2, or 3
#endif

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

float TruthAuroraLatticeHash3D(int3 lattice)
{
    uint3 bits = asuint(lattice);
    uint combined = (bits.x * 0xA24BAED5u)
        ^ (bits.y * 0x9FB21C65u)
        ^ (bits.z * 0xC13FA9A9u)
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

float TruthAuroraValueNoise3D(float3 sample_position)
{
    float3 base = floor(sample_position);
    int3 lattice = int3(base);
    float3 blend = float3(
        TruthAuroraSmooth(sample_position.x - base.x),
        TruthAuroraSmooth(sample_position.y - base.y),
        TruthAuroraSmooth(sample_position.z - base.z));
    float lower_near = lerp(
        TruthAuroraLatticeHash3D(lattice),
        TruthAuroraLatticeHash3D(lattice + int3(1, 0, 0)),
        blend.x);
    float upper_near = lerp(
        TruthAuroraLatticeHash3D(lattice + int3(0, 1, 0)),
        TruthAuroraLatticeHash3D(lattice + int3(1, 1, 0)),
        blend.x);
    float near_plane = lerp(lower_near, upper_near, blend.y);
    float lower_far = lerp(
        TruthAuroraLatticeHash3D(lattice + int3(0, 0, 1)),
        TruthAuroraLatticeHash3D(lattice + int3(1, 0, 1)),
        blend.x);
    float upper_far = lerp(
        TruthAuroraLatticeHash3D(lattice + int3(0, 1, 1)),
        TruthAuroraLatticeHash3D(lattice + int3(1, 1, 1)),
        blend.x);
    float far_plane = lerp(lower_far, upper_far, blend.y);
    return lerp(near_plane, far_plane, blend.z);
}

float TruthAuroraGaussian(float value, float center, float standard_deviation)
{
    float normalized = (value - center) / standard_deviation;
    return exp(-0.5 * normalized * normalized);
}

float3 TruthEvaluateAuroraDepositionProfile(float normalized_height)
{
    float height = saturate(normalized_height);
    float green = TruthAuroraGaussian(height, 0.32, 0.13);
    float blue = saturate(
        (0.84 * TruthAuroraGaussian(height, 0.29, 0.115))
        + (0.16 * TruthAuroraGaussian(height, 0.46, 0.17)));
    float red = TruthAuroraGaussian(height, 0.72, 0.23);
    return float3(red, green, blue);
}

float2 TruthEvaluateAuroraElectronFlux(
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
    float advected_along = along
        + (0.78 * wind.x * phase_sine)
        + (0.31 * wind.y * phase_arc);
    float advected_across = across
        + (0.64 * wind.y * phase_sine)
        - (0.24 * wind.x * phase_arc);

    float broad_warp = TruthAuroraValueNoise3D(float3(
        (0.055 * advected_along) + (0.42 * phase_sine) + 11.3,
        (0.045 * advected_across) + (0.42 * phase_cosine) - 7.1,
        2.9 + (0.35 * phase_sine))) - 0.5;
    float curl_warp = TruthAuroraValueNoise3D(float3(
        (0.13 * advected_along) - (0.37 * phase_cosine) - 5.8,
        (0.10 * advected_across) + (0.37 * phase_sine) + 9.6,
        -4.2 + (0.29 * phase_cosine))) - 0.5;
    float center = 10.45
        + (0.0060 * advected_along * advected_along)
        + (1.15 * sin((0.115 * advected_along) + (0.55 * phase_sine)))
        + (0.52 * sin((0.34 * advected_along) - (0.42 * phase_cosine)))
        + (1.75 * broad_warp)
        + (0.58 * curl_warp);
    float primary_width = 1.05
        + (0.40 * TruthAuroraValueNoise3D(float3(
            (0.085 * advected_along) + 3.2,
            (0.065 * advected_across) - 8.4,
            7.7 + (0.25 * phase_sine))));
    float primary_distance = (advected_across - center) / primary_width;
    float primary_sheet = exp(-0.5 * primary_distance * primary_distance);
    float secondary_center = center + 4.35
        + (0.62 * sin((0.19 * advected_along) + 1.4 - phase_sine));
    float secondary_distance = (advected_across - secondary_center) / 1.55;
    float secondary_sheet = exp(-0.5 * secondary_distance * secondary_distance);
    float normalized_along = advected_along / 29.0;
    float along_envelope = exp(-0.5 * normalized_along * normalized_along);

    float coarse_filament = TruthAuroraValueNoise3D(float3(
        (0.24 * advected_along) + (0.75 * phase_sine) + 4.8,
        (0.035 * advected_across) + (0.75 * phase_cosine) - 3.1,
        12.7 + (0.45 * phase_sine)));
    float fine_filament = TruthAuroraValueNoise3D(float3(
        (0.92 * advected_along) - (1.15 * phase_cosine) - 8.2,
        (0.055 * advected_across) + (1.15 * phase_sine) + 5.7,
        -6.4 + (0.63 * phase_cosine)));
    float filament_wave = 0.5 + (0.5 * sin(
        (2.75 * advected_along) + (1.9 * curl_warp) + (0.8 * phase_sine)));
    float filament_signal = (0.53 * coarse_filament)
        + (0.31 * fine_filament)
        + (0.16 * filament_wave);
    float filaments = 0.52
        + (0.48 * TruthAuroraSmoothStep(0.30, 0.78, filament_signal));
    float sheet = saturate(primary_sheet + (0.38 * secondary_sheet));
    float persistent_sheet = saturate(
        primary_sheet + (0.52 * secondary_sheet));
    return saturate(float2(
        along_envelope * sheet * filaments,
        along_envelope
            * persistent_sheet
            * (0.70 + (0.30 * coarse_filament))));
}

TruthAuroraCurtainOutput TruthEvaluateAuroraCurtain(
    TruthAuroraCurtainInput input)
{
    TruthAuroraCurtainOutput output;
    output.mask = 0.0;
    output.intrinsic_radiance = float3(0.0, 0.0, 0.0);
    output.samples = 0u;
    if (input.night_factor == 0.0
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
            float2 flux = TruthEvaluateAuroraElectronFlux(
                world_position.x,
                world_position.y,
                phase_sine,
                phase_cosine,
                input.wind);
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
