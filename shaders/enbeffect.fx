#define TRUTH_STAGE_CAPABILITY TRUTH_CAPABILITY_NATIVE
#define TRUTH_STAGE_OWNS_COLOR 1
#define TRUTH_STAGE_OWNS_DEPTH 1
#define TRUTH_STAGE_OWNS_NORMAL 0
#define TRUTH_STAGE_OWNS_MASK 0
#define TRUTH_STAGE_OWNS_NATIVE_CELESTIAL_VIEW 1
#define TRUTH_STAGE_OWNS_PREVIOUS_SCALAR_ADAPTATION 1
#define TRUTH_STAGE_OWNS_BRIDGE_VALUE 0
#define TRUTH_STAGE_SCRATCH_OWNER TRUTH_SCRATCH_MAIN
#define TRUTH_STAGE_SCRATCH_READ TRUTH_SCRATCH_NONE
#define TRUTH_STAGE_OWNS_FULL_FRAME_HISTORY 0
#define TRUTH_STAGE_OWNS_OBJECT_MOTION 0
#define TRUTH_STAGE_TREATS_SCRATCH_AS_HISTORY 0
#define TRUTH_STAGE_CROSS_EFFECT_ALPHA_PACKING 0
#define TRUTH_STAGE_NATIVE_CAPABILITY_AVAILABLE 1
#define TRUTH_STAGE_BRIDGE_CAPABILITY_AVAILABLE 0
#define TRUTH_STAGE_SPATIAL_CAPABILITY_AVAILABLE 0

#include "truth/TruthColorCore.fxh"
#include "truth/TruthAtmosphereCore.fxh"
#include "truth/TruthSkyFields.fxh"
#include "truth/TruthCloudLighting.fxh"
#include "truth/TruthSkyViewAdapter.fxh"
#include "truth/TruthRuntimeParameters.fxh"
#include "truth/TruthEffectParameters.fxh"
#include "truth/TruthHostCapabilities.fxh"
#include "truth/TruthPipelineCommon.fxh"

#ifndef TRUTH_ENABLE_ATMOSPHERE
#define TRUTH_ENABLE_ATMOSPHERE 1
#endif

#ifndef TRUTH_ENABLE_PROCEDURAL_SKY
#define TRUTH_ENABLE_PROCEDURAL_SKY 1
#endif

// ENBSeries 0.504 external interface. Names in this block are host-owned.
float4 Timer;
float4 ScreenSize;
float AdaptiveQuality;
float4 Weather;
float4 TimeOfDay1;
float4 TimeOfDay2;
float ENightDayFactor;
float EInteriorFactor;
float4 BloomSize;
float4 Params01[7];
float4 ENBParams01;

Texture2D TextureColor;
Texture2D TextureBloom;
Texture2D TextureLens;
Texture2D TextureDepth;
Texture2D TextureAdaptation;

SamplerState Sampler0
{
    Filter = MIN_MAG_MIP_POINT;
    AddressU = Clamp;
    AddressV = Clamp;
};

SamplerState Sampler1
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Clamp;
    AddressV = Clamp;
};

#pragma warning(push)
#pragma warning(disable: 3206)
#include "enb/ENBSeries0504VanillaPostProcess.fxh"
#pragma warning(pop)

float3 TruthResolveProceduralSunDirection(out float sun_elevation)
{
    static const float TruthPi = 3.14159265358979323846;
    float game_hour = frac(max(Weather.w, 0.0) / 24.0) * 24.0;
    float solar_phase = (game_hour - 6.0) * (TruthPi / 12.0);
    sun_elevation = sin(solar_phase);
    float azimuth = (game_hour * (TruthPi / 12.0)) + (0.15 * TruthPi);
    float horizon_radius = sqrt(saturate(1.0 - (sun_elevation * sun_elevation)));
    return normalize(float3(
        cos(azimuth) * horizon_radius,
        sin(azimuth) * horizon_radius,
        sun_elevation));
}

float3 TruthResolveSkyRadiance(float2 texcoord, out float valid)
{
    valid = 0.0;
    TruthSkyViewAdapterInput sky_view_input;
    sky_view_input.texcoord = texcoord;
    sky_view_input.inverse_view_projection =
        TruthRuntimeBuildInverseViewProjection();
    sky_view_input.camera_world_position = TruthRuntimeCameraWorld.xyz;
    sky_view_input.aurora_world_origin = TruthAuroraWorldOrigin;
    sky_view_input.engine_world_units_per_aurora_unit = TruthRuntimeStatus.w;
    TruthSkyViewAdapterOutput sky_view =
        TruthEvaluateSkyViewAdapter(sky_view_input);
    if (sky_view.valid <= 0.5)
    {
        return float3(0.0, 0.0, 0.0);
    }

    float night_factor = saturate(1.0 - ENightDayFactor);
    float sun_elevation;
    float3 sun_direction = TruthResolveProceduralSunDirection(sun_elevation);

    TruthSkyFieldInput sky_field_input;
    sky_field_input.view_direction = sky_view.view_world_direction;
    sky_field_input.phase = frac(max(Timer.x, 0.0));
    sky_field_input.wind = clamp(
        float2(TruthSkyWindX, TruthSkyWindY), -1.0, 1.0);
    sky_field_input.cloud_coverage = saturate(TruthCloudCoverage);
    sky_field_input.cloud_density = saturate(TruthCloudDensity);
    sky_field_input.weather_density = saturate(TruthWeatherDensity);
    sky_field_input.aurora_activity = saturate(TruthAuroraActivity)
        * saturate(TruthAuroraMask);
    sky_field_input.night_factor = night_factor;
    sky_field_input.camera_position = sky_view.camera_aurora_position;
    TruthSkyFieldOutput sky_field = TruthEvaluateSkyFields(sky_field_input);

    TruthUnifiedAtmosphereInput atmosphere_input;
    atmosphere_input.view_zenith_cosine = clamp(
        sky_view.view_world_direction.z, -1.0, 1.0);
    atmosphere_input.view_sun_cosine = clamp(
        dot(sky_view.view_world_direction, sun_direction), -1.0, 1.0);
    atmosphere_input.sun_elevation = sun_elevation;
    atmosphere_input.weather_density = saturate(TruthWeatherDensity);
    atmosphere_input.cloud_coverage = 0.0;
    atmosphere_input.cloud_density = 0.0;
    atmosphere_input.fog_density = saturate(TruthFogDensity);
    atmosphere_input.aurora_activity = 0.0;
    atmosphere_input.aurora_mask = 0.0;
    atmosphere_input.night_factor = night_factor;
    TruthUnifiedAtmosphereOutput atmosphere =
        TruthEvaluateAtmosphere(atmosphere_input);

    TruthCloudLightingInput cloud_input;
    cloud_input.sky_radiance = atmosphere.sky_radiance;
    cloud_input.aurora_intrinsic_radiance =
        sky_field.aurora_intrinsic_radiance;
    cloud_input.view_zenith_cosine = atmosphere_input.view_zenith_cosine;
    cloud_input.view_sun_cosine = atmosphere_input.view_sun_cosine;
    cloud_input.sun_elevation = atmosphere_input.sun_elevation;
    cloud_input.cloud_density = sky_field.cloud_density;
    cloud_input.cloud_detail_erosion = sky_field.cloud_detail_erosion;
    cloud_input.weather_density = atmosphere_input.weather_density;
    cloud_input.night_factor = night_factor;
    cloud_input.fog_transmittance = atmosphere.fog_transmittance;
    TruthCloudLightingOutput cloud = TruthEvaluateCloudLighting(cloud_input);
    valid = 1.0;
    return max(cloud.composite_radiance, 0.0) * max(TruthSkyRadianceScale, 0.0);
}

float3 TruthResolveEnbOpticalInput(float2 texcoord)
{
    float3 color = max(TextureColor.Sample(Sampler0, texcoord).rgb, 0.0);
    if (TruthUseEnbLens)
    {
        color += max(TextureLens.Sample(Sampler1, texcoord).rgb, 0.0)
            * max(ENBParams01.y, 0.0);
    }
    if (TruthUseEnbBloom)
    {
        float3 bloom = max(TextureBloom.Sample(Sampler1, texcoord).rgb, 0.0);
        color += max(bloom - color, 0.0) * max(ENBParams01.x, 0.0);
    }
    return color;
}

float3 TruthResolveMainCapability(float3 color)
{
    float native_available = TruthRuntimeReady() ? 1.0 : 0.0;
    return TruthResolveCapabilityColor(
        float4(color, 1.0), native_available, 0.0, 0.0).rgb;
}

float4 TruthEnbPixelMain(VS_OUTPUT_POST input) : SV_Target
{
    float3 linear_color = TruthResolveEnbOpticalInput(input.txcoord0);
    linear_color = TruthResolveMainCapability(linear_color);
    if (!TruthMasterEnabled)
    {
        return float4(saturate(linear_color), 1.0);
    }

#if TRUTH_ENABLE_ATMOSPHERE && TRUTH_ENABLE_PROCEDURAL_SKY
    if (TruthProceduralSkyEnabled && TruthRuntimeReady()
        && EInteriorFactor < 0.5)
    {
        float raw_depth = TextureDepth.SampleLevel(Sampler0, input.txcoord0, 0.0).x;
        float sky_mask = TruthSkyMask(
                raw_depth, TruthSkyDepthThreshold, TruthSkyDepthFeather)
            * saturate(1.0 - EInteriorFactor);
        if (sky_mask > 0.0)
        {
            float sky_valid;
            float3 sky_radiance = TruthResolveSkyRadiance(
                input.txcoord0, sky_valid);
            linear_color = lerp(
                linear_color,
                sky_radiance,
                sky_mask * sky_valid * saturate(TruthSkyReplacementStrength));
        }
    }
#endif

    float measured_luminance = max(
        TextureAdaptation.SampleLevel(Sampler0, input.txcoord0, 0.0).x,
        TruthLuminanceFloor);
    TruthAtmosphereSample metering;
    metering.scene_luminance = measured_luminance;
    metering.sky_luminance = measured_luminance;
    metering.interior_factor = saturate(EInteriorFactor);
    metering.delta_seconds = max(Timer.w, 0.0);
    metering.discontinuity = 0.0;
    float target_exposure_ev = TruthTargetExposureEv(metering);
    float exposure_ev = lerp(
        clamp(TruthManualExposureEv, -8.0, 8.0),
        target_exposure_ev,
        saturate(TruthAutoExposureBlend));
    float3 exposed = TruthApplyExposure(linear_color, exposure_ev);
    return float4(saturate(TruthFilmicToneCurve3(TruthFiniteOrBlack(exposed))), 1.0);
}

float4 TruthEnbFallbackPixel(VS_OUTPUT_POST input) : SV_Target
{
    return float4(saturate(TextureColor.Sample(Sampler0, input.txcoord0).rgb), 1.0);
}

technique11 Draw <string UIName = "Truth ENB";>
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, VS_Draw()));
        SetPixelShader(CompileShader(ps_5_0, TruthEnbPixelMain()));
    }
}

// This is intentionally Truth-named. ORIGINALPOSTPROCESS is an ENB-reserved
// vanilla implementation and must not be redefined by a custom shortcut.
technique11 TRUTHPASSTHROUGH <string UIName = "Truth: Safe passthrough";>
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, VS_Draw()));
        SetPixelShader(CompileShader(ps_5_0, TruthEnbFallbackPixel()));
    }
}



technique11 ORIGINALPOSTPROCESS <string UIName="Vanilla";> //do not modify this technique
{
	pass p0
	{
		SetVertexShader(CompileShader(vs_5_0, VS_Draw()));
		SetPixelShader(CompileShader(ps_5_0, PS_DrawOriginal()));
	}
}
