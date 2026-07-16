#include "TruthColorCore.fxh"
#include "TruthAtmosphereCore.fxh"
#include "TruthSkyFields.fxh"
#include "TruthCloudLighting.fxh"
#include "TruthCloudVolume.fxh"

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
    float3 TruthReferenceCameraPosition;
    float TruthReferenceCloudType;
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

TruthSkyFieldOutput TruthReferenceEvaluateSkyField(float2 texcoord)
{
    float vertical = saturate(1.0 - texcoord.y);
    float elevation = lerp(0.035, (0.5 * TruthSkyPi) - 0.035, vertical);
    float view_z = sin(elevation);
    float view_radius = cos(elevation);
    float azimuth = ((texcoord.x * 2.0) - 1.0) * TruthSkyPi;
    float3 view_direction = normalize(float3(
        sin(azimuth) * view_radius,
        cos(azimuth) * view_radius,
        view_z));

    TruthSkyFieldInput field_input;
    field_input.view_direction = view_direction;
    field_input.phase = TruthReferencePhase;
    field_input.wind = float2(0.62, -0.27);
    field_input.cloud_coverage = TruthReferenceCloudCoverage;
    field_input.cloud_density = TruthReferenceCloudDensity;
    field_input.weather_density = TruthReferenceWeatherDensity;
    field_input.aurora_activity = TruthReferenceAuroraActivity;
    field_input.night_factor = TruthReferenceNightFactor;
    field_input.camera_position = TruthReferenceCameraPosition;
    return TruthEvaluateSkyFields(field_input);
}

TruthCloudVolumeOutput TruthReferenceEvaluateCloudVolume(
    TruthReferenceVertexOutput input)
{
    float vertical = saturate(1.0 - input.texcoord.y);
    float elevation = lerp(0.035, (0.5 * TruthSkyPi) - 0.035, vertical);
    float view_z = sin(elevation);
    float view_radius = cos(elevation);
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

    TruthCloudVolumeInput volume_input;
    volume_input.camera_position = TruthReferenceCameraPosition;
    volume_input.view_direction = view_direction;
    volume_input.sun_direction = sun_direction;
    volume_input.cloud_base_height = 1.20;
    volume_input.cloud_top_height = 3.80;
    volume_input.max_distance = 60.0;
    volume_input.phase = TruthReferencePhase;
    volume_input.wind = float2(0.62, -0.27);
    volume_input.cloud_coverage = TruthReferenceCloudCoverage;
    volume_input.cloud_density = TruthReferenceCloudDensity;
    volume_input.weather_density = TruthReferenceWeatherDensity;
    volume_input.cloud_type = TruthReferenceCloudType;
    volume_input.night_factor = TruthReferenceNightFactor;
    volume_input.pixel_coordinate = uint2(input.position.xy);
    volume_input.jitter_frame = 0u;
    return TruthEvaluateCloudVolume(volume_input);
}

float3 TruthReferenceStarRadiance(float3 view_direction, float night_factor)
{
    float star_visibility = TruthAuroraSmoothStep(0.55, 0.92, night_factor);
    float3 primary_position = view_direction * 64.0;
    int3 primary_cell = int3(floor(primary_position));
    float3 primary_local = frac(primary_position) - 0.5;
    float primary_hash = TruthSkyLatticeHash3D(primary_cell);
    float primary_core = TruthAuroraSmoothStep(0.30, 0.035, length(primary_local));
    float primary_gate = TruthAuroraSmoothStep(0.982, 0.998, primary_hash);

    float3 secondary_position = view_direction * 109.0;
    int3 secondary_cell = int3(floor(secondary_position));
    float3 secondary_local = frac(secondary_position) - 0.5;
    float secondary_hash = TruthSkyLatticeHash3D(secondary_cell + int3(17, -9, 23));
    float secondary_core = TruthAuroraSmoothStep(0.25, 0.025, length(secondary_local));
    float secondary_gate = TruthAuroraSmoothStep(0.991, 0.9995, secondary_hash);

    float primary = primary_core * primary_gate;
    float secondary = secondary_core * secondary_gate;
    float color_hash = TruthSkyLatticeHash3D(primary_cell + int3(-5, 31, 11));
    float3 star_color = lerp(
        float3(0.78, 0.86, 1.00),
        float3(1.00, 0.91, 0.76),
        color_hash);
    return star_visibility
        * ((0.105 * primary * star_color)
           + (0.060 * secondary * float3(0.76, 0.84, 1.00)));
}

float4 TruthSkyFieldScalarProbePixelMain(
    TruthReferenceVertexOutput input) : SV_Target0
{
    TruthSkyFieldOutput field_output = TruthReferenceEvaluateSkyField(input.texcoord);
    return float4(
        field_output.cloud_body,
        field_output.cloud_detail_erosion,
        field_output.cloud_density,
        field_output.aurora_mask);
}

float4 TruthSkyFieldRadianceProbePixelMain(
    TruthReferenceVertexOutput input) : SV_Target0
{
    TruthSkyFieldOutput field_output = TruthReferenceEvaluateSkyField(input.texcoord);
    return float4(field_output.aurora_intrinsic_radiance, 1.0);
}

float4 TruthCloudVolumeScalarProbePixelMain(
    TruthReferenceVertexOutput input) : SV_Target0
{
    TruthCloudVolumeOutput volume = TruthReferenceEvaluateCloudVolume(input);
    return float4(
        volume.transmittance,
        saturate(volume.optical_depth / 8.0),
        float(volume.primary_steps) / 24.0,
        float(volume.light_samples) / 144.0);
}

float4 TruthCloudVolumeRadianceProbePixelMain(
    TruthReferenceVertexOutput input) : SV_Target0
{
    TruthCloudVolumeOutput volume = TruthReferenceEvaluateCloudVolume(input);
    return float4(saturate(volume.radiance), 1.0);
}

float4 TruthReferencePixelMain(TruthReferenceVertexOutput input) : SV_Target0
{
    float vertical = saturate(1.0 - input.texcoord.y);
    float elevation = lerp(0.035, (0.5 * TruthSkyPi) - 0.035, vertical);
    float view_z = sin(elevation);
    float view_radius = cos(elevation);
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
    field_input.camera_position = TruthReferenceCameraPosition;
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

    float3 composite_radiance;
    float background_transmittance;
#if TRUTH_ENABLE_CLOUD_VOLUME
    TruthCloudVolumeOutput volume_output = TruthReferenceEvaluateCloudVolume(input);
    float combined_transmittance = volume_output.transmittance
        * atmosphere_output.fog_transmittance;
    composite_radiance = (atmosphere_output.sky_radiance * combined_transmittance)
        + (volume_output.radiance * atmosphere_output.fog_transmittance)
        + (field_output.aurora_intrinsic_radiance * combined_transmittance);
    background_transmittance = combined_transmittance;
#else
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
    composite_radiance = lighting_output.composite_radiance;
    background_transmittance = lighting_output.cloud_transmittance
        * atmosphere_output.fog_transmittance;
#endif

    float aurora_luminance = dot(
        field_output.aurora_intrinsic_radiance,
        float3(0.2126, 0.7152, 0.0722));
    float aurora_star_preservation = 1.0
        - (0.72 * TruthAuroraSmoothStep(0.025, 0.15, aurora_luminance));
    composite_radiance += TruthReferenceStarRadiance(
        view_direction,
        TruthReferenceNightFactor)
        * background_transmittance
        * aurora_star_preservation;

    float3 exposed = TruthApplyExposure(
        composite_radiance,
        TruthReferenceExposureEv);
    return float4(saturate(TruthFilmicToneCurve3(exposed)), 1.0);
}
