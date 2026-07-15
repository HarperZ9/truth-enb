#include "truth/TruthColorCore.fxh"
#include "truth/TruthAtmosphereCore.fxh"

#ifndef TRUTH_ENABLE_ATMOSPHERE
#define TRUTH_ENABLE_ATMOSPHERE 1
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
    atmosphere_input.cloud_coverage = saturate(TruthCloudCoverage);
    atmosphere_input.cloud_density = saturate(TruthCloudDensity);
    atmosphere_input.fog_density = saturate(TruthFogDensity);
    atmosphere_input.aurora_activity = saturate(TruthAuroraActivity);
    atmosphere_input.aurora_mask = saturate(TruthAuroraMask);
    atmosphere_input.night_factor = saturate(TruthNightFactor);
    TruthUnifiedAtmosphereOutput atmosphere_output = TruthEvaluateAtmosphere(atmosphere_input);
    linear_color += atmosphere_output.composite_radiance;
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
