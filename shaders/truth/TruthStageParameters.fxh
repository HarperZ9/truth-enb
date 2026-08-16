#ifndef TRUTH_STAGE_PARAMETERS_FXH
#define TRUTH_STAGE_PARAMETERS_FXH

#ifndef TRUTH_STAGE_PARAMETER_SLOT
#error Truth stage must select its parameter slot before including TruthStageParameters.fxh
#endif

#if TRUTH_STAGE_PARAMETER_SLOT == 0
bool TruthPrepassEnabled <string UIName = "[Truth 10] Prepass | Enabled";> = true;
float TruthPrepassIntensity <string UIName = "[Truth 10] Prepass | Intensity"; string UIWidget = "Spinner"; float UIMin = 0.0; float UIMax = 1.0; float UIStep = 0.01;> = 0.52;
float TruthPrepassDepthShape <string UIName = "[Truth 10] Prepass | Depth Shape"; string UIWidget = "Spinner"; float UIMin = 0.0; float UIMax = 1.0; float UIStep = 0.01;> = 0.50;
#define TRUTH_STAGE_ENABLED TruthPrepassEnabled
#define TRUTH_STAGE_INTENSITY TruthPrepassIntensity
#elif TRUTH_STAGE_PARAMETER_SLOT == 1
bool TruthDepthOfFieldEnabled <string UIName = "[Truth 20] Depth of Field | Enabled";> = false;
float TruthDepthOfFieldIntensity <string UIName = "[Truth 20] Depth of Field | Intensity"; string UIWidget = "Spinner"; float UIMin = 0.0; float UIMax = 1.0; float UIStep = 0.01;> = 0.12;
float TruthDepthOfFieldFocusShape <string UIName = "[Truth 20] Depth of Field | Focus Shape"; string UIWidget = "Spinner"; float UIMin = 0.0; float UIMax = 1.0; float UIStep = 0.01;> = 0.50;
#define TRUTH_STAGE_ENABLED TruthDepthOfFieldEnabled
#define TRUTH_STAGE_INTENSITY TruthDepthOfFieldIntensity
#elif TRUTH_STAGE_PARAMETER_SLOT == 2
bool TruthBloomEnabled <string UIName = "[Truth 30] Bloom | Enabled";> = true;
float TruthBloomIntensity <string UIName = "[Truth 30] Bloom | Intensity"; string UIWidget = "Spinner"; float UIMin = 0.0; float UIMax = 1.0; float UIStep = 0.01;> = 0.20;
float TruthBloomThresholdShape <string UIName = "[Truth 30] Bloom | Threshold Shape"; string UIWidget = "Spinner"; float UIMin = 0.0; float UIMax = 4.0; float UIStep = 0.01;> = 1.15;
#define TRUTH_STAGE_ENABLED TruthBloomEnabled
#define TRUTH_STAGE_INTENSITY TruthBloomIntensity
#elif TRUTH_STAGE_PARAMETER_SLOT == 3
bool TruthAdaptationEnabled <string UIName = "[Truth 40] Adaptation | Enabled";> = true;
float TruthAdaptationIntensity <string UIName = "[Truth 40] Adaptation | Intensity"; string UIWidget = "Spinner"; float UIMin = 0.0; float UIMax = 1.0; float UIStep = 0.01;> = 0.50;
float TruthAdaptationResponseShape <string UIName = "[Truth 40] Adaptation | Response Shape"; string UIWidget = "Spinner"; float UIMin = 0.0; float UIMax = 1.0; float UIStep = 0.01;> = 0.50;
#define TRUTH_STAGE_ENABLED TruthAdaptationEnabled
#define TRUTH_STAGE_INTENSITY TruthAdaptationIntensity
#elif TRUTH_STAGE_PARAMETER_SLOT == 4
bool TruthLensEnabled <string UIName = "[Truth 50] Lens | Enabled";> = false;
float TruthLensIntensity <string UIName = "[Truth 50] Lens | Intensity"; string UIWidget = "Spinner"; float UIMin = 0.0; float UIMax = 1.0; float UIStep = 0.01;> = 0.08;
float TruthLensApertureShape <string UIName = "[Truth 50] Lens | Aperture Shape"; string UIWidget = "Spinner"; float UIMin = 0.0; float UIMax = 1.0; float UIStep = 0.01;> = 0.45;
#define TRUTH_STAGE_ENABLED TruthLensEnabled
#define TRUTH_STAGE_INTENSITY TruthLensIntensity
#elif TRUTH_STAGE_PARAMETER_SLOT == 6
bool TruthPostpassEnabled <string UIName = "[Truth 70] Postpass | Enabled";> = true;
float TruthPostpassIntensity <string UIName = "[Truth 70] Postpass | Intensity"; string UIWidget = "Spinner"; float UIMin = 0.0; float UIMax = 1.0; float UIStep = 0.01;> = 0.45;
float TruthPostpassGrainShape <string UIName = "[Truth 70] Postpass | Grain Shape"; string UIWidget = "Spinner"; float UIMin = 0.0; float UIMax = 1.0; float UIStep = 0.01;> = 0.0;
float TruthPostpassVignetteStrength <string UIName = "[Truth 70] Postpass | Vignette Strength"; string UIWidget = "Spinner"; float UIMin = 0.0; float UIMax = 1.0; float UIStep = 0.01;> = 0.18;
#define TRUTH_STAGE_ENABLED TruthPostpassEnabled
#define TRUTH_STAGE_INTENSITY TruthPostpassIntensity
#elif TRUTH_STAGE_PARAMETER_SLOT == 7
bool TruthSunSpriteEnabled <string UIName = "[Truth 80] Sun Sprite | Enabled";> = true;
float TruthSunSpriteIntensity <string UIName = "[Truth 80] Sun Sprite | Intensity"; string UIWidget = "Spinner"; float UIMin = 0.0; float UIMax = 1.0; float UIStep = 0.01;> = 0.18;
float TruthSunSpriteDiscShape <string UIName = "[Truth 80] Sun Sprite | Disc Shape"; string UIWidget = "Spinner"; float UIMin = 0.0; float UIMax = 1.0; float UIStep = 0.01;> = 0.50;
#define TRUTH_STAGE_ENABLED TruthSunSpriteEnabled
#define TRUTH_STAGE_INTENSITY TruthSunSpriteIntensity
#elif TRUTH_STAGE_PARAMETER_SLOT == 8
bool TruthUnderwaterEnabled <string UIName = "[Truth 90] Underwater | Enabled";> = true;
float TruthUnderwaterIntensity <string UIName = "[Truth 90] Underwater | Intensity"; string UIWidget = "Spinner"; float UIMin = 0.0; float UIMax = 1.0; float UIStep = 0.01;> = 0.32;
float TruthUnderwaterDensityShape <string UIName = "[Truth 90] Underwater | Density Shape"; string UIWidget = "Spinner"; float UIMin = 0.0; float UIMax = 1.0; float UIStep = 0.01;> = 0.25;
#define TRUTH_STAGE_ENABLED TruthUnderwaterEnabled
#define TRUTH_STAGE_INTENSITY TruthUnderwaterIntensity
#else
#error Truth stage parameter slot must be one of 0-4 or 6-8; main reuses its ABI-bound controls
#endif

bool TruthStageIsActive()
{
    return TRUTH_STAGE_ENABLED && TRUTH_STAGE_INTENSITY > 0.0;
}

#endif
