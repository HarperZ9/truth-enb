#ifndef TRUTH_QUALITY_FXH
#define TRUTH_QUALITY_FXH

#ifndef TRUTH_QUALITY_TIER
#define TRUTH_QUALITY_TIER 1
#endif

#if TRUTH_QUALITY_TIER < 0 || TRUTH_QUALITY_TIER > 4
#error TRUTH_QUALITY_TIER must be in [0,4]
#endif

static const uint TruthQualityTier = TRUTH_QUALITY_TIER;

#if TRUTH_QUALITY_TIER == 0
static const uint TruthQualityCloudPrimarySteps = 0u;
static const uint TruthQualityCloudLightSteps = 0u;
static const uint TruthQualityAuroraSamples = 1u;
static const uint TruthQualityAODirections = 4u;
static const uint TruthQualityAOSteps = 2u;
static const uint TruthQualityDOFRings = 0u;
static const uint TruthQualityBloomRadius = 2u;
static const uint TruthQualitySSRSteps = 0u;
static const uint TruthQualityUsesVolumeClouds = 0u;
#elif TRUTH_QUALITY_TIER == 1
static const uint TruthQualityCloudPrimarySteps = 0u;
static const uint TruthQualityCloudLightSteps = 0u;
static const uint TruthQualityAuroraSamples = 2u;
static const uint TruthQualityAODirections = 6u;
static const uint TruthQualityAOSteps = 3u;
static const uint TruthQualityDOFRings = 2u;
static const uint TruthQualityBloomRadius = 3u;
static const uint TruthQualitySSRSteps = 0u;
static const uint TruthQualityUsesVolumeClouds = 0u;
#elif TRUTH_QUALITY_TIER == 2
static const uint TruthQualityCloudPrimarySteps = 8u;
static const uint TruthQualityCloudLightSteps = 2u;
static const uint TruthQualityAuroraSamples = 4u;
static const uint TruthQualityAODirections = 8u;
static const uint TruthQualityAOSteps = 4u;
static const uint TruthQualityDOFRings = 3u;
static const uint TruthQualityBloomRadius = 4u;
static const uint TruthQualitySSRSteps = 8u;
static const uint TruthQualityUsesVolumeClouds = 1u;
#elif TRUTH_QUALITY_TIER == 3
static const uint TruthQualityCloudPrimarySteps = 12u;
static const uint TruthQualityCloudLightSteps = 3u;
static const uint TruthQualityAuroraSamples = 7u;
static const uint TruthQualityAODirections = 12u;
static const uint TruthQualityAOSteps = 5u;
static const uint TruthQualityDOFRings = 4u;
static const uint TruthQualityBloomRadius = 5u;
static const uint TruthQualitySSRSteps = 12u;
static const uint TruthQualityUsesVolumeClouds = 1u;
#else
static const uint TruthQualityCloudPrimarySteps = 16u;
static const uint TruthQualityCloudLightSteps = 4u;
static const uint TruthQualityAuroraSamples = 10u;
static const uint TruthQualityAODirections = 16u;
static const uint TruthQualityAOSteps = 6u;
static const uint TruthQualityDOFRings = 5u;
static const uint TruthQualityBloomRadius = 6u;
static const uint TruthQualitySSRSteps = 16u;
static const uint TruthQualityUsesVolumeClouds = 1u;
#endif

#endif
