#include "TruthColorCore.fxh"
#include "TruthAtmosphereCore.fxh"
#include "TruthSkyFields.fxh"
#include "TruthCloudLighting.fxh"

cbuffer TruthReferenceSceneParameters : register(b0)
{
    float TruthReferenceSunElevation;
    float TruthReferenceWeatherDensity;
    float TruthReferenceCloudCoverage;
    float TruthReferenceCloudDensity;
    float TruthReferenceFogDensity;
    float TruthReferenceAuroraActivity;
    float TruthReferenceNightFactor;
    float TruthReferencePhase;
    float TruthReferenceExposureEv;
    float TruthReferenceSunAzimuth;
    float2 TruthReferencePadding;
};

struct TruthReferenceVertexOutput
{
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
};

TruthReferenceVertexOutput TruthReferenceVertexMain(uint vertex_id : SV_VertexID)
{
    float2 texcoord = float2((vertex_id << 1u) & 2u, vertex_id & 2u);
    TruthReferenceVertexOutput output;
    output.position = float4(
        (texcoord * float2(2.0, -2.0)) + float2(-1.0, 1.0),
        0.0,
        1.0);
    output.texcoord = texcoord;
    return output;
}

float4 TruthReferencePixelMain(TruthReferenceVertexOutput input) : SV_Target0
{
    float vertical = saturate(1.0 - input.texcoord.y);
    float view_z = lerp(0.035, 0.999, vertical);
    float view_radius = sqrt(max(1.0 - (view_z * view_z), 0.0));
    float azimuth = ((input.texcoord.x * 2.0) - 1.0) * TruthSkyPi;
    float3 view_direction = normalize(float3(
        sin(azimuth) * view_radius,
        cos(azimuth) * view_radius,
        view_z));
    float sun_radius = cos(TruthReferenceSunElevation);
    float3 sun_direction = normalize(float3(
        sin(TruthReferenceSunAzimuth) * sun_radius,
        cos(TruthReferenceSunAzimuth) * sun_radius,
        sin(TruthReferenceSunElevation)));

    TruthSkyFieldInput field_input;
    field_input.view_direction = view_direction;
    field_input.phase = TruthReferencePhase;
    field_input.wind = float2(0.62, -0.27);
    field_input.cloud_coverage = TruthReferenceCloudCoverage;
    field_input.cloud_density = TruthReferenceCloudDensity;
    field_input.weather_density = TruthReferenceWeatherDensity;
    field_input.aurora_activity = TruthReferenceAuroraActivity;
    field_input.night_factor = TruthReferenceNightFactor;
    TruthSkyFieldOutput field_output = TruthEvaluateSkyFields(field_input);

    TruthUnifiedAtmosphereInput atmosphere_input;
    atmosphere_input.view_zenith_cosine = view_z;
    atmosphere_input.view_sun_cosine = dot(view_direction, sun_direction);
    atmosphere_input.sun_elevation = TruthReferenceSunElevation;
    atmosphere_input.weather_density = TruthReferenceWeatherDensity;
    atmosphere_input.cloud_coverage = 0.0;
    atmosphere_input.cloud_density = 0.0;
    atmosphere_input.fog_density = TruthReferenceFogDensity;
    atmosphere_input.aurora_activity = 0.0;
    atmosphere_input.aurora_mask = 0.0;
    atmosphere_input.night_factor = TruthReferenceNightFactor;
    TruthUnifiedAtmosphereOutput atmosphere_output =
        TruthEvaluateAtmosphere(atmosphere_input);

    TruthCloudLightingInput lighting_input;
    lighting_input.sky_radiance = atmosphere_output.sky_radiance;
    lighting_input.aurora_intrinsic_radiance = field_output.aurora_intrinsic_radiance;
    lighting_input.view_zenith_cosine = view_z;
    lighting_input.view_sun_cosine = atmosphere_input.view_sun_cosine;
    lighting_input.sun_elevation = TruthReferenceSunElevation;
    lighting_input.cloud_density = field_output.cloud_density;
    lighting_input.cloud_detail_erosion = field_output.cloud_detail_erosion;
    lighting_input.weather_density = TruthReferenceWeatherDensity;
    lighting_input.night_factor = TruthReferenceNightFactor;
    lighting_input.fog_transmittance = atmosphere_output.fog_transmittance;
    TruthCloudLightingOutput lighting_output = TruthEvaluateCloudLighting(lighting_input);

    float3 exposed = TruthApplyExposure(
        lighting_output.composite_radiance,
        TruthReferenceExposureEv);
    return float4(saturate(TruthFilmicToneCurve3(exposed)), 1.0);
}
