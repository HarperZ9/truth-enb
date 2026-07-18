#ifndef TRUTH_ADAPTATION_FXH
#define TRUTH_ADAPTATION_FXH

float TruthUpdateAdaptedLuminance(
    float measured,
    float history,
    float delta_seconds)
{
    if (TruthAdaptationIntensity <= 0.0 || !TruthFinite1(history))
    {
        return measured;
    }

    float safe_measured = clamp(measured, 0.00001, 65504.0);
    float safe_history = clamp(history, 0.00001, 65504.0);
    float measured_ev = log2(safe_measured);
    float history_ev = log2(safe_history);
    float delta_ev = measured_ev - history_ev;
    float rate = delta_ev >= 0.0 ? 3.0 : 1.5;
    float bounded_delta = clamp(
        delta_ev,
        -rate * max(delta_seconds, 0.0),
        rate * max(delta_seconds, 0.0));
    return exp2(history_ev
        + (bounded_delta * saturate(TruthAdaptationIntensity)));
}

#endif
