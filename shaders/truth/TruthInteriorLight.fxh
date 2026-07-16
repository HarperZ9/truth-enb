#ifndef TRUTH_INTERIOR_LIGHT_FXH
#define TRUTH_INTERIOR_LIGHT_FXH

// Shader mirror of truth::render::EvaluateInteriorLight (src/render/InteriorLight.cpp).
// Occlusion-aware interior daylight: a cell is lit by the exterior sky only
// through its window/portal apertures and only when not occluded. A windowless
// or sealed cell (basement) receives exactly zero exterior daylight and keeps
// only its ambient floor. Same bounded model, same exact-zero exclusion as the
// CPU reference; validate-then-commit is a CPU concern, so this mirror assumes
// finite in-range input and reproduces only the arithmetic.

#define TRUTH_MAX_INTERIOR_APERTURES 8
static const float TruthInteriorMaxLight = 1000000.0;

struct TruthInteriorAperture
{
    float sky_visibility;  // [0,1]
    float transmittance;   // [0,1]
};

struct TruthInteriorLightInput
{
    float exterior_sky_luminance;
    float ambient_floor;
    float occlusion;       // [0,1]; 1 = fully occluded (basement / sealed)
    uint aperture_count;   // <= TRUTH_MAX_INTERIOR_APERTURES
    TruthInteriorAperture apertures[TRUTH_MAX_INTERIOR_APERTURES];
};

struct TruthInteriorLightOutput
{
    float interior_light;
    float exterior_daylight;   // exactly 0 for basements
    float effective_aperture;  // clamped [0,1]
    float exterior_excluded;   // 1.0 when no exterior light reaches the space
};

TruthInteriorLightOutput TruthEvaluateInteriorLight(TruthInteriorLightInput input)
{
    float aperture_sum = 0.0;
    [unroll]
    for (uint i = 0; i < TRUTH_MAX_INTERIOR_APERTURES; ++i)
    {
        if (i < input.aperture_count)
        {
            aperture_sum += input.apertures[i].sky_visibility * input.apertures[i].transmittance;
        }
    }

    float effective_aperture = saturate(aperture_sum);
    float open_factor = effective_aperture * (1.0 - input.occlusion);
    float exterior_daylight = input.exterior_sky_luminance * open_factor;
    float interior_light = clamp(input.ambient_floor + exterior_daylight, 0.0, TruthInteriorMaxLight);

    TruthInteriorLightOutput output;
    output.interior_light = interior_light;
    output.exterior_daylight = exterior_daylight;
    output.effective_aperture = effective_aperture;
    output.exterior_excluded = (open_factor == 0.0) ? 1.0 : 0.0;
    return output;
}

#endif  // TRUTH_INTERIOR_LIGHT_FXH
