#ifndef TRUTH_ATMOSPHERE_CORE_FXH
#define TRUTH_ATMOSPHERE_CORE_FXH

static const float TruthAtmosphereRayleighScale = 0.75;
static const float TruthAtmosphereMieAnisotropy = 0.65;
static const float TruthAtmosphereMieDenominatorFloor = 0.05;
static const float TruthAtmosphereMiePhaseMaximum = 12.0;
static const float TruthAtmosphereHorizonCosineFloor = 0.05;
static const float TruthAtmosphereCloudOpticalDepthScale = 4.0;
static const float TruthAtmosphereFogOpticalDepthScale = 3.0;

struct TruthUnifiedAtmosphereInput
{
    float view_zenith_cosine;
    float view_sun_cosine;
    float sun_elevation;
    float weather_density;
    float cloud_coverage;
    float cloud_density;
    float fog_density;
    float aurora_activity;
    float aurora_mask;
    float night_factor;
};

struct TruthUnifiedAtmosphereOutput
{
    float3 sky_radiance;
    float cloud_transmittance;
    float fog_transmittance;
    float3 aurora_radiance;
    float3 composite_radiance;
};

float TruthAtmosphereRayleighPhase(float view_sun_cosine)
{
    float cosine = clamp(view_sun_cosine, -1.0, 1.0);
    return TruthAtmosphereRayleighScale * (1.0 + (cosine * cosine));
}

float TruthAtmosphereMiePhase(float view_sun_cosine)
{
    float cosine = clamp(view_sun_cosine, -1.0, 1.0);
    float g_squared = TruthAtmosphereMieAnisotropy * TruthAtmosphereMieAnisotropy;
    float denominator = max(1.0 + g_squared
                            - (2.0 * TruthAtmosphereMieAnisotropy * cosine),
                            TruthAtmosphereMieDenominatorFloor);
    float phase = (1.0 - g_squared) / (denominator * sqrt(denominator));
    return min(phase, TruthAtmosphereMiePhaseMaximum);
}

TruthUnifiedAtmosphereOutput TruthEvaluateAtmosphere(TruthUnifiedAtmosphereInput input)
{
    float rayleigh_phase = TruthAtmosphereRayleighPhase(input.view_sun_cosine);
    float mie_phase = TruthAtmosphereMiePhase(input.view_sun_cosine);
    float path_cosine = max(input.view_zenith_cosine, TruthAtmosphereHorizonCosineFloor);
    float air_mass = 1.0 / path_cosine;
    float horizon_boost = 1.0 + (0.04 * (air_mass - 1.0));
    float daylight = saturate((input.sun_elevation + 0.1) / 0.2);
    float weather_attenuation = 1.0 - (0.55 * input.weather_density);

    TruthUnifiedAtmosphereOutput output;
    output.sky_radiance = float3(
        ((daylight * ((0.18 * rayleigh_phase) + (0.035 * mie_phase)) * horizon_boost)
         + (0.00225 * input.night_factor)) * weather_attenuation,
        ((daylight * ((0.28 * rayleigh_phase) + (0.025 * mie_phase)) * horizon_boost)
         + (0.00525 * input.night_factor)) * weather_attenuation,
        ((daylight * ((0.52 * rayleigh_phase) + (0.015 * mie_phase)) * horizon_boost)
         + (0.01350 * input.night_factor)) * weather_attenuation);

    float cloud_optical_depth = TruthAtmosphereCloudOpticalDepthScale
        * input.cloud_coverage
        * input.cloud_density
        * (0.35 + (0.65 * input.weather_density));
    output.cloud_transmittance = exp(-cloud_optical_depth);

    float fog_path = 1.0 + (0.15 * (air_mass - 1.0));
    float fog_optical_depth = TruthAtmosphereFogOpticalDepthScale * input.fog_density * fog_path;
    output.fog_transmittance = exp(-fog_optical_depth);

    float aurora_view = 0.35 + (0.65 * saturate(input.view_zenith_cosine));
    float aurora_strength = input.aurora_activity
        * input.aurora_mask
        * input.night_factor
        * aurora_view;
    float3 intrinsic_aurora = float3(0.10, 0.80, 0.55) * aurora_strength;

    float attenuation = output.cloud_transmittance * output.fog_transmittance;
    output.aurora_radiance = intrinsic_aurora * attenuation;
    output.composite_radiance = (output.sky_radiance * attenuation) + output.aurora_radiance;
    return output;
}

#endif
