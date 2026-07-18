#define TRUTH_STAGE_CAPABILITY TRUTH_CAPABILITY_NATIVE
#define TRUTH_STAGE_OWNS_COLOR 1
#define TRUTH_STAGE_OWNS_DEPTH 1
#define TRUTH_STAGE_OWNS_NORMAL 1
#define TRUTH_STAGE_OWNS_MASK 1
#define TRUTH_STAGE_OWNS_NATIVE_CELESTIAL_VIEW 1
#define TRUTH_STAGE_OWNS_PREVIOUS_SCALAR_ADAPTATION 0
#define TRUTH_STAGE_OWNS_BRIDGE_VALUE 1
#define TRUTH_STAGE_NATIVE_CAPABILITY_AVAILABLE 1
#define TRUTH_STAGE_BRIDGE_CAPABILITY_AVAILABLE 1
#define TRUTH_STAGE_SPATIAL_CAPABILITY_AVAILABLE 1
#define TRUTH_STAGE_SCRATCH_OWNER TRUTH_SCRATCH_PREPASS
#define TRUTH_STAGE_SCRATCH_READ TRUTH_SCRATCH_NONE
#define TRUTH_STAGE_OWNS_FULL_FRAME_HISTORY 0
#define TRUTH_STAGE_OWNS_OBJECT_MOTION 0
#define TRUTH_STAGE_TREATS_SCRATCH_AS_HISTORY 0
#define TRUTH_STAGE_CROSS_EFFECT_ALPHA_PACKING 0

#include "truth/TruthSkyViewAdapter.fxh"
#include "truth/TruthRuntimeParameters.fxh"
#include "truth/TruthEffectParameters.fxh"
#include "truth/TruthHostCapabilities.fxh"
#include "truth/TruthPipelineCommon.fxh"
#define TRUTH_STAGE_PARAMETER_SLOT 0
#include "truth/TruthStageParameters.fxh"

// ENBSeries 0.504 prepass resources. TextureNormal and TextureMask are the
// host's current-frame normal and material-mask surfaces; neither is treated
// as history or cross-effect scratch.
float4 Timer;
float4 ScreenSize;
float4 Weather;
float ENightDayFactor;
float EInteriorFactor;

Texture2D TextureColor;
Texture2D TextureDepth;
Texture2D TextureNormal;
Texture2D TextureMask;

SamplerState Sampler0
{
    Filter = MIN_MAG_MIP_POINT;
    AddressU = Clamp;
    AddressV = Clamp;
};

#include "truth/TruthPrepassCore.fxh"

float4 TruthPrepassMain(TruthStageVSOutput input) : SV_Target
{
    float4 source = TextureColor.Sample(Sampler0, input.texcoord);
    if (!TruthStageIsActive() || !TruthRuntimeReady())
    {
        return TruthStageIdentity(source, false, 0.0);
    }

    float raw_depth = TextureDepth.SampleLevel(Sampler0, input.texcoord, 0.0).x;
    TruthPrepassResult composed = TruthComposePrepass(
        source.rgb, raw_depth, input.texcoord, EInteriorFactor);
    float3 color = lerp(source.rgb, composed.color, saturate(TruthPrepassIntensity));
    return float4(TruthFiniteOrBlack(color), source.a);
}

technique11 Draw <string UIName = "Truth [10] Prepass";>
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, TruthFullscreenVertex()));
        SetPixelShader(CompileShader(ps_5_0, TruthPrepassMain()));
    }
}
