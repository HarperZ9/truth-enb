#define TRUTH_STAGE_CAPABILITY TRUTH_CAPABILITY_NATIVE
#define TRUTH_STAGE_OWNS_COLOR 1
#define TRUTH_STAGE_OWNS_DEPTH 0
#define TRUTH_STAGE_OWNS_NORMAL 0
#define TRUTH_STAGE_OWNS_MASK 0
#define TRUTH_STAGE_OWNS_NATIVE_CELESTIAL_VIEW 1
#define TRUTH_STAGE_OWNS_PREVIOUS_SCALAR_ADAPTATION 0
#define TRUTH_STAGE_OWNS_BRIDGE_VALUE 1
#define TRUTH_STAGE_NATIVE_CAPABILITY_AVAILABLE 1
#define TRUTH_STAGE_BRIDGE_CAPABILITY_AVAILABLE 1
#define TRUTH_STAGE_SPATIAL_CAPABILITY_AVAILABLE 0
#define TRUTH_STAGE_SCRATCH_OWNER TRUTH_SCRATCH_SUNSPRITE
#define TRUTH_STAGE_SCRATCH_READ TRUTH_SCRATCH_NONE
#define TRUTH_STAGE_OWNS_FULL_FRAME_HISTORY 0
#define TRUTH_STAGE_OWNS_OBJECT_MOTION 0
#define TRUTH_STAGE_TREATS_SCRATCH_AS_HISTORY 0
#define TRUTH_STAGE_CROSS_EFFECT_ALPHA_PACKING 0
#define TRUTH_STAGE_PARAMETER_SLOT 7
#include "truth/TruthHostCapabilities.fxh"
#include "truth/TruthPipelineCommon.fxh"
#include "truth/TruthStageParameters.fxh"
#include "truth/TruthSkyViewAdapter.fxh"
#include "truth/TruthRuntimeParameters.fxh"

float4 Timer;

Texture2D TextureColor;

float4 SB_Sun_Direction
<
    string UIName = "SB_Sun_Direction";
    string UIWidget = "Color";
    int UIHidden = 1;
> = {0.0, 0.0, 0.0, 0.0};

float4 SB_Render_Frame
<
    string UIName = "SB_Render_Frame";
    string UIWidget = "Color";
    int UIHidden = 1;
> = {0.0, 0.0, 0.0, 0.0};

SamplerState Sampler0
{
    Filter = MIN_MAG_MIP_POINT;
    AddressU = Clamp;
    AddressV = Clamp;
};

#include "truth/TruthSunSprite.fxh"

float4 TruthSunSpriteMain(TruthStageVSOutput input) : SV_Target
{
    float4 source = TextureColor.Sample(Sampler0, input.texcoord);
    if (!TruthStageIsActive() || TRUTH_STAGE_INTENSITY <= 0.0)
    {
        return TruthStageIdentity(source, false, 0.0);
    }
    TruthSunSpriteCelestial celestial = TruthSunSpriteResolveCelestial();
    if (celestial.availability <= 0.5)
    {
        return TruthStageIdentity(source, false, 0.0);
    }
    float3 bridge_keepalive = TruthRetainSkyrimBridgeSunBindings(
        input.texcoord);
    float3 sprite = TruthEvaluateSunSprite(
        input.texcoord,
        celestial.direction,
        celestial.visibility);
    return float4(
        saturate(TruthFiniteOrBlack(source.rgb + sprite + bridge_keepalive)),
        source.a);
}

technique11 Draw <string UIName = "Truth [80] Sun Sprite";>
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, TruthFullscreenVertex()));
        SetPixelShader(CompileShader(ps_5_0, TruthSunSpriteMain()));
    }
}
