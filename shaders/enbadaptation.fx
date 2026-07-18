#define TRUTH_STAGE_CAPABILITY TRUTH_CAPABILITY_BRIDGE
#define TRUTH_STAGE_OWNS_COLOR 1
#define TRUTH_STAGE_OWNS_DEPTH 0
#define TRUTH_STAGE_OWNS_NORMAL 0
#define TRUTH_STAGE_OWNS_MASK 0
#define TRUTH_STAGE_OWNS_NATIVE_CELESTIAL_VIEW 0
#define TRUTH_STAGE_OWNS_PREVIOUS_SCALAR_ADAPTATION 1
#define TRUTH_STAGE_SCRATCH_OWNER TRUTH_SCRATCH_ADAPTATION
#define TRUTH_STAGE_SCRATCH_READ TRUTH_SCRATCH_NONE
#define TRUTH_STAGE_OWNS_FULL_FRAME_HISTORY 0
#define TRUTH_STAGE_OWNS_OBJECT_MOTION 0
#define TRUTH_STAGE_TREATS_SCRATCH_AS_HISTORY 0
#define TRUTH_STAGE_CROSS_EFFECT_ALPHA_PACKING 0
#define TRUTH_STAGE_PARAMETER_SLOT 3
#include "truth/TruthHostCapabilities.fxh"
#include "truth/TruthPipelineCommon.fxh"
#include "truth/TruthStageParameters.fxh"

Texture2D TextureCurrent;
Texture2D TexturePrevious;

SamplerState Sampler0
{
    Filter = MIN_MAG_MIP_POINT;
    AddressU = Clamp;
    AddressV = Clamp;
};

float4 TruthAdaptationMain(TruthStageVSOutput input) : SV_Target
{
    float4 source = TextureCurrent.Sample(Sampler0, input.texcoord);
    return TruthStageIdentity(source, TruthStageIsActive(), TRUTH_STAGE_INTENSITY);
}

technique11 Draw <string UIName = "Truth [40] Adaptation";>
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, TruthFullscreenVertex()));
        SetPixelShader(CompileShader(ps_5_0, TruthAdaptationMain()));
    }
}
