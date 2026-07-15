#ifndef TRUTH_CLOUD_LIGHTING_FXH
#define TRUTH_CLOUD_LIGHTING_FXH

static const float TruthCloudHorizonCosineFloor = 0.10;
static const float TruthCloudDaylightStart = -0.08;
static const float TruthCloudDaylightEnd = 0.12;
static const float TruthCloudSilverLiningStart = 0.72;
static const float TruthCloudSilverLiningEnd = 0.98;
static const float TruthCloudMaximumOpticalDepth = 42.0;
static const float TruthCloudMaximumForwardScattering = 6.0;
static const float TruthCloudMaximumDirectScattering = 6.0;
static const float TruthCloudMaximumRadiance = 6.0;
static const float TruthCloudOpticalDepthScale = 3.2;
static const float TruthCloudForwardAnisotropy = 0.72;
static const float TruthCloudForwardDenominatorFloor = 0.05;

struct TruthCloudLightingInput
{
    float3 sky_radiance;
    float3 aurora_intrinsic_radiance;
    float view_zenith_cosine;
    float view_sun_cosine;
    float sun_elevation;
    float cloud_density;
    float cloud_detail_erosion;
    float weather_density;
    float night_factor;
    float fog_transmittance;
};

struct TruthCloudLightingOutput
{
    float cloud_optical_depth;
    float cloud_transmittance;
    float forward_scattering;
    float silver_lining;
    float direct_scattering;
    float ambient_scattering;
    float multiple_scattering;
    float self_shadow;
    float powder_response;
    float3 cloud_tint;
    float3 cloud_radiance;
    float3 aurora_radiance;
    float3 composite_radiance;
};

float TruthCloudSmooth(float value)
{
    return value * value * (3.0 - (2.0 * value));
}

float TruthCloudSmoothStep(float lower, float upper, float value)
{
    return TruthCloudSmooth(saturate((value - lower) / (upper - lower)));
}

TruthCloudLightingOutput TruthEvaluateCloudLighting(TruthCloudLightingInput input)
{
    float path_cosine = max(input.view_zenith_cosine, TruthCloudHorizonCosineFloor);
    float air_mass = 1.0 / path_cosine;
    float weather_extinction = lerp(0.70, 1.30, input.weather_density);

    TruthCloudLightingOutput output;
    if (input.cloud_density == 0.0)
    {
        output.cloud_optical_depth = 0.0;
        output.cloud_transmittance = 1.0;
    }
    else
    {
        output.cloud_optical_depth = min(
            input.cloud_density
                * TruthCloudOpticalDepthScale
                * air_mass
                * weather_extinction,
            TruthCloudMaximumOpticalDepth);
        output.cloud_transmittance = exp(-output.cloud_optical_depth);
    }

    float daylight = TruthCloudSmoothStep(
        TruthCloudDaylightStart,
        TruthCloudDaylightEnd,
        input.sun_elevation);
    float anisotropy_squared = TruthCloudForwardAnisotropy * TruthCloudForwardAnisotropy;
    float forward_denominator = max(
        1.0 + anisotropy_squared
            - (2.0 * TruthCloudForwardAnisotropy * input.view_sun_cosine),
        TruthCloudForwardDenominatorFloor);
    output.forward_scattering = min(
        0.25 * (1.0 - anisotropy_squared)
            / (forward_denominator * sqrt(forward_denominator)),
        TruthCloudMaximumForwardScattering);

    float sun_edge = TruthCloudSmoothStep(
        TruthCloudSilverLiningStart,
        TruthCloudSilverLiningEnd,
        input.view_sun_cosine);
    float detail_edge = lerp(0.35, 1.0, input.cloud_detail_erosion);
    float density_edge = 1.0 - (0.45 * input.cloud_density);
    output.silver_lining = saturate(sun_edge * detail_edge * density_edge);
    output.direct_scattering = clamp(
        daylight * ((0.32 * output.forward_scattering)
                    + (1.35 * output.silver_lining)),
        0.0,
        TruthCloudMaximumDirectScattering);

    output.self_shadow = exp(-0.62 * output.cloud_optical_depth);
    output.powder_response = 1.0 - exp(
        -2.4 * input.cloud_density * lerp(0.85, 1.15, input.weather_density));
    output.multiple_scattering = saturate(
        0.055 + (0.22 * output.powder_response
                 * (1.0 - (0.35 * input.weather_density))));
    float ambient_base = lerp(
        0.07 + (0.05 * input.night_factor),
        0.22,
        daylight);
    output.ambient_scattering = saturate(ambient_base + output.multiple_scattering);

    float3 night_tint = float3(0.18, 0.28, 0.52);
    float3 day_tint = float3(1.00, 0.97, 0.92);
    float3 base_tint = lerp(night_tint, day_tint, daylight);
    output.cloud_tint = base_tint * lerp(
        float3(1.0, 1.0, 1.0),
        float3(0.62, 0.72, 0.88),
        input.weather_density);

    float cloud_opacity = 1.0 - output.cloud_transmittance;
    float direct_visibility = output.self_shadow
        * lerp(0.55, 1.0, output.powder_response);
    float lighting = min(
        output.ambient_scattering + (output.direct_scattering * direct_visibility),
        TruthCloudMaximumRadiance);
    output.cloud_radiance = output.cloud_tint * cloud_opacity * lighting;

    float combined_transmittance = output.cloud_transmittance * input.fog_transmittance;
    float aurora_transmittance = (input.night_factor == 0.0 || daylight >= 1.0)
        ? 0.0
        : combined_transmittance;
    output.aurora_radiance = input.aurora_intrinsic_radiance
        * aurora_transmittance;
    output.composite_radiance = (input.sky_radiance * combined_transmittance)
        + (output.cloud_radiance * input.fog_transmittance)
        + output.aurora_radiance;
    return output;
}

#endif
