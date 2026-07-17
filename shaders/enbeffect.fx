#ifndef TRUTH_SKY_FIELD_QUALITY
// The live ENB pass uses the balanced five-noise cloud field. Reference and CPU
// parity shaders retain the full quality-2 field unless they opt down.
#define TRUTH_SKY_FIELD_QUALITY 1
#endif

#include "truth/TruthColorCore.fxh"
#include "truth/TruthAtmosphereCore.fxh"
#include "truth/TruthSkyFields.fxh"
#include "truth/TruthCloudLighting.fxh"
#include "truth/TruthSkyViewAdapter.fxh"
#include "truth/TruthRuntimeParameters.fxh"
#include "truth/TruthEffectParameters.fxh"

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

bool TruthFinite3(float3 value)
{
    return all((asuint(value) & 0x7fffffffu) < 0x7f800000u);
}

float3 TruthResolveProceduralSunDirection(out float sun_elevation)
{
    static const float TruthPi = 3.14159265358979323846;
    // This remains the bounded fallback until the runtime publishes the engine
    // sun vector. Keep it isolated so the eventual ABI replacement is explicit.
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

    float3 radiance = cloud.composite_radiance
        * clamp(TruthSkyRadianceScale, 0.0, 8.0);
    if (!TruthFinite3(radiance))
    {
        return float3(0.0, 0.0, 0.0);
    }
    valid = 1.0;
    return clamp(radiance, 0.0, 16.0);
}

float3 TruthResolveEnbOpticalInput(float2 texcoord)
{
    float3 color = TextureColor.Sample(Sampler0, texcoord).rgb;
    color = TruthFinite3(color) ? max(color, 0.0) : float3(0.0, 0.0, 0.0);

    float lens_strength = TruthRuntimeFinite1(ENBParams01.y)
        ? clamp(ENBParams01.y, 0.0, 4.0)
        : 0.0;
    if (TruthUseEnbLens && lens_strength > 0.0)
    {
        float3 lens = TextureLens.Sample(Sampler1, texcoord).rgb;
        if (TruthFinite3(lens))
        {
            color += max(lens, 0.0) * lens_strength;
        }
    }

    float bloom_strength = TruthRuntimeFinite1(ENBParams01.x)
        ? clamp(ENBParams01.x, 0.0, 4.0)
        : 0.0;
    if (TruthUseEnbBloom && bloom_strength > 0.0)
    {
        float3 bloom = TextureBloom.Sample(Sampler1, texcoord).rgb;
        if (TruthFinite3(bloom))
        {
            color += max(max(bloom, 0.0) - color, 0.0) * bloom_strength;
        }
    }
    return TruthFinite3(color) ? min(color, 65536.0) : float3(0.0, 0.0, 0.0);
}

float4 TruthEnbPixelMain(VS_OUTPUT_POST input) : SV_Target
{
    float3 linear_color = TruthResolveEnbOpticalInput(input.txcoord0);
    if (!TruthMasterEnabled)
    {
        return float4(saturate(linear_color), 1.0);
    }

#if TRUTH_ENABLE_ATMOSPHERE && TRUTH_ENABLE_PROCEDURAL_SKY
    if (TruthProceduralSkyEnabled && TruthRuntimeReady()
        && EInteriorFactor < 0.5)
    {
        float raw_depth = TextureDepth.SampleLevel(
            Sampler0, input.txcoord0, 0.0).x;
        if (TruthRuntimeFinite1(raw_depth))
        {
            float feather = clamp(TruthSkyDepthFeather, 0.00001, 0.005);
            // Keeping the lower and upper smoothstep edges distinct removes the
            // threshold==1 degeneracy that can produce NaNs or hard sky pops.
            float threshold = clamp(
                TruthSkyDepthThreshold, 0.99, 1.0 - feather);
            float sky_mask = smoothstep(
                threshold, threshold + feather, raw_depth);

            // A far-depth sample can belong to sky while sharing a derivative
            // quad with foreground geometry. Reject that silhouette quad rather
            // than bleeding the procedural sky over trees, hair, or mountains.
            float depth_gradient = max(
                abs(ddx(raw_depth)), abs(ddy(raw_depth)));
            float silhouette_width = max(feather * 8.0, 0.0001);
            float silhouette_guard = 1.0
                - saturate(depth_gradient / silhouette_width);
            sky_mask *= silhouette_guard
                * saturate(1.0 - EInteriorFactor);

            if (sky_mask > 0.0001)
            {
                float sky_valid;
                float3 sky_radiance = TruthResolveSkyRadiance(
                    input.txcoord0, sky_valid);
                linear_color = lerp(
                    linear_color,
                    sky_radiance,
                    sky_mask * sky_valid
                        * saturate(TruthSkyReplacementStrength));
            }
        }
    }
#endif

    float measured_luminance = TextureAdaptation.SampleLevel(
        Sampler0, input.txcoord0, 0.0).x;
    measured_luminance = TruthRuntimeFinite1(measured_luminance)
        ? max(measured_luminance, TruthLuminanceFloor)
        : TruthMiddleGray;
    TruthAtmosphereSample metering;
    metering.scene_luminance = measured_luminance;
    metering.sky_luminance = measured_luminance;
    metering.interior_factor = saturate(EInteriorFactor);
    metering.delta_seconds = TruthRuntimeFinite1(Timer.w)
        ? max(Timer.w, 0.0)
        : 0.0;
    metering.discontinuity = 0.0;
    float target_exposure_ev = TruthTargetExposureEv(metering);
    float manual_exposure_ev = TruthRuntimeFinite1(TruthManualExposureEv)
        ? clamp(TruthManualExposureEv, -8.0, 8.0)
        : 0.0;
    float auto_exposure_blend = TruthRuntimeFinite1(TruthAutoExposureBlend)
        ? saturate(TruthAutoExposureBlend)
        : 0.0;
    float exposure_ev = lerp(
        manual_exposure_ev,
        target_exposure_ev,
        auto_exposure_blend);
    float3 exposed = TruthApplyExposure(linear_color, exposure_ev);
    float3 tone_mapped = TruthFilmicToneCurve3(exposed);
    tone_mapped = TruthFinite3(tone_mapped)
        ? saturate(tone_mapped)
        : float3(0.0, 0.0, 0.0);
    return float4(tone_mapped, 1.0);
}

float4 TruthEnbFallbackPixel(VS_OUTPUT_POST input) : SV_Target
{
    float3 color = TextureColor.Sample(Sampler0, input.txcoord0).rgb;
    color = TruthFinite3(color) ? saturate(color) : float3(0.0, 0.0, 0.0);
    return float4(color, 1.0);
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
