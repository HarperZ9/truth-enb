#ifndef TRUTH_EFFECT_PARAMETERS_FXH
#define TRUTH_EFFECT_PARAMETERS_FXH

bool TruthMasterEnabled
<
    string UIName = "[Truth 00] Master | Enabled";
> = true;

float TruthManualExposureEv
<
    string UIName = "[Truth 60] Main Effect | Manual EV";
    string UIWidget = "Spinner";
    float UIMin = -8.0;
    float UIMax = 8.0;
    float UIStep = 0.05;
> = 0.0;

float TruthAutoExposureBlend
<
    string UIName = "[Truth 60] Main Effect | Auto Blend";
    string UIWidget = "Spinner";
    float UIMin = 0.0;
    float UIMax = 1.0;
    float UIStep = 0.01;
> = 0.25;

bool TruthUseEnbBloom
<
    string UIName = "[Truth 02] Optical | ENB Bloom";
> = true;

bool TruthUseEnbLens
<
    string UIName = "[Truth 02] Optical | ENB Lens";
> = false;

#endif
