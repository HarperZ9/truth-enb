// TruthScreenSpaceWarpProbe.hlsl — rendered contract probe for TruthScreenSpace.fxh
//
// Task 3, Step 1. truth_scene_contracts greps the shader sources for required
// and forbidden tokens, which proves structure but not behaviour. This probe
// runs the real HLSL on WARP so the safety contracts can be asserted against
// actual output: an effect that should not fire must return the scene it was
// given, bit for bit.
//
// The host compiles this once per tier via TRUTH_QUALITY_TIER. Tier 2 is the
// lowest tier where SSR is live; at tiers 0 and 1 TruthApplySSR compiles out.

#ifndef TRUTH_QUALITY_TIER
#define TRUTH_QUALITY_TIER 2
#endif

// Screen-space effects consume depth, normal, and mask, so this stage is
// SPATIAL. It reads no celestial or view payload, which is what separates it
// from a NATIVE stage; declaring NATIVE here is rejected by
// TruthHostCapabilities.fxh, and correctly so.
#define TRUTH_STAGE_CAPABILITY TRUTH_CAPABILITY_SPATIAL
#define TRUTH_STAGE_OWNS_COLOR 1
#define TRUTH_STAGE_OWNS_DEPTH 1
#define TRUTH_STAGE_OWNS_NORMAL 1
#define TRUTH_STAGE_OWNS_MASK 1
#define TRUTH_STAGE_OWNS_NATIVE_CELESTIAL_VIEW 0
#define TRUTH_STAGE_OWNS_PREVIOUS_SCALAR_ADAPTATION 0
#define TRUTH_STAGE_OWNS_BRIDGE_VALUE 0
#define TRUTH_STAGE_NATIVE_CAPABILITY_AVAILABLE 0
#define TRUTH_STAGE_BRIDGE_CAPABILITY_AVAILABLE 0
#define TRUTH_STAGE_SPATIAL_CAPABILITY_AVAILABLE 1
#define TRUTH_STAGE_SCRATCH_OWNER TRUTH_SCRATCH_NONE
#define TRUTH_STAGE_SCRATCH_READ TRUTH_SCRATCH_NONE
#define TRUTH_STAGE_OWNS_FULL_FRAME_HISTORY 0
#define TRUTH_STAGE_OWNS_OBJECT_MOTION 0
#define TRUTH_STAGE_TREATS_SCRATCH_AS_HISTORY 0
#define TRUTH_STAGE_CROSS_EFFECT_ALPHA_PACKING 0

#include "TruthHostCapabilities.fxh"
#include "TruthPipelineCommon.fxh"
#include "TruthScreenSpace.fxh"

struct TruthScreenSpaceWarpInput
{
    float4 scene;              // rgb scene colour, w unused
    float4 texel_and_depth;    // xy texel size, z raw depth, w sky depth threshold
    float4 view_direction;     // xyz view direction, w unused
    float4 validity;           // x native_normal_valid, y skin_mask_valid, zw unused
    float4 ao;                 // x intensity, y contact intensity, z radius pixels, w unused
    float4 ssr;                // x intensity, y max distance, z thickness, w unused
    float4 diffusion;          // x intensity, y radius pixels, zw unused
    float4 texcoord;           // xy texcoord, zw unused
};

struct TruthScreenSpaceWarpOutput
{
    float4 scene_in;
    float4 after_ao;
    float4 after_ssr;
    float4 after_diffusion;
    float4 after_all;
    uint   geometry_valid;
    uint   quality_tier;
    uint   ao_directions;
    uint   ssr_steps;
};

Texture2D TruthProbeScene        : register(t0);
Texture2D TruthProbeDepth        : register(t1);
Texture2D TruthProbeNormal       : register(t2);
Texture2D TruthProbeMaterialMask : register(t3);
StructuredBuffer<TruthScreenSpaceWarpInput> TruthProbeInputs : register(t4);

RWStructuredBuffer<TruthScreenSpaceWarpOutput> TruthProbeOutputs : register(u0);

SamplerState TruthProbeSampler : register(s0);

[numthreads(1, 1, 1)]
void TruthScreenSpaceWarpProbeMain(uint3 dispatch_id : SV_DispatchThreadID)
{
    TruthScreenSpaceWarpInput probe = TruthProbeInputs[dispatch_id.x];

    TruthScreenSpaceInput input;
    input.texel_size             = probe.texel_and_depth.xy;
    input.view_direction         = probe.view_direction.xyz;
    input.raw_depth              = probe.texel_and_depth.z;
    input.native_normal_valid    = probe.validity.x;
    input.skin_mask_valid        = probe.validity.y;
    input.sky_depth_threshold    = probe.texel_and_depth.w;
    input.ao_intensity           = probe.ao.x;
    input.contact_intensity      = probe.ao.y;
    input.ao_radius_pixels       = probe.ao.z;
    input.ssr_intensity          = probe.ssr.x;
    input.ssr_max_distance       = probe.ssr.y;
    input.ssr_thickness          = probe.ssr.z;
    input.diffusion_intensity    = probe.diffusion.x;
    input.diffusion_radius_pixels = probe.diffusion.y;

    const float3 scene = probe.scene.rgb;
    const float2 texcoord = probe.texcoord.xy;

    // Each effect is run in isolation from the same input scene so a failing
    // contract names one effect instead of the composite.
    const float3 after_ao = TruthApplyGTAO(
        scene, TruthProbeDepth, TruthProbeNormal,
        TruthProbeSampler, input, texcoord);

    const float3 after_ssr = TruthApplySSR(
        scene, TruthProbeScene, TruthProbeDepth, TruthProbeNormal,
        TruthProbeSampler, input, texcoord);

    const float3 after_diffusion = TruthApplySkinDiffusion(
        scene, TruthProbeScene, TruthProbeDepth, TruthProbeNormal,
        TruthProbeMaterialMask, TruthProbeSampler, input, texcoord);

    const float3 after_all = TruthApplyScreenSpaceEffects(
        scene, TruthProbeScene, TruthProbeDepth, TruthProbeNormal,
        TruthProbeMaterialMask, TruthProbeSampler, input, texcoord);

    TruthScreenSpaceWarpOutput output;
    output.scene_in        = float4(scene, 1.0);
    output.after_ao        = float4(after_ao, 1.0);
    output.after_ssr       = float4(after_ssr, 1.0);
    output.after_diffusion = float4(after_diffusion, 1.0);
    output.after_all       = float4(after_all, 1.0);
    output.geometry_valid  = TruthScreenSpaceGeometryValid(
        input.raw_depth, input.sky_depth_threshold) ? 1u : 0u;
    output.quality_tier    = TruthQualityTier;
    output.ao_directions   = TruthQualityAODirections;
    output.ssr_steps       = TruthQualitySSRSteps;

    TruthProbeOutputs[dispatch_id.x] = output;
}
