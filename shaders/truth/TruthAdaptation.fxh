#ifndef TRUTH_ADAPTATION_FXH
#define TRUTH_ADAPTATION_FXH

static const float TruthAdaptationLuminanceFloor = 0.0001;
static const float TruthAdaptationMaximumLuminance = 65504.0;
static const float TruthAdaptationFallbackDeltaSeconds = 1.0 / 60.0;
static const float TruthAdaptationMinimumDeltaSeconds = 1.0 / 240.0;
static const float TruthAdaptationMaximumDeltaSeconds = 0.25;

bool TruthAdaptationFinite3(float3 value)
{
    return all((asuint(value) & 0x7fffffffu) < 0x7f800000u.xxx);
}

float TruthAdaptationSafeLuminance(float value, float fallback)
{
    float selected = TruthFinite1(value) ? value : fallback;
    if (!TruthFinite1(selected))
    {
        selected = TruthAdaptationLuminanceFloor;
    }
    return clamp(
        selected,
        TruthAdaptationLuminanceFloor,
        TruthAdaptationMaximumLuminance);
}

float3 TruthAdaptationSafeMeasuredColor(float3 color, float fallback_luminance)
{
    float safe_fallback = TruthAdaptationSafeLuminance(
        fallback_luminance, TruthAdaptationLuminanceFloor);
    return TruthAdaptationFinite3(color)
        ? max(color, 0.0.xxx)
        : safe_fallback.xxx;
}

float TruthAdaptationDeltaSeconds(float timer_delta_seconds)
{
    return TruthFinite1(timer_delta_seconds) && timer_delta_seconds > 0.0
        ? clamp(
              timer_delta_seconds,
              TruthAdaptationMinimumDeltaSeconds,
              TruthAdaptationMaximumDeltaSeconds)
        : TruthAdaptationFallbackDeltaSeconds;
}

float TruthUpdateAdaptedLuminance(
    float measured,
    float history,
    float delta_seconds)
{
    float safe_measured = TruthAdaptationSafeLuminance(
        measured,
        TruthFinite1(history) ? history : TruthAdaptationLuminanceFloor);
    if (TruthAdaptationIntensity <= 0.0 || !TruthFinite1(history))
    {
        return safe_measured;
    }

    float safe_history = TruthAdaptationSafeLuminance(history, safe_measured);
    float measured_ev = log2(safe_measured);
    float history_ev = log2(safe_history);
    float delta_ev = measured_ev - history_ev;
    float rate = delta_ev >= 0.0 ? 3.0 : 1.5;
    float safe_delta_seconds = TruthAdaptationDeltaSeconds(delta_seconds);
    float bounded_delta = clamp(
        delta_ev,
        -rate * safe_delta_seconds,
        rate * safe_delta_seconds);
    return exp2(history_ev
        + (bounded_delta * saturate(TruthAdaptationIntensity)));
}

#endif
