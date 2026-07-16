#define TRUTH_RUNTIME_PARAMETER_PROBE 1
#include "TruthSkyViewAdapter.fxh"
#include "TruthRuntimeParameters.fxh"

struct TruthRuntimeProbeResult
{
    float4 direction_and_valid;
    float4 camera_and_ready;
};

RWStructuredBuffer<TruthRuntimeProbeResult> TruthRuntimeProbeResults
    : register(u0);

[numthreads(1, 1, 1)]
void TruthRuntimeParametersProbeMain(uint3 dispatch_thread_id
                                     : SV_DispatchThreadID)
{
    if (any(dispatch_thread_id != uint3(0, 0, 0)))
    {
        return;
    }

    TruthSkyViewAdapterOutput output;
    output.view_world_direction = float3(0.0, 0.0, 1.0);
    output.camera_aurora_position = float3(0.0, 0.0, 0.0);
    output.valid = 0.0;
    bool ready = TruthRuntimeReady();
    if (ready)
    {
        TruthSkyViewAdapterInput input;
        input.texcoord = float2(0.5, 0.5);
        input.inverse_view_projection =
            TruthRuntimeBuildInverseViewProjection();
        input.camera_world_position = TruthRuntimeCameraWorld.xyz;
        input.aurora_world_origin = float3(0.0, 0.0, 0.0);
        input.engine_world_units_per_aurora_unit = TruthRuntimeStatus.w;
        output = TruthEvaluateSkyViewAdapter(input);
    }

    TruthRuntimeProbeResult result;
    result.direction_and_valid = float4(
        output.view_world_direction, output.valid);
    result.camera_and_ready = float4(
        output.camera_aurora_position, ready ? 47.0 : -47.0);
    TruthRuntimeProbeResults[0] = result;
}
