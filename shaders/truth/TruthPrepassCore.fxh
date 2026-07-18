#ifndef TRUTH_PREPASS_CORE_FXH
#define TRUTH_PREPASS_CORE_FXH

// This header is intentionally included after the prepass host declarations.
// It owns the one HDR scene composition point: exterior environment or
// interior response, then current-frame screen-space work.  The main effect
// consumes the resulting TextureColor and never rebuilds this environment.

#include "TruthColorCore.fxh"
#include "TruthAtmosphereCore.fxh"
#include "TruthSkyViewAdapter.fxh"
#include "TruthRuntimeParameters.fxh"
#include "TruthEffectParameters.fxh"
#include "TruthPipelineCommon.fxh"
#include "TruthSkyFields.fxh"
#include "TruthCloudLighting.fxh"
#include "TruthCloudVolume.fxh"
#include "TruthInteriorLight.fxh"
#include "TruthScreenSpace.fxh"

// Versioned SkyrimBridge v3 interoperability surface.  These are independent
// ENB UI-name bindings rather than an include dependency, so Truth can run
// without SkyrimBridge and can safely select the spatial fallback when its
// values are absent.  SkyrimBridge publishes the same stable UI names.
float4 TruthBridgeSunDirection
<
    string UIName = "SB_Sun_Direction";
    string UIWidget = "Color";
    int UIHidden = 1;
> = {0.0, 0.0, 0.0, 0.0};

float4 TruthBridgeRenderFrame
<
    string UIName = "SB_Render_Frame";
    string UIWidget = "Color";
    int UIHidden = 1;
> = {0.0, 0.0, 0.0, 0.0};

float4 TruthBridgeInteriorAmbient
<
    string UIName = "SB_Interior_Ambient";
    string UIWidget = "Color";
    int UIHidden = 1;
> = {0.0, 0.0, 0.0, 0.0};

float4 TruthBridgeInteriorDirectional
<
    string UIName = "SB_Interior_DirColor";
    string UIWidget = "Color";
    int UIHidden = 1;
> = {0.0, 0.0, 0.0, 0.0};

struct TruthPrepassResult
{
    float3 color;
    float environment_applied;
};

struct TruthPrepassCelestial
{
    float3 direction;
    float elevation;
    float availability;
};

float TruthPrepassLuminance(float3 value)
{
    return dot(max(value, 0.0.xxx), float3(0.2126, 0.7152, 0.0722));
}

TruthPrepassCelestial TruthPrepassMakeCelestial(
    float3 direction,
    float availability)
{
    TruthPrepassCelestial output;
    output.direction = float3(0.0, 0.0, 1.0);
    output.elevation = 0.0;
    output.availability = 0.0;
    float length_squared = dot(direction, direction);
    if (availability > 0.0
        && TruthSkyViewFinite3(direction)
        && TruthFinite1(length_squared)
        && length_squared > 0.0001)
    {
        output.direction = direction * rsqrt(length_squared);
        output.elevation = clamp(output.direction.z, -1.0, 1.0);
        output.availability = 1.0;
    }
    return output;
}

TruthPrepassCelestial TruthPrepassResolveNativeCelestial()
{
    return TruthPrepassMakeCelestial(
        TruthRuntimeCelestial.xyz,
        TruthRuntimeCelestialReady() ? 1.0 : 0.0);
}

bool TruthPrepassBridgeActive()
{
    return TruthSkyViewFinite4(TruthBridgeRenderFrame)
        && TruthBridgeRenderFrame.x > 0.0
        && TruthSkyViewFinite4(TruthBridgeSunDirection);
}

TruthPrepassCelestial TruthPrepassResolveBridgeCelestial()
{
    return TruthPrepassMakeCelestial(
        TruthBridgeSunDirection.xyz,
        TruthPrepassBridgeActive() ? 1.0 : 0.0);
}

TruthPrepassCelestial TruthPrepassResolveSpatialCelestial()
{
    // Do not invent a sun azimuth from game hour. Missing native/Bridge
    // celestial state fails closed so the authored scene is preserved.
    return TruthPrepassMakeCelestial(float3(0.0, 0.0, 0.0), 0.0);
}

TruthPrepassCelestial TruthPrepassResolveCelestial()
{
    TruthPrepassCelestial native_value = TruthPrepassResolveNativeCelestial();
    TruthPrepassCelestial bridge_value = TruthPrepassResolveBridgeCelestial();
    TruthPrepassCelestial spatial_value = TruthPrepassResolveSpatialCelestial();
    TruthCapabilityValue selected = TruthResolveCapability(
        TruthMakeCapability(
            float4(native_value.direction, native_value.elevation),
            native_value.availability),
        TruthMakeCapability(
            float4(bridge_value.direction, bridge_value.elevation),
            bridge_value.availability),
        TruthMakeCapability(
            float4(spatial_value.direction, spatial_value.elevation),
            spatial_value.availability),
        TruthMakeCapability(float4(0.0, 0.0, 1.0, 0.0), 0.0));
    TruthPrepassCelestial output = TruthPrepassMakeCelestial(
        selected.color.xyz, selected.availability);
    output.elevation = clamp(selected.color.w, -1.0, 1.0);
    return output;
}

TruthSkyViewAdapterOutput TruthPrepassResolveSkyView(float2 texcoord)
{
    TruthSkyViewAdapterInput input;
    input.texcoord = texcoord;
    input.inverse_view_projection = TruthRuntimeBuildInverseViewProjection();
    input.camera_world_position = TruthRuntimeCameraWorld.xyz;
    input.aurora_world_origin = TruthAuroraWorldOrigin;
    input.engine_world_units_per_aurora_unit = TruthRuntimeStatus.w;
    return TruthEvaluateSkyViewAdapter(input);
}

float3 TruthPrepassResolveSkyRadiance(
    TruthSkyViewAdapterOutput sky_view,
    float2 texcoord,
    out float valid)
{
    valid = 0.0;
    TruthPrepassCelestial celestial = TruthPrepassResolveCelestial();
    if (sky_view.valid <= 0.5 || celestial.availability <= 0.5)
    {
        return float3(0.0, 0.0, 0.0);
    }

    float phase = TruthFinite1(Timer.x) ? frac(max(Timer.x, 0.0)) : 0.0;
    float night_factor = saturate(1.0 - ENightDayFactor);
    TruthSkyFieldInput sky_field_input;
    sky_field_input.view_direction = sky_view.view_world_direction;
    sky_field_input.phase = phase;
    sky_field_input.wind = clamp(float2(TruthSkyWindX, TruthSkyWindY), -1.0, 1.0);
    sky_field_input.cloud_coverage = saturate(TruthCloudCoverage);
    sky_field_input.cloud_density = saturate(TruthCloudDensity);
    sky_field_input.weather_density = saturate(TruthWeatherDensity);
    sky_field_input.aurora_activity = saturate(TruthAuroraActivity)
        * saturate(TruthAuroraMask);
    sky_field_input.night_factor = night_factor;
    sky_field_input.camera_position = sky_view.camera_aurora_position;
    TruthSkyFieldOutput sky_field = TruthEvaluateSkyFields(sky_field_input);

    TruthUnifiedAtmosphereInput atmosphere_input;
    atmosphere_input.view_zenith_cosine = clamp(sky_view.view_world_direction.z, -1.0, 1.0);
    atmosphere_input.view_sun_cosine = clamp(
        dot(sky_view.view_world_direction, celestial.direction), -1.0, 1.0);
    atmosphere_input.sun_elevation = celestial.elevation;
    atmosphere_input.weather_density = saturate(TruthWeatherDensity);
    atmosphere_input.cloud_coverage = 0.0;
    atmosphere_input.cloud_density = 0.0;
    atmosphere_input.fog_density = saturate(TruthFogDensity);
    atmosphere_input.aurora_activity = 0.0;
    atmosphere_input.aurora_mask = 0.0;
    atmosphere_input.night_factor = night_factor;
    TruthUnifiedAtmosphereOutput atmosphere = TruthEvaluateAtmosphere(atmosphere_input);

    float3 composite_radiance;
#if TRUTH_QUALITY_TIER >= 2
    TruthCloudVolumeInput volume_input;
    volume_input.camera_position = sky_view.camera_aurora_position;
    volume_input.view_direction = sky_view.view_world_direction;
    volume_input.sun_direction = celestial.direction;
    volume_input.cloud_base_height = 1.20;
    volume_input.cloud_top_height = 3.80;
    volume_input.max_distance = 60.0;
    volume_input.phase = phase;
    volume_input.wind = sky_field_input.wind;
    volume_input.cloud_coverage = sky_field_input.cloud_coverage;
    volume_input.cloud_density = sky_field_input.cloud_density;
    volume_input.weather_density = sky_field_input.weather_density;
    volume_input.cloud_type = 0.0;
    volume_input.night_factor = night_factor;
    volume_input.pixel_coordinate = uint2(
        saturate(texcoord) * max(ScreenSize.xy, float2(1.0, 1.0)));
    // No frame-random jitter is permitted without a temporal resolver.
    volume_input.jitter_frame = 0u;
    TruthCloudVolumeOutput volume = TruthEvaluateCloudVolume(volume_input);
    float combined_transmittance = volume.transmittance * atmosphere.fog_transmittance;
    composite_radiance = (atmosphere.sky_radiance * combined_transmittance)
        + (volume.radiance * atmosphere.fog_transmittance)
        + (sky_field.aurora_intrinsic_radiance * combined_transmittance);
#else
    TruthCloudLightingInput cloud_input;
    cloud_input.sky_radiance = atmosphere.sky_radiance;
    cloud_input.aurora_intrinsic_radiance = sky_field.aurora_intrinsic_radiance;
    cloud_input.view_zenith_cosine = atmosphere_input.view_zenith_cosine;
    cloud_input.view_sun_cosine = atmosphere_input.view_sun_cosine;
    cloud_input.sun_elevation = atmosphere_input.sun_elevation;
    cloud_input.cloud_density = sky_field.cloud_density;
    cloud_input.cloud_detail_erosion = sky_field.cloud_detail_erosion;
    cloud_input.weather_density = atmosphere_input.weather_density;
    cloud_input.night_factor = night_factor;
    cloud_input.fog_transmittance = atmosphere.fog_transmittance;
    TruthCloudLightingOutput cloud = TruthEvaluateCloudLighting(cloud_input);
    composite_radiance = cloud.composite_radiance;
#endif

    valid = 1.0;
    return TruthFiniteOrBlack(composite_radiance)
        * max(TruthSkyRadianceScale, 0.0);
}

float3 TruthPrepassApplyInterior(float3 scene)
{
    if (!TruthPrepassBridgeActive())
    {
        return scene;
    }

    float3 ambient_color = TruthFiniteOrBlack(TruthBridgeInteriorAmbient.rgb)
        * saturate(TruthBridgeInteriorAmbient.w);
    float3 directional_color = TruthFiniteOrBlack(TruthBridgeInteriorDirectional.rgb)
        * saturate(TruthBridgeInteriorDirectional.w);
    TruthInteriorLightInput input;
    input.exterior_sky_luminance = 0.0;
    input.ambient_floor = clamp(
        TruthPrepassLuminance(ambient_color + directional_color), 0.0, 1.0);
    input.occlusion = 1.0;
    input.aperture_count = 0u;
    [unroll]
    for (uint aperture_index = 0u;
         aperture_index < TRUTH_MAX_INTERIOR_APERTURES;
         ++aperture_index)
    {
        input.apertures[aperture_index].sky_visibility = 0.0;
        input.apertures[aperture_index].transmittance = 0.0;
    }
    TruthInteriorLightOutput light = TruthEvaluateInteriorLight(input);
    // A sealed room receives no exterior sky.  Bridge-provided room light is
    // deliberately restrained, bounded, and only appears on the interior arm.
    float response = saturate(light.interior_light) * 0.12;
    return scene * (1.0 + response)
        + ((ambient_color + directional_color) * (0.04 * response));
}

float3 TruthPrepassApplyScreenSpace(
    float3 scene,
    float raw_depth,
    float2 texcoord,
    float3 view_direction)
{
    TruthScreenSpaceInput input;
    input.texel_size = 1.0 / max(ScreenSize.xy, float2(1.0, 1.0));
    input.view_direction = view_direction;
    input.raw_depth = raw_depth;
    input.native_normal_valid = 1.0;
    input.skin_mask_valid = 1.0;
    input.sky_depth_threshold = TruthSkyDepthThreshold;
    input.ao_intensity = 0.16 * saturate(TruthPrepassIntensity);
    input.contact_intensity = 0.25;
    input.ao_radius_pixels = 6.0 + (18.0 * saturate(TruthPrepassDepthShape));
    input.ssr_intensity = 0.08 * saturate(TruthPrepassIntensity);
    input.ssr_max_distance = 20.0;
    input.ssr_thickness = 0.12;
    input.diffusion_intensity = 0.10 * saturate(TruthPrepassIntensity);
    input.diffusion_radius_pixels = 1.5;
    return TruthApplyScreenSpaceEffects(
        scene, TextureColor, TextureDepth, TextureNormal, TextureMask,
        Sampler0, input, texcoord);
}

TruthPrepassResult TruthComposePrepass(
    float3 scene,
    float raw_depth,
    float2 texcoord,
    float interior_factor)
{
    TruthPrepassResult output;
    output.color = scene;
    output.environment_applied = 0.0;
    if (!TruthRuntimeReady())
    {
        return output;
    }

    TruthSkyViewAdapterOutput sky_view = TruthPrepassResolveSkyView(texcoord);
    if (sky_view.valid <= 0.5)
    {
        return output;
    }

    if (saturate(interior_factor) >= 0.5)
    {
        float3 interior_color = TruthPrepassApplyInterior(output.color);
        output.environment_applied = any(interior_color != output.color) ? 1.0 : 0.0;
        output.color = interior_color;
    }
    else if (TruthFinite1(raw_depth)
             && TruthProceduralSkyEnabled)
    {
        float sky_mask = TruthSkyMask(
            raw_depth, TruthSkyDepthThreshold, TruthSkyDepthFeather);
        if (sky_mask > 0.0)
        {
            float sky_valid;
            float3 sky_radiance = TruthPrepassResolveSkyRadiance(
                sky_view, texcoord, sky_valid);
            float environment_weight = sky_mask * sky_valid
                * saturate(TruthSkyReplacementStrength);
            if (environment_weight > 0.0)
            {
                output.color = lerp(output.color, sky_radiance, environment_weight);
                output.environment_applied = environment_weight;
            }
        }
    }

    output.color = TruthPrepassApplyScreenSpace(
        output.color, raw_depth, texcoord, sky_view.view_world_direction);
    output.color = TruthFiniteOrBlack(output.color);
    return output;
}

#endif
