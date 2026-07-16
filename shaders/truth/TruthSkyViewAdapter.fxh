#ifndef TRUTH_SKY_VIEW_ADAPTER_FXH
#define TRUTH_SKY_VIEW_ADAPTER_FXH

static const float TruthSkyViewMinimumWorldScale = 0.0001;
static const float TruthSkyViewMaximumWorldScale = 1000000.0;
static const float TruthSkyViewMinimumTexcoord = 0.0;
static const float TruthSkyViewMaximumTexcoord = 1.0;
static const float TruthSkyViewMaximumWorldCoordinate = 100000000.0;
static const float TruthSkyViewMinimumHomogeneousW = 0.000001;
static const float TruthSkyViewMinimumDirectionLengthSquared = 0.00000001;
static const float TruthSkyViewMaximumAuroraCoordinate = 4096.0;

bool TruthSkyViewFinite1(float value)
{
    return (asuint(value) & 0x7fffffffu) < 0x7f800000u;
}

bool TruthSkyViewFinite3(float3 value)
{
    return all((asuint(value) & 0x7fffffffu) < 0x7f800000u);
}

bool TruthSkyViewFinite4(float4 value)
{
    return all((asuint(value) & 0x7fffffffu) < 0x7f800000u);
}

struct TruthSkyViewAdapterInput
{
    float2 texcoord;
    row_major float4x4 inverse_view_projection;
    float3 camera_world_position;
    float3 aurora_world_origin;
    float engine_world_units_per_aurora_unit;
};

struct TruthSkyViewAdapterOutput
{
    float3 view_world_direction;
    float3 camera_aurora_position;
    float valid;
};

TruthSkyViewAdapterOutput TruthEvaluateSkyViewAdapter(
    TruthSkyViewAdapterInput input)
{
    TruthSkyViewAdapterOutput output;
    output.view_world_direction = float3(0.0, 0.0, 1.0);
    output.camera_aurora_position = float3(0.0, 0.0, 0.0);
    output.valid = 0.0;
    // Every input is range-gated here and then finite-gated at its first
    // arithmetic result below. This catches NaN/Inf without paying for a
    // second full matrix scan in the production pixel shader.
    if (any(input.texcoord < TruthSkyViewMinimumTexcoord)
        || any(input.texcoord > TruthSkyViewMaximumTexcoord)
        || any(abs(input.camera_world_position)
               > TruthSkyViewMaximumWorldCoordinate)
        || any(abs(input.aurora_world_origin)
               > TruthSkyViewMaximumWorldCoordinate)
        || input.engine_world_units_per_aurora_unit < TruthSkyViewMinimumWorldScale
        || input.engine_world_units_per_aurora_unit > TruthSkyViewMaximumWorldScale)
    {
        return output;
    }

    float4 clip_position = float4(
        (2.0 * input.texcoord.x) - 1.0,
        1.0 - (2.0 * input.texcoord.y),
        1.0,
        1.0);
    float4 world_position = mul(input.inverse_view_projection, clip_position);
    if (!TruthSkyViewFinite4(world_position)
        || abs(world_position.w) < TruthSkyViewMinimumHomogeneousW)
    {
        return output;
    }

    float3 world_far = world_position.xyz / world_position.w;
    float3 world_ray = world_far - input.camera_world_position;
    float direction_length_squared = dot(world_ray, world_ray);
    if (!TruthSkyViewFinite3(world_ray)
        || !TruthSkyViewFinite1(direction_length_squared)
        || direction_length_squared < TruthSkyViewMinimumDirectionLengthSquared)
    {
        return output;
    }

    float3 camera_aurora =
        (input.camera_world_position - input.aurora_world_origin)
        / input.engine_world_units_per_aurora_unit;
    float3 view_world_direction = world_ray * rsqrt(direction_length_squared);
    if (!TruthSkyViewFinite3(camera_aurora)
        || any(abs(camera_aurora) > TruthSkyViewMaximumAuroraCoordinate)
        || !TruthSkyViewFinite3(view_world_direction))
    {
        return output;
    }

    output.view_world_direction = view_world_direction;
    output.camera_aurora_position = camera_aurora;
    output.valid = 1.0;
    return output;
}

#endif
