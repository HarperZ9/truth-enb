#include "truth/TruthColorCore.fxh"
#include "truth/TruthAtmosphereCore.fxh"
#include "truth/TruthSkyFields.fxh"
#include "truth/TruthCloudLighting.fxh"

#ifndef TRUTH_ENABLE_ATMOSPHERE
#define TRUTH_ENABLE_ATMOSPHERE 1
#endif

#ifndef TRUTH_ENABLE_PROCEDURAL_SKY
#define TRUTH_ENABLE_PROCEDURAL_SKY 1
#endif

Texture2D TruthSceneTexture : register(t0);

SamplerState TruthSceneSampler : register(s0)
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Clamp;
    AddressV = Clamp;
};

cbuffer TruthFrameParameters : register(b0)
{
    float TruthSceneLuminance;
    float TruthSkyLuminance;
    float TruthInteriorFactor;
    float TruthDeltaSeconds;
    float TruthExposureEv;
    float TruthUseTargetExposure;
    float2 TruthFramePadding;
};

cbuffer TruthAtmosphereParameters : register(b1)
{
    float TruthViewZenithCosine;
    float TruthViewSunCosine;
    float TruthSunElevation;
    float TruthWeatherDensity;
    float TruthCloudCoverage;
    float TruthCloudDensity;
    float TruthFogDensity;
    float TruthAuroraActivity;
    float TruthAuroraMask;
    float TruthNightFactor;
    float2 TruthAtmospherePadding;
};

cbuffer TruthSkyFieldParameters : register(b2)
{
    float TruthSkyPhase;
    float2 TruthSkyWind;
    float TruthSkyProjectionScale;
    float3 TruthSkyCameraPosition;
    float TruthSkyFieldPadding;
};

struct TruthVertexInput
{
    float3 position : POSITION;
    float2 texcoord : TEXCOORD0;
};

struct TruthVertexOutput
{
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
};

TruthVertexOutput TruthVertexMain(TruthVertexInput input)
{
    TruthVertexOutput output;
    output.position = float4(input.position, 1.0);
    output.texcoord = input.texcoord;
    return output;
}

float4 TruthPixelMain(TruthVertexOutput input) : SV_Target0
{
    TruthAtmosphereSample atmosphere;
    atmosphere.scene_luminance = TruthSceneLuminance;
    atmosphere.sky_luminance = TruthSkyLuminance;
    atmosphere.interior_factor = TruthInteriorFactor;
    atmosphere.delta_seconds = TruthDeltaSeconds;
    atmosphere.discontinuity = 0.0;

    float target_exposure_ev = TruthTargetExposureEv(atmosphere);
    float selected_exposure_ev = lerp(TruthExposureEv,
                                      target_exposure_ev,
                                      saturate(TruthUseTargetExposure));
    float4 scene_color = TruthSceneTexture.Sample(TruthSceneSampler, input.texcoord);
    float3 linear_color = scene_color.rgb;

#if TRUTH_ENABLE_ATMOSPHERE
    TruthUnifiedAtmosphereInput atmosphere_input;
    atmosphere_input.view_zenith_cosine = clamp(TruthViewZenithCosine, -1.0, 1.0);
    atmosphere_input.view_sun_cosine = clamp(TruthViewSunCosine, -1.0, 1.0);
    atmosphere_input.sun_elevation = clamp(TruthSunElevation, -1.0, 1.0);
    atmosphere_input.weather_density = saturate(TruthWeatherDensity);
    atmosphere_input.cloud_coverage = 0.0;
#if TRUTH_ENABLE_PROCEDURAL_SKY
    float2 projected_view = ((input.texcoord * 2.0) - 1.0)
        * max(abs(TruthSkyProjectionScale), 0.001);
    float view_vertical = TruthViewZenithCosine < 0.0
        ? min(TruthViewZenithCosine, -0.05)
        : max(TruthViewZenithCosine, 0.05);
    TruthSkyFieldInput sky_field_input;
    sky_field_input.view_direction = normalize(float3(projected_view, view_vertical));
    sky_field_input.phase = saturate(TruthSkyPhase);
    sky_field_input.wind = clamp(TruthSkyWind, -1.0, 1.0);
    sky_field_input.cloud_coverage = saturate(TruthCloudCoverage);
    sky_field_input.cloud_density = saturate(TruthCloudDensity);
    sky_field_input.weather_density = saturate(TruthWeatherDensity);
    sky_field_input.aurora_activity = saturate(TruthAuroraActivity);
    sky_field_input.night_factor = saturate(TruthNightFactor);
    sky_field_input.camera_position = TruthSkyCameraPosition;
    TruthSkyFieldOutput sky_field_output = TruthEvaluateSkyFields(sky_field_input);
    float resolved_cloud_density = sky_field_output.cloud_density;
    float resolved_cloud_detail_erosion = sky_field_output.cloud_detail_erosion;
    float3 resolved_aurora_intrinsic_radiance =
        sky_field_output.aurora_intrinsic_radiance;
#else
    float resolved_cloud_density = saturate(TruthCloudCoverage)
        * saturate(TruthCloudDensity);
    float resolved_cloud_detail_erosion = 0.35;
    float direct_aurora_view = 0.35
        + (0.65 * saturate(TruthViewZenithCosine));
    float direct_aurora_strength = saturate(TruthAuroraActivity)
        * saturate(TruthAuroraMask)
        * saturate(TruthNightFactor)
        * direct_aurora_view;
    float3 resolved_aurora_intrinsic_radiance = float3(0.10, 0.80, 0.55)
        * direct_aurora_strength;
#endif
    atmosphere_input.cloud_density = 0.0;
    atmosphere_input.fog_density = saturate(TruthFogDensity);
    atmosphere_input.aurora_activity = 0.0;
    atmosphere_input.aurora_mask = 0.0;
    atmosphere_input.night_factor = saturate(TruthNightFactor);
    TruthUnifiedAtmosphereOutput atmosphere_output = TruthEvaluateAtmosphere(atmosphere_input);

    TruthCloudLightingInput cloud_lighting_input;
    cloud_lighting_input.sky_radiance = atmosphere_output.sky_radiance;
    cloud_lighting_input.aurora_intrinsic_radiance =
        resolved_aurora_intrinsic_radiance;
    cloud_lighting_input.view_zenith_cosine = atmosphere_input.view_zenith_cosine;
    cloud_lighting_input.view_sun_cosine = atmosphere_input.view_sun_cosine;
    cloud_lighting_input.sun_elevation = atmosphere_input.sun_elevation;
    cloud_lighting_input.cloud_density = resolved_cloud_density;
    cloud_lighting_input.cloud_detail_erosion = resolved_cloud_detail_erosion;
    cloud_lighting_input.weather_density = atmosphere_input.weather_density;
    cloud_lighting_input.night_factor = atmosphere_input.night_factor;
    cloud_lighting_input.fog_transmittance = atmosphere_output.fog_transmittance;
    TruthCloudLightingOutput cloud_lighting_output =
        TruthEvaluateCloudLighting(cloud_lighting_input);
    linear_color += cloud_lighting_output.composite_radiance;
#endif

    float3 exposed_color = TruthApplyExposure(linear_color, selected_exposure_ev);
    return float4(TruthFilmicToneCurve3(exposed_color), scene_color.a);
}

technique11 TruthMasterLookTechnique
{
    pass TruthMasterLookPass
    {
        SetVertexShader(CompileShader(vs_5_0, TruthVertexMain()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, TruthPixelMain()));
    }
}
