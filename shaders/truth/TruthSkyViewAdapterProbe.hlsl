#include "TruthSkyViewAdapter.fxh"

cbuffer TruthSkyViewProbeParameters : register(b2)
{
    float2 TruthSkyViewProbeTexcoord;
    float2 TruthSkyViewProbePadding0;
    row_major float4x4 TruthSkyViewProbeInverseViewProjection;
    float3 TruthSkyViewProbeCameraWorldPosition;
    float TruthSkyViewProbePadding1;
    float3 TruthSkyViewProbeAuroraWorldOrigin;
    float TruthSkyViewProbeEngineWorldUnitsPerAuroraUnit;
};

struct TruthSkyViewProbeResult
{
    float4 direction_and_valid;
    float4 camera_and_sentinel;
};

RWStructuredBuffer<TruthSkyViewProbeResult> TruthSkyViewProbeResults
    : register(u0);

[numthreads(1, 1, 1)]
void TruthSkyViewAdapterProbeMain(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (any(dispatch_thread_id != uint3(0, 0, 0)))
    {
        return;
    }

    TruthSkyViewAdapterInput input;
    input.texcoord = TruthSkyViewProbeTexcoord;
    input.inverse_view_projection = TruthSkyViewProbeInverseViewProjection;
    input.camera_world_position = TruthSkyViewProbeCameraWorldPosition;
    input.aurora_world_origin = TruthSkyViewProbeAuroraWorldOrigin;
    input.engine_world_units_per_aurora_unit =
        TruthSkyViewProbeEngineWorldUnitsPerAuroraUnit;

    TruthSkyViewAdapterOutput output = TruthEvaluateSkyViewAdapter(input);
    TruthSkyViewProbeResult result;
    result.direction_and_valid = float4(output.view_world_direction, output.valid);
    result.camera_and_sentinel = float4(output.camera_aurora_position, 47.0);
    TruthSkyViewProbeResults[0] = result;
}
