#define TRUTH_STAGE_CAPABILITY TRUTH_CAPABILITY_NATIVE
#define TRUTH_STAGE_OWNS_COLOR 1
#define TRUTH_STAGE_OWNS_DEPTH 0
#define TRUTH_STAGE_OWNS_NORMAL 0
#define TRUTH_STAGE_OWNS_MASK 0
#define TRUTH_STAGE_OWNS_NATIVE_CELESTIAL_VIEW 1
#define TRUTH_STAGE_OWNS_PREVIOUS_SCALAR_ADAPTATION 1
#define TRUTH_STAGE_OWNS_BRIDGE_VALUE 0
#define TRUTH_STAGE_SCRATCH_OWNER TRUTH_SCRATCH_MAIN
#define TRUTH_STAGE_SCRATCH_READ TRUTH_SCRATCH_NONE
#define TRUTH_STAGE_OWNS_FULL_FRAME_HISTORY 0
#define TRUTH_STAGE_OWNS_OBJECT_MOTION 0
#define TRUTH_STAGE_TREATS_SCRATCH_AS_HISTORY 0
#define TRUTH_STAGE_CROSS_EFFECT_ALPHA_PACKING 0
#define TRUTH_STAGE_NATIVE_CAPABILITY_AVAILABLE 1
#define TRUTH_STAGE_BRIDGE_CAPABILITY_AVAILABLE 0
#define TRUTH_STAGE_SPATIAL_CAPABILITY_AVAILABLE 0

#include "truth/TruthColorCore.fxh"
#include "truth/TruthSkyViewAdapter.fxh"
#include "truth/TruthRuntimeParameters.fxh"
#include "truth/TruthEffectParameters.fxh"
#include "truth/TruthHostCapabilities.fxh"
#include "truth/TruthPipelineCommon.fxh"

// ENBSeries 0.504 main-effect interface.  TextureColor is the HDR result of
// the prepass; this stage owns optical mix, exposure, tone mapping, and color.
float4 Timer;
float EInteriorFactor;
float4 Params01[7];
float4 ENBParams01;

Texture2D TextureColor;
Texture2D TextureBloom;
Texture2D TextureLens;
Texture2D TextureAdaptation;

SamplerState Sampler0
{
    Filter = MIN_MAG_MIP_POINT;
    AddressU = Clamp;
    AddressV = Clamp;
};

SamplerState Sampler1
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Clamp;
    AddressV = Clamp;
};

struct TruthEnbVertexInput
{
    float3 position : POSITION;
    float2 texcoord : TEXCOORD0;
};

struct TruthEnbVertexOutput
{
    float4 position : SV_POSITION;
    float2 txcoord0 : TEXCOORD0;
};

TruthEnbVertexOutput TruthEnbVertex(TruthEnbVertexInput input)
{
    TruthEnbVertexOutput output;
    output.position = float4(input.position, 1.0);
    output.txcoord0 = input.texcoord;
    return output;
}

float TruthResolveMainAdaptationLuminance(float raw_luminance)
{
    return TruthFinite1(raw_luminance)
        ? clamp(raw_luminance, TruthLuminanceFloor, 65504.0)
        : TruthMiddleGray;
}

float TruthResolveMainNonNegativeControl(float value)
{
    return TruthFinite1(value) ? max(value, 0.0) : 0.0;
}

float3 TruthResolveEnbOpticalInput(float2 texcoord)
{
    float3 scene = TruthFiniteOrBlack(
        TextureColor.Sample(Sampler0, texcoord).rgb);
    float3 lens_payload = 0.0.xxx;
    if (TruthUseEnbLens)
    {
        lens_payload = TruthFiniteOrBlack(
            TextureLens.Sample(Sampler1, texcoord).rgb)
            * TruthResolveMainNonNegativeControl(ENBParams01.y);
    }
    float3 bloom_payload = 0.0.xxx;
    if (TruthUseEnbBloom)
    {
        bloom_payload = TruthFiniteOrBlack(
            TextureBloom.Sample(Sampler1, texcoord).rgb)
            * TruthResolveMainNonNegativeControl(ENBParams01.x);
    }
    return scene + bloom_payload + lens_payload;
}

float3 TruthResolveMainCapability(float3 color)
{
    // The prepass owns scene augmentation.  The main pass retains the
    // established runtime ABI while its capability result stays color-neutral.
    return TruthResolveCapabilityColor(
        float4(color, 1.0), TruthRuntimeReady() ? 1.0 : 0.0, 0.0, 0.0).rgb;
}

float3 TruthCompressDisplayGamut(float3 color)
{
    float peak = max(max(color.r, color.g), color.b);
    return peak > 1.0 ? color / peak : max(color, 0.0);
}

float4 TruthEnbPixelMain(TruthEnbVertexOutput input) : SV_Target
{
    float3 linear_color = TruthResolveMainCapability(
        TruthResolveEnbOpticalInput(input.txcoord0));
    if (!TruthMasterEnabled)
    {
        return float4(saturate(linear_color), 1.0);
    }

    float measured_luminance = TruthResolveMainAdaptationLuminance(
        TextureAdaptation.SampleLevel(Sampler0, input.txcoord0, 0.0).x);
    TruthAtmosphereSample metering;
    metering.scene_luminance = measured_luminance;
    metering.sky_luminance = measured_luminance;
    metering.interior_factor = saturate(EInteriorFactor);
    metering.delta_seconds = max(Timer.w, 0.0);
    metering.discontinuity = 0.0;
    float target_exposure_ev = TruthTargetExposureEv(metering);
    float exposure_ev = lerp(
        clamp(TruthManualExposureEv, -8.0, 8.0),
        target_exposure_ev,
        saturate(TruthAutoExposureBlend));
    float3 exposed = TruthApplyExposure(linear_color, exposure_ev);
    float3 display_color = TruthFilmicToneCurve3(TruthFiniteOrBlack(exposed));
    return float4(saturate(TruthCompressDisplayGamut(display_color)), 1.0);
}

float4 TruthEnbFallbackPixel(TruthEnbVertexOutput input) : SV_Target
{
    return TextureColor.Sample(Sampler0, input.txcoord0);
}

technique11 Draw <string UIName = "Truth ENB";>
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, TruthEnbVertex()));
        SetPixelShader(CompileShader(ps_5_0, TruthEnbPixelMain()));
    }
}

technique11 TRUTHPASSTHROUGH <string UIName = "Truth: Safe passthrough";>
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, TruthEnbVertex()));
        SetPixelShader(CompileShader(ps_5_0, TruthEnbFallbackPixel()));
    }
}

// ENB reserves this technique name. The implementation is an independently
// authored scene-color identity fallback, not redistributed ENB/Bethesda code.
technique11 ORIGINALPOSTPROCESS <string UIName="Truth: Safe fallback";>
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, TruthEnbVertex()));
        SetPixelShader(CompileShader(ps_5_0, TruthEnbFallbackPixel()));
    }
}
