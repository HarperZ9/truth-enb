#ifndef TRUTH_SUN_SPRITE_FXH
#define TRUTH_SUN_SPRITE_FXH

struct TruthSunSpriteCelestial
{
    float3 direction;
    float visibility;
    float availability;
};

bool TruthSunSpriteFinite1(float value)
{
    return (asuint(value) & 0x7fffffffu) < 0x7f800000u;
}

bool TruthSunSpriteFinite3(float3 value)
{
    return all((asuint(value) & 0x7fffffffu) < 0x7f800000u.xxx);
}

bool TruthSunSpriteFinite4(float4 value)
{
    return all((asuint(value) & 0x7fffffffu) < 0x7f800000u);
}

TruthSunSpriteCelestial TruthSunSpriteMakeCelestial(
    float3 sun_direction,
    float visibility,
    float availability)
{
    TruthSunSpriteCelestial output;
    output.direction = float3(0.0, 0.0, 0.0);
    output.visibility = 0.0;
    output.availability = 0.0;
    if (availability <= 0.0
        || !TruthSunSpriteFinite3(sun_direction)
        || !TruthSunSpriteFinite1(visibility))
    {
        return output;
    }

    float length_squared = dot(sun_direction, sun_direction);
    if (!TruthSunSpriteFinite1(length_squared) || length_squared <= 0.0001)
    {
        return output;
    }

    output.direction = sun_direction * rsqrt(length_squared);
    output.visibility = saturate(visibility);
    output.availability = 1.0;
    return output;
}

bool TruthSunSpriteBridgeActive()
{
    return TruthSunSpriteFinite4(SB_Render_Frame)
        && SB_Render_Frame.x > 0.0
        && TruthSunSpriteFinite4(SB_Sun_Direction);
}

TruthSunSpriteCelestial TruthSunSpriteResolveNativeCelestial()
{
    return TruthSunSpriteMakeCelestial(
        TruthRuntimeCelestial.xyz,
        TruthRuntimeCelestial.w,
        TruthRuntimeReady() && TruthRuntimeCelestialReady() ? 1.0 : 0.0);
}

TruthSunSpriteCelestial TruthSunSpriteResolveBridgeCelestial()
{
    // SkyrimBridge's direct SB_Sun_Direction.w is elevation, not a validity
    // flag.  Availability is therefore the finite direction plus a finite,
    // positive SB_Render_Frame publish marker; visibility stays bounded here.
    return TruthSunSpriteMakeCelestial(
        SB_Sun_Direction.xyz,
        1.0,
        TruthSunSpriteBridgeActive() ? 1.0 : 0.0);
}

TruthSunSpriteCelestial TruthSunSpriteResolveCelestial()
{
    TruthSunSpriteCelestial native_value =
        TruthSunSpriteResolveNativeCelestial();
    TruthSunSpriteCelestial bridge_value =
        TruthSunSpriteResolveBridgeCelestial();
    TruthCapabilityValue selected = TruthResolveCapability(
        TruthMakeCapability(
            float4(native_value.direction, native_value.visibility),
            native_value.availability),
        TruthMakeCapability(
            float4(bridge_value.direction, bridge_value.visibility),
            bridge_value.availability),
        TruthMakeCapability(float4(0.0, 0.0, 0.0, 0.0), 0.0),
        TruthMakeCapability(float4(0.0, 0.0, 0.0, 0.0), 0.0));
    return TruthSunSpriteMakeCelestial(
        selected.color.xyz, selected.color.w, selected.availability);
}

float3 TruthRetainSkyrimBridgeSunBindings(float2 uv)
{
    // Local two-binding mirror of SkyrimBridge.fxh's SB_Retain keepalive.
    // Truth consumes only SB_Sun_Direction and SB_Render_Frame here, so
    // importing the full 102-float4 SkyrimBridge header would expand the stage
    // ABI and retain data this shader never reads.
    float3 retained = 0.0.xxx;
    [branch] if (Timer.x < -1.0e15)
    {
        float4 sink = SB_Sun_Direction + SB_Render_Frame;
        retained = sink.rgb * uv.x * 0.0001;
    }
    return retained;
}

float3 TruthEvaluateSunSprite(
    float2 uv,
    float3 sun_direction,
    float visibility)
{
    if (TruthSunSpriteIntensity <= 0.0
        || !TruthSunSpriteFinite3(sun_direction)
        || !TruthSunSpriteFinite1(visibility)
        || visibility <= 0.0
        || saturate(visibility) <= 0.0)
    {
        return 0.0;
    }

    float direction_length_squared = dot(sun_direction, sun_direction);
    if (!TruthSunSpriteFinite1(direction_length_squared)
        || direction_length_squared <= 0.0001)
    {
        return 0.0;
    }

    float3 direction = sun_direction * rsqrt(direction_length_squared);
    float2 sun_uv = 0.5 + (direction.xy * 0.45);
    float radius = lerp(0.006, 0.018, saturate(TruthSunSpriteDiscShape));
    float distance_to_disc = length(uv - sun_uv);
    float disc = 1.0 - smoothstep(radius * 0.70, radius, distance_to_disc);
    float halo = exp2(-distance_to_disc * (48.0 / max(radius, 0.0001)));
    float3 sun_color = float3(1.0, 0.72, 0.42);
    return sun_color * (disc + 0.08 * halo)
        * saturate(visibility)
        * saturate(TruthSunSpriteIntensity);
}

#endif
