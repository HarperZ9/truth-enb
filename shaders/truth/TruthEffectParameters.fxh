#ifndef TRUTH_EFFECT_PARAMETERS_FXH
#define TRUTH_EFFECT_PARAMETERS_FXH

// Public-release defaults are intentionally restrained. Every large optical or
// atmospheric contribution remains available to authors, but the initial state
// avoids stacking lens artifacts, dense weather, full-strength sky replacement,
// and an always-on aurora before a user has calibrated the preset in-game.

bool TruthMasterEnabled
<
    string UIName = "[Truth 00] Master | Enabled";
> = true;

float TruthManualExposureEv
<
    string UIName = "[Truth 01] Exposure | Manual EV";
    string UIWidget = "Spinner";
    float UIMin = -8.0;
    float UIMax = 8.0;
    float UIStep = 0.05;
> = 0.0;

float TruthAutoExposureBlend
<
    string UIName = "[Truth 01] Exposure | Auto Blend";
    string UIWidget = "Spinner";
    float UIMin = 0.0;
    float UIMax = 1.0;
    float UIStep = 0.01;
> = 0.0;

bool TruthUseEnbBloom
<
    string UIName = "[Truth 02] Optical | ENB Bloom";
> = true;

bool TruthUseEnbLens
<
    string UIName = "[Truth 02] Optical | ENB Lens";
> = false;

bool TruthProceduralSkyEnabled
<
    string UIName = "[Truth 10] Sky | Procedural Replacement";
> = true;

float TruthSkyReplacementStrength
<
    string UIName = "[Truth 10] Sky | Replacement Strength";
    string UIWidget = "Spinner";
    float UIMin = 0.0;
    float UIMax = 1.0;
    float UIStep = 0.01;
> = 0.75;

float TruthSkyDepthThreshold
<
    string UIName = "[Truth 10] Sky | Depth Threshold";
    string UIWidget = "Spinner";
    float UIMin = 0.99;
    float UIMax = 0.99999;
    float UIStep = 0.0001;
> = 0.9998;

float TruthSkyDepthFeather
<
    string UIName = "[Truth 10] Sky | Depth Feather";
    string UIWidget = "Spinner";
    float UIMin = 0.00001;
    float UIMax = 0.005;
    float UIStep = 0.00001;
> = 0.0002;

float TruthSkyRadianceScale
<
    string UIName = "[Truth 10] Sky | Radiance Scale";
    string UIWidget = "Spinner";
    float UIMin = 0.1;
    float UIMax = 8.0;
    float UIStep = 0.05;
> = 0.90;

float TruthWeatherDensity
<
    string UIName = "[Truth 11] Weather | Density";
    string UIWidget = "Spinner";
    float UIMin = 0.0;
    float UIMax = 1.0;
    float UIStep = 0.01;
> = 0.18;

float TruthCloudCoverage
<
    string UIName = "[Truth 12] Clouds | Coverage";
    string UIWidget = "Spinner";
    float UIMin = 0.0;
    float UIMax = 1.0;
    float UIStep = 0.01;
> = 0.35;

float TruthCloudDensity
<
    string UIName = "[Truth 12] Clouds | Density";
    string UIWidget = "Spinner";
    float UIMin = 0.0;
    float UIMax = 1.0;
    float UIStep = 0.01;
> = 0.48;

float TruthFogDensity
<
    string UIName = "[Truth 13] Atmosphere | Fog Density";
    string UIWidget = "Spinner";
    float UIMin = 0.0;
    float UIMax = 1.0;
    float UIStep = 0.01;
> = 0.06;

float TruthAuroraActivity
<
    string UIName = "[Truth 14] Aurora | Activity";
    string UIWidget = "Spinner";
    float UIMin = 0.0;
    float UIMax = 1.0;
    float UIStep = 0.01;
> = 0.0;

float TruthAuroraMask
<
    string UIName = "[Truth 14] Aurora | Weather Mask";
    string UIWidget = "Spinner";
    float UIMin = 0.0;
    float UIMax = 1.0;
    float UIStep = 0.01;
> = 1.0;

float TruthSkyWindX
<
    string UIName = "[Truth 15] Motion | Wind X";
    string UIWidget = "Spinner";
    float UIMin = -1.0;
    float UIMax = 1.0;
    float UIStep = 0.01;
> = 0.18;

float TruthSkyWindY
<
    string UIName = "[Truth 15] Motion | Wind Y";
    string UIWidget = "Spinner";
    float UIMin = -1.0;
    float UIMax = 1.0;
    float UIStep = 0.01;
> = -0.08;

float3 TruthAuroraWorldOrigin
<
    string UIName = "[Truth 16] World | Aurora Origin";
    string UIWidget = "Vector";
> = {0.0, 0.0, 0.0};

#endif
