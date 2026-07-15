#ifndef TRUTH_COLOR_CORE_FXH
#define TRUTH_COLOR_CORE_FXH

static const float TruthMiddleGray = 0.18;
static const float TruthLuminanceFloor = 0.0001;
static const float TruthMinimumExposureEv = -16.0;
static const float TruthMaximumExposureEv = 16.0;
static const float TruthBrightenRateEvPerSecond = 3.0;
static const float TruthDarkenRateEvPerSecond = 1.5;
static const float TruthFilmicLinearWhite = 4.0;

struct TruthAtmosphereSample
{
    float scene_luminance;
    float sky_luminance;
    float interior_factor;
    float delta_seconds;
    float discontinuity;
};

struct TruthMasterLookState
{
    float exposure_ev;
    float target_exposure_ev;
    uint history_epoch;
    uint validity;
};

float TruthUnifiedLuminance(TruthAtmosphereSample sample)
{
    float exterior_luminance = (0.75 * sample.scene_luminance) + (0.25 * sample.sky_luminance);
    return lerp(exterior_luminance, sample.scene_luminance, sample.interior_factor);
}

float TruthTargetExposureEv(TruthAtmosphereSample sample)
{
    float metered_luminance = max(TruthUnifiedLuminance(sample), TruthLuminanceFloor);
    return clamp(log2(TruthMiddleGray / metered_luminance),
                 TruthMinimumExposureEv,
                 TruthMaximumExposureEv);
}

float TruthAdaptExposureEv(float exposure_ev, float target_exposure_ev, float delta_seconds)
{
    float difference_ev = target_exposure_ev - exposure_ev;
    float minimum_step_ev = -TruthDarkenRateEvPerSecond * delta_seconds;
    float maximum_step_ev = TruthBrightenRateEvPerSecond * delta_seconds;
    return clamp(exposure_ev + clamp(difference_ev, minimum_step_ev, maximum_step_ev),
                 TruthMinimumExposureEv,
                 TruthMaximumExposureEv);
}

float3 TruthApplyExposure(float3 linear_color, float exposure_ev)
{
    return linear_color * exp2(exposure_ev);
}

float TruthFilmicToneCurve(float linear_value)
{
    if (!(linear_value > 0.0))
    {
        return 0.0;
    }
    if (linear_value >= TruthFilmicLinearWhite)
    {
        return 1.0;
    }

    float white_squared = TruthFilmicLinearWhite * TruthFilmicLinearWhite;
    return (linear_value * (1.0 + (linear_value / white_squared))) / (1.0 + linear_value);
}

float3 TruthFilmicToneCurve3(float3 linear_color)
{
    return float3(TruthFilmicToneCurve(linear_color.r),
                  TruthFilmicToneCurve(linear_color.g),
                  TruthFilmicToneCurve(linear_color.b));
}

#endif
