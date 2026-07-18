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

#pragma warning(push)
#pragma warning(disable: 3206)
#include "enb/ENBSeries0504VanillaPostProcess.fxh"
#pragma warning(pop)

float3 TruthResolveEnbOpticalInput(float2 texcoord)
{
    float3 color = max(TextureColor.Sample(Sampler0, texcoord).rgb, 0.0);
    if (TruthUseEnbLens)
    {
        color += max(TextureLens.Sample(Sampler1, texcoord).rgb, 0.0)
            * max(ENBParams01.y, 0.0);
    }
    if (TruthUseEnbBloom)
    {
        float3 bloom = max(TextureBloom.Sample(Sampler1, texcoord).rgb, 0.0);
        color += max(bloom - color, 0.0) * max(ENBParams01.x, 0.0);
    }
    return color;
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

float4 TruthEnbPixelMain(VS_OUTPUT_POST input) : SV_Target
{
    float3 linear_color = TruthResolveMainCapability(
        TruthResolveEnbOpticalInput(input.txcoord0));
    if (!TruthMasterEnabled)
    {
        return float4(saturate(linear_color), 1.0);
    }

    float measured_luminance = max(
        TextureAdaptation.SampleLevel(Sampler0, input.txcoord0, 0.0).x,
        TruthLuminanceFloor);
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

float4 TruthEnbFallbackPixel(VS_OUTPUT_POST input) : SV_Target
{
    return float4(saturate(TextureColor.Sample(Sampler0, input.txcoord0).rgb), 1.0);
}

technique11 Draw <string UIName = "Truth ENB";>
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, VS_Draw()));
        SetPixelShader(CompileShader(ps_5_0, TruthEnbPixelMain()));
    }
}

// This is intentionally Truth-named. ORIGINALPOSTPROCESS is an ENB-reserved
// vanilla implementation and must not be redefined by a custom shortcut.
technique11 TRUTHPASSTHROUGH <string UIName = "Truth: Safe passthrough";>
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, VS_Draw()));
        SetPixelShader(CompileShader(ps_5_0, TruthEnbFallbackPixel()));
    }
}

technique11 ORIGINALPOSTPROCESS <string UIName="Vanilla";> //do not modify this technique
{
    pass p0
    {
        SetVertexShader(CompileShader(vs_5_0, VS_Draw()));
        SetPixelShader(CompileShader(ps_5_0, PS_DrawOriginal()));
    }
}
