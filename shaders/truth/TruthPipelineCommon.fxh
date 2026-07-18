#ifndef TRUTH_PIPELINE_COMMON_FXH
#define TRUTH_PIPELINE_COMMON_FXH

#ifndef TRUTH_HOST_CAPABILITIES_FXH
#error Include TruthHostCapabilities.fxh after stage declarations and before TruthPipelineCommon.fxh
#endif

#include "TruthQuality.fxh"

// Truth uses device depth where sky is near one. All stage depth tests use this
// helper instead of duplicating a local threshold or feather.
#define TRUTH_DEPTH_CONVENTION_DEVICE_Z_SKY_AT_ONE 1

bool TruthFinite1(float value)
{
    return (asuint(value) & 0x7fffffffu) < 0x7f800000u;
}

float3 TruthFiniteOrBlack(float3 value)
{
    return all((asuint(value) & 0x7fffffffu) < 0x7f800000u.xxx)
        ? max(value, 0.0.xxx)
        : 0.0.xxx;
}

float TruthSkyMask(float raw_depth, float threshold, float feather)
{
    float safe_feather = clamp(feather, 0.00001, 0.005);
    return smoothstep(clamp(threshold, 0.99, 1.0),
                      min(clamp(threshold, 0.99, 1.0) + safe_feather, 1.0),
                      raw_depth);
}

float4 TruthIdentityColor(float4 value)
{
    return value;
}

float TruthIdentityScalar(float value)
{
    return value;
}

struct TruthStageVSOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

TruthStageVSOutput TruthFullscreenVertex(uint vertex_id : SV_VertexID)
{
    TruthStageVSOutput output;
    float2 triangle_position = vertex_id == 0u
        ? float2(-1.0, -1.0)
        : (vertex_id == 1u ? float2(-1.0, 3.0) : float2(3.0, -1.0));
    output.position = float4(triangle_position, 0.0, 1.0);
    output.texcoord = triangle_position * float2(0.5, -0.5) + 0.5;
    return output;
}

struct TruthCapabilityValue
{
    float3 radiance;
    float confidence;
};

TruthCapabilityValue TruthNativeCapability(TruthCapabilityValue value)
{
    return value;
}

TruthCapabilityValue TruthBridgeCapability(TruthCapabilityValue value)
{
    return value;
}

TruthCapabilityValue TruthSpatialCapability(TruthCapabilityValue value)
{
    return value;
}

TruthCapabilityValue TruthIdentityCapability(TruthCapabilityValue value)
{
    return value;
}

TruthCapabilityValue TruthResolveCapability(
    TruthCapabilityValue native_value,
    TruthCapabilityValue bridge_value,
    TruthCapabilityValue spatial_value,
    TruthCapabilityValue identity_value)
{
#if TRUTH_STAGE_CAPABILITY >= TRUTH_CAPABILITY_NATIVE
    if (native_value.confidence > 0.0)
    {
        return TruthNativeCapability(native_value);
    }
#endif
#if TRUTH_STAGE_CAPABILITY >= TRUTH_CAPABILITY_BRIDGE
    if (bridge_value.confidence > 0.0)
    {
        return TruthBridgeCapability(bridge_value);
    }
#endif
#if TRUTH_STAGE_CAPABILITY >= TRUTH_CAPABILITY_SPATIAL
    if (spatial_value.confidence > 0.0)
    {
        return TruthSpatialCapability(spatial_value);
    }
#endif
    return TruthIdentityCapability(identity_value);
}

float4 TruthStageIdentity(float4 source, bool stage_enabled, float intensity)
{
    if (!stage_enabled || intensity <= 0.0)
    {
        return TruthIdentityColor(source);
    }

    // Initial public stages are deliberately inert until their bounded effects
    // are introduced. This keeps every stage's default and zero-intensity path
    // an exact authored identity.
    return TruthIdentityColor(source);
}

#endif
