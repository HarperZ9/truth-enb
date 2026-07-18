#ifndef TRUTH_CLOUD_VOLUME_FXH
#define TRUTH_CLOUD_VOLUME_FXH

#include "TruthQuality.fxh"

#ifdef TRUTH_ENABLE_CLOUD_VOLUME
#undef TRUTH_ENABLE_CLOUD_VOLUME
#endif

#if TRUTH_QUALITY_TIER < 2
#define TRUTH_ENABLE_CLOUD_VOLUME 0
#else
#define TRUTH_ENABLE_CLOUD_VOLUME 1
#endif

static const uint TruthCloudVolumePrimarySteps = TruthQualityCloudPrimarySteps;
static const uint TruthCloudVolumeLightSteps = TruthQualityCloudLightSteps;
static const float TruthCloudVolumeTwoPi = 6.28318530717958647692;
static const float TruthCloudVolumeExtinction = 1.35;
static const float TruthCloudVolumeShadowExtinction = 1.10;
static const float TruthCloudVolumeMinimumTransmittance = 0.015;
static const float TruthCloudVolumeMaximumOpticalDepth = 64.0;

struct TruthCloudVolumeInput
{
    float3 camera_position;
    float3 view_direction;
    float3 sun_direction;
    float cloud_base_height;
    float cloud_top_height;
    float max_distance;
    float phase;
    float2 wind;
    float cloud_coverage;
    float cloud_density;
    float weather_density;
    float cloud_type;
    float night_factor;
    uint2 pixel_coordinate;
    uint jitter_frame;
};

struct TruthCloudVolumeOutput
{
    float3 radiance;
    float transmittance;
    float optical_depth;
    uint primary_steps;
    uint light_samples;
};

#if TRUTH_QUALITY_TIER >= 2

float TruthCloudVolumeSmooth(float value)
{
    return value * value * (3.0 - (2.0 * value));
}

float TruthCloudVolumeSmoothStep(float lower, float upper, float value)
{
    return TruthCloudVolumeSmooth(saturate((value - lower) / (upper - lower)));
}

bool TruthIntersectCloudVolumeLayer(
    float camera_height,
    float view_vertical,
    float cloud_base_height,
    float cloud_top_height,
    float max_distance,
    out float2 intersection)
{
    if (abs(view_vertical) < 1.0e-6)
    {
        intersection = 0.0;
        return false;
    }
    float first = (cloud_base_height - camera_height) / view_vertical;
    float second = (cloud_top_height - camera_height) / view_vertical;
    float near_distance = max(min(first, second), 0.0);
    float far_distance = min(max(first, second), max_distance);
    intersection = float2(near_distance, far_distance);
    return far_distance > near_distance;
}

float TruthCloudVolumeInterleavedJitter(uint2 pixel_coordinate, uint frame)
{
    uint seed = (pixel_coordinate.x * 0x8DA6B343u)
        ^ (pixel_coordinate.y * 0xD8163841u)
        ^ (frame * 0xCB1AB31Fu)
        ^ 0xA511E9B3u;
    return float(TruthSkyMixBits(seed) & 0x00FFFFFFu) / 16777216.0;
}

float TruthCloudVolumeVerticalProfile(float normalized_height, float cloud_type)
{
    if (normalized_height <= 0.0 || normalized_height >= 1.0)
    {
        return 0.0;
    }
    float height = saturate(normalized_height);
    float type = saturate(cloud_type);
    float stratus = TruthCloudVolumeSmoothStep(0.0, 0.08, height)
        * (1.0 - TruthCloudVolumeSmoothStep(0.58, 0.82, height));
    float cumulus = TruthCloudVolumeSmoothStep(0.0, 0.18, height)
        * (1.0 - TruthCloudVolumeSmoothStep(0.76, 1.0, height));
    float anvil_body = TruthCloudVolumeSmoothStep(0.0, 0.12, height)
        * (1.0 - TruthCloudVolumeSmoothStep(0.90, 1.0, height));
    float anvil_shelf = 0.58
        + (0.42 * TruthCloudVolumeSmoothStep(0.48, 0.68, height));
    float anvil = anvil_body * anvil_shelf;
    if (type <= 0.5)
    {
        return lerp(stratus, cumulus, type * 2.0);
    }
    return lerp(cumulus, anvil, (type - 0.5) * 2.0);
}

float3 TruthCloudVolumePhaseMotion(TruthCloudVolumeInput input)
{
    float wrapped_phase = input.phase >= 1.0 ? 0.0 : input.phase;
    float angle = wrapped_phase * TruthCloudVolumeTwoPi;
    float phase_sine = sin(angle);
    float phase_arc = 1.0 - cos(angle);
    return float3(
        (2.4 * input.wind.x * phase_sine)
            + (0.75 * input.wind.y * phase_arc),
        (2.4 * input.wind.y * phase_sine)
            - (0.75 * input.wind.x * phase_arc),
        0.22 * (input.wind.x - input.wind.y) * phase_sine);
}

float TruthCloudVolumeCellularDistance(float3 sample_position)
{
    int3 base = int3(floor(sample_position));
    float minimum_squared = 1.0e20;
    [unroll]
    for (int offset_z = -1; offset_z <= 1; ++offset_z)
    {
        [unroll]
        for (int offset_y = -1; offset_y <= 1; ++offset_y)
        {
            [unroll]
            for (int offset_x = -1; offset_x <= 1; ++offset_x)
            {
                int3 lattice = base + int3(offset_x, offset_y, offset_z);
                float3 feature = float3(lattice)
                    + float3(
                        TruthSkyLatticeHash3D(lattice),
                        TruthSkyLatticeHash3D(lattice + int3(37, -17, 53)),
                        TruthSkyLatticeHash3D(lattice + int3(-29, 71, 11)));
                float3 delta = feature - sample_position;
                minimum_squared = min(minimum_squared, dot(delta, delta));
            }
        }
    }
    return sqrt(minimum_squared);
}

float TruthSampleCloudVolumeDensity(
    TruthCloudVolumeInput input,
    float3 position,
    bool detailed)
{
    if (input.cloud_coverage == 0.0
        || input.cloud_density == 0.0
        || position.z <= input.cloud_base_height
        || position.z >= input.cloud_top_height)
    {
        return 0.0;
    }

    float3 motion = TruthCloudVolumePhaseMotion(input);
    float3 advected = position + motion;
    float weather_low = TruthSkyValueNoise3D(float3(
        (0.045 * advected.x) + 12.7,
        (0.045 * advected.y) - 8.2,
        4.3));
    float weather_fold = TruthSkyValueNoise3D(float3(
        (0.095 * advected.x) - 5.1,
        (0.095 * advected.y) + 17.6,
        -9.4));
    float weather_field = (0.68 * weather_low) + (0.32 * weather_fold);
    float local_coverage = saturate(
        input.cloud_coverage
        + (0.50 * (weather_field - 0.5))
        + (0.12 * input.weather_density));
    float local_type = saturate(
        input.cloud_type + (0.24 * (weather_fold - 0.5)));

    float3 body_point = float3(
        (0.22 * advected.x) + 2.7,
        (0.22 * advected.y) - 6.1,
        (0.46 * advected.z) + 9.3);
    float body_broad = TruthSkyValueNoise3D(body_point);
    float body_middle = TruthSkyValueNoise3D(float3(
        (1.93 * body_point.x) - 11.2,
        (1.93 * body_point.y) + 7.8,
        (1.93 * body_point.z) - 3.5));
    float body_breakup = TruthSkyValueNoise3D(float3(
        (3.87 * body_point.x) + 5.6,
        (3.87 * body_point.y) - 13.1,
        (3.87 * body_point.z) + 8.4));
    float body = saturate(
        (0.52 * body_broad) + (0.31 * body_middle) + (0.17 * body_breakup));
    float threshold = lerp(0.76, 0.30, local_coverage);
    float occupied = TruthCloudVolumeSmoothStep(
        threshold - 0.14,
        threshold + 0.18,
        body);
    float normalized_height = (position.z - input.cloud_base_height)
        / (input.cloud_top_height - input.cloud_base_height);
    float vertical_profile = TruthCloudVolumeVerticalProfile(
        normalized_height,
        local_type);
    float base_support = occupied * vertical_profile;
    if (base_support <= 0.002)
    {
        return 0.0;
    }
    float weather_scale = 0.62 + (0.58 * input.weather_density);
    float coarse_density = saturate(
        base_support
        * input.cloud_density
        * weather_scale
        * 0.82);
    if (!detailed)
    {
        return coarse_density;
    }
    float3 camera_delta = position - input.camera_position;
    if (input.night_factor >= 0.75 && dot(camera_delta, camera_delta) > 400.0)
    {
        return coarse_density;
    }

    float detail_noise = TruthSkyValueNoise3D(float3(
        (1.35 * advected.x) - 4.8,
        (1.35 * advected.y) + 3.2,
        (1.75 * advected.z) + 7.7));
    float cellular_scale = lerp(1.15, 1.75, local_type);
    float cellular_distance = TruthCloudVolumeCellularDistance(float3(
        (cellular_scale * advected.x) + 6.3,
        (cellular_scale * advected.y) - 9.7,
        (1.25 * cellular_scale * advected.z) + 2.1));
    float cellular_support = 1.0 - TruthCloudVolumeSmoothStep(
        0.24,
        0.96,
        cellular_distance);
    float cellular_weight = 0.42 * lerp(1.0, 0.45, input.night_factor);
    float detail_support = ((1.0 - cellular_weight) * detail_noise)
        + (cellular_weight * cellular_support);
    detail_support = lerp(
        detail_support,
        0.70,
        0.42 * input.night_factor);
    float erosion = 0.28 * (1.0 - cellular_support)
        * lerp(1.0, 0.68, input.weather_density)
        * lerp(1.0, 0.62, input.night_factor);

    float sculpted = max(occupied - erosion, 0.0)
        * lerp(0.62, 1.0, detail_support);
    return saturate(
        sculpted
        * vertical_profile
        * input.cloud_density
        * weather_scale);
}

TruthCloudVolumeOutput TruthEvaluateCloudVolume(TruthCloudVolumeInput input)
{
    TruthCloudVolumeOutput output;
    output.radiance = 0.0;
    output.transmittance = 1.0;
    output.optical_depth = 0.0;
    output.primary_steps = 0u;
    output.light_samples = 0u;
    if (input.cloud_coverage == 0.0 || input.cloud_density == 0.0)
    {
        return output;
    }

    float2 intersection;
    if (!TruthIntersectCloudVolumeLayer(
            input.camera_position.z,
            input.view_direction.z,
            input.cloud_base_height,
            input.cloud_top_height,
            input.max_distance,
            intersection))
    {
        return output;
    }

    float path_length = intersection.y - intersection.x;
    float step_length = path_length / float(TruthCloudVolumePrimarySteps);
    float jitter = TruthCloudVolumeInterleavedJitter(
        input.pixel_coordinate,
        input.jitter_frame);
    float sample_distance = intersection.x + (jitter * step_length);
    float view_sun_cosine = clamp(dot(input.view_direction, input.sun_direction), -1.0, 1.0);
    float daylight = (1.0 - input.night_factor)
        * TruthCloudVolumeSmoothStep(-0.08, 0.18, input.sun_direction.z);
    float anisotropy = 0.65;
    float anisotropy_squared = anisotropy * anisotropy;
    float phase_denominator = max(
        1.0 + anisotropy_squared
            - (2.0 * anisotropy * view_sun_cosine),
        0.035);
    float forward_phase = clamp(
        (1.0 - anisotropy_squared)
            / (phase_denominator * sqrt(phase_denominator)),
        0.0,
        4.0);

    [loop]
    for (uint step_index = 0u;
         step_index < TruthCloudVolumePrimarySteps;
         ++step_index)
    {
        if (sample_distance >= intersection.y)
        {
            break;
        }
        ++output.primary_steps;
        float3 position = input.camera_position
            + (input.view_direction * sample_distance);
        float density = TruthSampleCloudVolumeDensity(input, position, true);
        if (density > 0.002)
        {
            float2 light_intersection;
            float shadow_optical_depth = 0.0;
            if (TruthIntersectCloudVolumeLayer(
                    position.z,
                    input.sun_direction.z,
                    input.cloud_base_height,
                    input.cloud_top_height,
                    12.0,
                    light_intersection))
            {
                float light_path = light_intersection.y - light_intersection.x;
                float light_step_length = light_path / float(TruthCloudVolumeLightSteps);
                [unroll]
                for (uint light_index = 0u;
                     light_index < TruthCloudVolumeLightSteps;
                     ++light_index)
                {
                    float light_distance = light_intersection.x
                        + ((float(light_index) + 0.5) * light_step_length);
                    float3 light_position = position
                        + (input.sun_direction * light_distance);
                    shadow_optical_depth += TruthSampleCloudVolumeDensity(
                        input,
                        light_position,
                        false)
                        * TruthCloudVolumeShadowExtinction
                        * light_step_length;
                    ++output.light_samples;
                }
            }
            float sun_transmittance = exp(-min(shadow_optical_depth, 16.0));
            float powder = 1.0 - exp(-2.4 * density);
            float silver_lining = TruthCloudVolumeSmoothStep(
                0.72,
                0.995,
                view_sun_cosine)
                * (1.0 - sun_transmittance)
                * powder
                * lerp(1.0, 0.12, input.weather_density);
            float multiple_scattering = 0.055
                + (0.18
                   * (1.0 - sun_transmittance)
                   * lerp(1.0, 0.50, input.weather_density))
                + (0.10 * powder);
            float ambient = 0.08
                + (0.12 * (1.0 - input.weather_density))
                + (0.025 * input.night_factor)
                + multiple_scattering;
            float core_darkening = 1.0
                - (0.52
                   * input.weather_density
                   * powder
                   * (0.55 + (0.45 * (1.0 - sun_transmittance))));
            float normalized_height = (position.z - input.cloud_base_height)
                / (input.cloud_top_height - input.cloud_base_height);
            float top_lighting = lerp(
                1.0,
                0.52 + (0.48 * TruthCloudVolumeSmoothStep(
                    0.08,
                    0.92,
                    normalized_height)),
                input.weather_density);
            float direct = daylight
                * ((sun_transmittance
                    * (0.16 + (0.24 * forward_phase))
                    * lerp(1.0, 0.45, input.weather_density))
                   + (0.55 * silver_lining));
            float3 weather_tint = lerp(
                float3(1.00, 0.98, 0.94),
                float3(0.50, 0.57, 0.68),
                input.weather_density);
            float3 tint = lerp(
                weather_tint,
                float3(0.12, 0.15, 0.20),
                input.night_factor);
            float step_optical_depth = density
                * TruthCloudVolumeExtinction
                * step_length;
            float segment_transmittance = exp(-step_optical_depth);
            float segment_opacity = 1.0 - segment_transmittance;
            float3 source = tint
                * ((ambient * core_darkening * top_lighting) + direct);
            output.radiance += source
                * output.transmittance
                * segment_opacity;
            output.transmittance *= segment_transmittance;
            output.optical_depth = min(
                output.optical_depth + step_optical_depth,
                TruthCloudVolumeMaximumOpticalDepth);
            if (output.transmittance <= TruthCloudVolumeMinimumTransmittance)
            {
                break;
            }
        }
        sample_distance += step_length;
    }
    return output;
}

#else

TruthCloudVolumeOutput TruthEvaluateCloudVolume(TruthCloudVolumeInput input)
{
    TruthCloudVolumeOutput output;
    output.radiance = 0.0;
    output.transmittance = 1.0;
    output.optical_depth = 0.0;
    output.primary_steps = 0u;
    output.light_samples = 0u;
    return output;
}

#endif

#endif
