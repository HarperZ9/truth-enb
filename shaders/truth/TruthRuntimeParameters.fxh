#ifndef TRUTH_RUNTIME_PARAMETERS_FXH
#define TRUTH_RUNTIME_PARAMETERS_FXH

// The probe declaration preserves the exact seven-vector runtime ABI in a
// reflected constant buffer so WARP can execute the same row builder and
// readiness gate used by the ENB effect.
#ifdef TRUTH_RUNTIME_PARAMETER_PROBE
cbuffer TruthRuntimeProbeParameters : register(b2)
{
    float4 TruthRuntimeInverseViewProjectionRow0;
    float4 TruthRuntimeInverseViewProjectionRow1;
    float4 TruthRuntimeInverseViewProjectionRow2;
    float4 TruthRuntimeInverseViewProjectionRow3;
    float4 TruthRuntimeCameraWorld;
    float4 TruthRuntimeCelestial;
    float4 TruthRuntimeStatus;
};
#else
// Runtime-owned values are hidden from the editor but remain addressable by
// ENB SDK v1002. The native bridge writes the UIName keys, not these HLSL
// identifiers. Defaults deliberately keep the world-space path disabled.
float4 TruthRuntimeInverseViewProjectionRow0
<
    string UIName = "Truth Runtime | Inverse VP Row 0";
    string UIWidget = "Color";
    int UIHidden = 1;
> = {1.0, 0.0, 0.0, 0.0};

float4 TruthRuntimeInverseViewProjectionRow1
<
    string UIName = "Truth Runtime | Inverse VP Row 1";
    string UIWidget = "Color";
    int UIHidden = 1;
> = {0.0, 1.0, 0.0, 0.0};

float4 TruthRuntimeInverseViewProjectionRow2
<
    string UIName = "Truth Runtime | Inverse VP Row 2";
    string UIWidget = "Color";
    int UIHidden = 1;
> = {0.0, 0.0, 1.0, 0.0};

float4 TruthRuntimeInverseViewProjectionRow3
<
    string UIName = "Truth Runtime | Inverse VP Row 3";
    string UIWidget = "Color";
    int UIHidden = 1;
> = {0.0, 0.0, 0.0, 1.0};

float4 TruthRuntimeCameraWorld
<
    string UIName = "Truth Runtime | Camera World";
    string UIWidget = "Color";
    int UIHidden = 1;
> = {0.0, 0.0, 0.0, 0.0};

// xyz = normalized world-space sun direction; w = validity.
float4 TruthRuntimeCelestial
<
    string UIName = "Truth Runtime | Celestial";
    string UIWidget = "Color";
    int UIHidden = 1;
> = {0.0, 0.0, 0.0, 0.0};

// x = protocol version, y = valid flag, z = generation, w = world units per
// aurora unit. Protocol v1 uses a row-major inverse view-projection matrix.
float4 TruthRuntimeStatus
<
    string UIName = "Truth Runtime | Status";
    string UIWidget = "Color";
    int UIHidden = 1;
> = {0.0, 0.0, 0.0, 0.0};
#endif

bool TruthRuntimeFinite1(float value)
{
    return (asuint(value) & 0x7fffffffu) < 0x7f800000u;
}

bool TruthRuntimeFinite4(float4 value)
{
    return all((asuint(value) & 0x7fffffffu) < 0x7f800000u);
}

bool TruthRuntimeReady()
{
    return TruthRuntimeFinite4(TruthRuntimeStatus)
        && (TruthRuntimeStatus.x == 1.0 || TruthRuntimeStatus.x == 1.1)
        && TruthRuntimeStatus.y > 0.5
        && TruthRuntimeStatus.w >= TruthSkyViewMinimumWorldScale
        && TruthRuntimeStatus.w <= TruthSkyViewMaximumWorldScale
        && TruthRuntimeFinite4(TruthRuntimeCameraWorld);
}

bool TruthRuntimeCelestialReady()
{
    float length_squared = dot(
        TruthRuntimeCelestial.xyz, TruthRuntimeCelestial.xyz);
    return TruthRuntimeStatus.x >= 1.1
        && TruthRuntimeFinite4(TruthRuntimeCelestial)
        && TruthRuntimeCelestial.w > 0.5
        && TruthRuntimeFinite1(length_squared)
        && length_squared > 0.0001;
}

float4x4 TruthRuntimeBuildInverseViewProjection()
{
    return float4x4(
        TruthRuntimeInverseViewProjectionRow0,
        TruthRuntimeInverseViewProjectionRow1,
        TruthRuntimeInverseViewProjectionRow2,
        TruthRuntimeInverseViewProjectionRow3);
}

#endif
