#include "truth/TruthColorCore.fxh"

Texture2D TruthSceneTexture : register(t0);

SamplerState TruthSceneSampler : register(s0)
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Clamp;
    AddressV = Clamp;
};

cbuffer TruthFrameParameters : register(b0)
{
    float TruthSceneLuminance;
    float TruthSkyLuminance;
    float TruthInteriorFactor;
    float TruthDeltaSeconds;
    float TruthExposureEv;
    float TruthUseTargetExposure;
    float2 TruthFramePadding;
};

struct TruthVertexInput
{
    float3 position : POSITION;
    float2 texcoord : TEXCOORD0;
};

struct TruthVertexOutput
{
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
};

TruthVertexOutput TruthVertexMain(TruthVertexInput input)
{
    TruthVertexOutput output;
    output.position = float4(input.position, 1.0);
    output.texcoord = input.texcoord;
    return output;
}

float4 TruthPixelMain(TruthVertexOutput input) : SV_Target0
{
    TruthAtmosphereSample atmosphere;
    atmosphere.scene_luminance = TruthSceneLuminance;
    atmosphere.sky_luminance = TruthSkyLuminance;
    atmosphere.interior_factor = TruthInteriorFactor;
    atmosphere.delta_seconds = TruthDeltaSeconds;
    atmosphere.discontinuity = 0.0;

    float target_exposure_ev = TruthTargetExposureEv(atmosphere);
    float selected_exposure_ev = lerp(TruthExposureEv,
                                      target_exposure_ev,
                                      saturate(TruthUseTargetExposure));
    float4 scene_color = TruthSceneTexture.Sample(TruthSceneSampler, input.texcoord);
    float3 exposed_color = TruthApplyExposure(scene_color.rgb, selected_exposure_ev);
    return float4(TruthFilmicToneCurve3(exposed_color), scene_color.a);
}

technique11 TruthMasterLookTechnique
{
    pass TruthMasterLookPass
    {
        SetVertexShader(CompileShader(vs_5_0, TruthVertexMain()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, TruthPixelMain()));
    }
}
