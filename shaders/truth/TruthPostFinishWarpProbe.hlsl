// TruthPostFinishWarpProbe.hlsl — rendered contract probe for TruthPostFinish.fxh
//
// TruthFinishLdr is called from enbeffectpostpass.fx, so it is not reachable
// through the reference sky pass that TruthReferencePixelMain drives. It needs
// its own probe.
//
// The contract under test is that vignette strength is controllable
// independently of the stage gate. Zeroing TruthPostpassIntensity does disable
// the vignette, but it takes TruthTriangularDither with it, which bands a sky
// that has just been graded. A host that owns its own vignette needs the
// vignette off and the dither kept.
//
// The host supplies the four globals the header reads and dispatches one
// thread per probe point.

cbuffer TruthPostFinishProbeParams : register(b0)
{
    float4 ScreenSize;
    float  TruthPostpassIntensity;
    float  TruthPostpassVignetteStrength;
    float  TruthPostpassGrainShape;
    float  TruthPostFinishProbePad;
};

#include "TruthPostFinish.fxh"

struct TruthPostFinishWarpInput
{
    float4 texcoord;       // xy uv, zw unused
    float4 display_color;  // rgb display colour, w unused
};

struct TruthPostFinishWarpOutput
{
    float4 finished;     // rgb TruthFinishLdr output
    float4 dither_only;  // rgb TruthTriangularDither applied to the input alone
};

StructuredBuffer<TruthPostFinishWarpInput> TruthPostFinishProbeInputs : register(t0);
RWStructuredBuffer<TruthPostFinishWarpOutput> TruthPostFinishProbeOutputs : register(u0);

[numthreads(1, 1, 1)]
void TruthPostFinishWarpProbeMain(uint3 dispatch_id : SV_DispatchThreadID)
{
    const TruthPostFinishWarpInput probe =
        TruthPostFinishProbeInputs[dispatch_id.x];

    TruthPostFinishWarpOutput result;
    result.finished = float4(
        TruthFinishLdr(probe.texcoord.xy, probe.display_color.rgb), 0.0);
    result.dither_only = float4(
        TruthTriangularDither(probe.texcoord.xy, probe.display_color.rgb), 0.0);

    TruthPostFinishProbeOutputs[dispatch_id.x] = result;
}
