#ifndef TRUTH_SUN_SPRITE_FXH
#define TRUTH_SUN_SPRITE_FXH

float3 TruthEvaluateSunSprite(
    float2 uv,
    float3 sun_direction,
    float visibility)
{
    float direction_length = length(sun_direction);
    if (TruthSunSpriteIntensity <= 0.0
        || visibility <= 0.0
        || direction_length <= 0.0001)
    {
        return 0.0;
    }

    float3 direction = sun_direction / direction_length;
    float2 sun_uv = 0.5 + (direction.xy * 0.45);
    float radius = lerp(0.006, 0.018, saturate(TruthSunSpriteDiscShape));
    float distance_to_disc = length(uv - sun_uv);
    float disc = 1.0 - smoothstep(radius * 0.70, radius, distance_to_disc);
    float halo = exp2(-distance_to_disc * (48.0 / max(radius, 0.0001)));
    float3 sun_color = float3(1.0, 0.72, 0.42);
    return sun_color * (disc + 0.08 * halo)
        * saturate(visibility)
        * saturate(TruthSunSpriteIntensity);
}

#endif
