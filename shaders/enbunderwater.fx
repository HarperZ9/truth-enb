#define TRUTH_STAGE_CAPABILITY TRUTH_CAPABILITY_SPATIAL
#define TRUTH_STAGE_OWNS_COLOR 1
#define TRUTH_STAGE_OWNS_DEPTH 1
#define TRUTH_STAGE_OWNS_NORMAL 0
#define TRUTH_STAGE_OWNS_MASK 0
#define TRUTH_STAGE_OWNS_NATIVE_CELESTIAL_VIEW 0
#define TRUTH_STAGE_OWNS_PREVIOUS_SCALAR_ADAPTATION 0
#define TRUTH_STAGE_OWNS_BRIDGE_VALUE 0
#define TRUTH_STAGE_NATIVE_CAPABILITY_AVAILABLE 0
#define TRUTH_STAGE_BRIDGE_CAPABILITY_AVAILABLE 0
#define TRUTH_STAGE_SPATIAL_CAPABILITY_AVAILABLE 1
#define TRUTH_STAGE_SCRATCH_OWNER TRUTH_SCRATCH_UNDERWATER
#define TRUTH_STAGE_SCRATCH_READ TRUTH_SCRATCH_NONE
#define TRUTH_STAGE_OWNS_FULL_FRAME_HISTORY 0
#define TRUTH_STAGE_OWNS_OBJECT_MOTION 0
#define TRUTH_STAGE_TREATS_SCRATCH_AS_HISTORY 0
#define TRUTH_STAGE_CROSS_EFFECT_ALPHA_PACKING 0
#define TRUTH_STAGE_PARAMETER_SLOT 8
#include "truth/TruthHostCapabilities.fxh"
#include "truth/TruthPipelineCommon.fxh"
#include "truth/TruthStageParameters.fxh"

Texture2D TextureColor;
Texture2D TextureDepth;

SamplerState Sampler0
{
    Filter = MIN_MAG_MIP_POINT;
    AddressU = Clamp;
    AddressV = Clamp;
};

#include "truth/TruthUnderwater.fxh"

float4 TruthUnderwaterMain(TruthStageVSOutput input) : SV_Target
{
    float4 source = TextureColor.Sample(Sampler0, input.texcoord);
    if (!TruthStageIsActive() || TRUTH_STAGE_INTENSITY <= 0.0)
    {
        return TruthStageIdentity(source, false, 0.0);
    }
    float linear_depth =
        TextureDepth.SampleLevel(Sampler0, input.texcoord, 0.0).x;
    return float4(
        TruthEvaluateUnderwater(input.texcoord, source.rgb, linear_depth),
        source.a);
}

technique11 Draw <string UIName = "Truth [90] Underwater";>
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, TruthFullscreenVertex()));
        SetPixelShader(CompileShader(ps_5_0, TruthUnderwaterMain()));
    }
}
