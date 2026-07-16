// FXC compile witness for TruthInteriorLight.fxh. Compiled as fx_5_0 with
// warnings-as-errors so the shader mirror is proven to build, and the technique
// forces every field of the model to be consumed so nothing dead-strips.

#include "TruthInteriorLight.fxh"

float4 TruthInteriorLightProbePixel(float4 position : SV_Position) : SV_Target
{
    TruthInteriorLightInput input = (TruthInteriorLightInput)0;
    input.exterior_sky_luminance = 100.0;
    input.ambient_floor = 2.0;
    input.occlusion = 0.0;
    input.aperture_count = 1;
    input.apertures[0].sky_visibility = 1.0;
    input.apertures[0].transmittance = 1.0;

    TruthInteriorLightOutput output = TruthEvaluateInteriorLight(input);

    return float4(output.interior_light,
                  output.exterior_daylight,
                  output.effective_aperture,
                  output.exterior_excluded);
}

technique11 TruthInteriorLightProbe
{
    pass P0
    {
        SetPixelShader(CompileShader(ps_5_0, TruthInteriorLightProbePixel()));
    }
}
