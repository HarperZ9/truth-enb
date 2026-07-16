// WARP parity probe for TruthInteriorLight.fxh. Each thread rebuilds one
// canonical case identically to the C++ harness, evaluates the model on the
// GPU, and writes the four outputs. The C++ side evaluates the same cases with
// the CPU reference and asserts field-by-field agreement.

#include "TruthInteriorLight.fxh"

RWStructuredBuffer<float4> TruthInteriorLightResults : register(u0);

static const uint kTruthInteriorLightCaseCount = 5;

TruthInteriorLightInput TruthInteriorLightCase(uint index)
{
    TruthInteriorLightInput input = (TruthInteriorLightInput)0;
    input.exterior_sky_luminance = 100.0;
    input.ambient_floor = 2.0;
    input.occlusion = 0.0;
    input.aperture_count = 1;
    input.apertures[0].sky_visibility = 1.0;
    input.apertures[0].transmittance = 1.0;

    if (index == 1)
    {
        // Basement: no aperture.
        input.aperture_count = 0;
    }
    else if (index == 2)
    {
        // Sealed windowed cell: full occlusion.
        input.occlusion = 1.0;
    }
    else if (index == 3)
    {
        // Partial aperture and occlusion.
        input.ambient_floor = 1.0;
        input.occlusion = 0.25;
        input.apertures[0].sky_visibility = 0.5;
        input.apertures[0].transmittance = 0.8;
    }
    else if (index == 4)
    {
        // Over-unity aperture sum must clamp.
        input.exterior_sky_luminance = 50.0;
        input.ambient_floor = 0.0;
        input.aperture_count = 2;
        input.apertures[0].sky_visibility = 1.0;
        input.apertures[0].transmittance = 1.0;
        input.apertures[1].sky_visibility = 1.0;
        input.apertures[1].transmittance = 1.0;
    }

    return input;
}

[numthreads(kTruthInteriorLightCaseCount, 1, 1)]
void TruthInteriorLightWarpProbeMain(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint index = dispatch_thread_id.x;
    if (index >= kTruthInteriorLightCaseCount)
    {
        return;
    }

    TruthInteriorLightOutput output = TruthEvaluateInteriorLight(TruthInteriorLightCase(index));
    TruthInteriorLightResults[index] = float4(
        output.interior_light,
        output.exterior_daylight,
        output.effective_aperture,
        output.exterior_excluded);
}
