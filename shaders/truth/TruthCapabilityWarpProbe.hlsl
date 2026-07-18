#ifndef TRUTH_CAPABILITY_WARP_NATIVE_AVAILABLE
#define TRUTH_CAPABILITY_WARP_NATIVE_AVAILABLE 1
#endif

#define TRUTH_STAGE_CAPABILITY TRUTH_CAPABILITY_NATIVE
#define TRUTH_STAGE_OWNS_COLOR 0
#define TRUTH_STAGE_OWNS_DEPTH 1
#define TRUTH_STAGE_OWNS_NORMAL 0
#define TRUTH_STAGE_OWNS_MASK 0
#define TRUTH_STAGE_OWNS_NATIVE_CELESTIAL_VIEW TRUTH_CAPABILITY_WARP_NATIVE_AVAILABLE
#define TRUTH_STAGE_OWNS_PREVIOUS_SCALAR_ADAPTATION 0
#define TRUTH_STAGE_OWNS_BRIDGE_VALUE 1
#define TRUTH_STAGE_NATIVE_CAPABILITY_AVAILABLE TRUTH_CAPABILITY_WARP_NATIVE_AVAILABLE
#define TRUTH_STAGE_BRIDGE_CAPABILITY_AVAILABLE 1
#define TRUTH_STAGE_SPATIAL_CAPABILITY_AVAILABLE 1
#define TRUTH_STAGE_SCRATCH_OWNER TRUTH_SCRATCH_NONE
#define TRUTH_STAGE_SCRATCH_READ TRUTH_SCRATCH_NONE
#define TRUTH_STAGE_OWNS_FULL_FRAME_HISTORY 0
#define TRUTH_STAGE_OWNS_OBJECT_MOTION 0
#define TRUTH_STAGE_TREATS_SCRATCH_AS_HISTORY 0
#define TRUTH_STAGE_CROSS_EFFECT_ALPHA_PACKING 0

#include "TruthHostCapabilities.fxh"
#include "TruthPipelineCommon.fxh"

struct TruthCapabilityWarpInput
{
    float4 native_color;
    float4 bridge_color;
    float4 spatial_color;
    float4 identity_color;
    float4 runtime_availability;
};

struct TruthCapabilityWarpOutput
{
    float4 wrapper_color;
    float4 direct_color;
    uint route;
    float3 padding;
};

StructuredBuffer<TruthCapabilityWarpInput> TruthCapabilityWarpInputs : register(t0);
RWStructuredBuffer<TruthCapabilityWarpOutput> TruthCapabilityWarpOutputs : register(u0);

[numthreads(1, 1, 1)]
void TruthCapabilityWarpProbeMain(uint3 dispatch_id : SV_DispatchThreadID)
{
    TruthCapabilityWarpInput input = TruthCapabilityWarpInputs[dispatch_id.x];
    TruthCapabilityValue direct = TruthResolveCapability(
        TruthMakeCapability(input.native_color, input.runtime_availability.x),
        TruthMakeCapability(input.bridge_color, input.runtime_availability.y),
        TruthMakeCapability(input.spatial_color, input.runtime_availability.z),
        TruthMakeCapability(input.identity_color, input.runtime_availability.w));

    TruthCapabilityWarpOutput output;
    output.wrapper_color = TruthResolveCapabilityColor(
        input.native_color,
        input.bridge_color,
        input.spatial_color,
        input.identity_color,
        input.runtime_availability.x,
        input.runtime_availability.y,
        input.runtime_availability.z);
    output.direct_color = direct.color;
    output.route = direct.route;
    output.padding = 0.0.xxx;
    TruthCapabilityWarpOutputs[dispatch_id.x] = output;
}
