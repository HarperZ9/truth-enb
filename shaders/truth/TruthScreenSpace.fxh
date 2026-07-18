#ifndef TRUTH_SCREEN_SPACE_FXH
#define TRUTH_SCREEN_SPACE_FXH

#ifndef TRUTH_PIPELINE_COMMON_FXH
#error Include TruthHostCapabilities.fxh and TruthPipelineCommon.fxh before TruthScreenSpace.fxh
#endif

#include "TruthQuality.fxh"

static const float TruthScreenSpaceTwoPi = 6.28318530717958647692;
static const float TruthScreenSpaceMinimumDepth = 0.000001;
static const float TruthScreenSpaceMinimumNormalLength = 0.0001;
static const float TruthScreenSpaceMaximumRadiusPixels = 48.0;

struct TruthScreenSpaceInput
{
    float2 texel_size;
    float3 view_direction;
    float raw_depth;
    float native_normal_valid;
    float skin_mask_valid;
    float sky_depth_threshold;
    float ao_intensity;
    float contact_intensity;
    float ao_radius_pixels;
    float ssr_intensity;
    float ssr_max_distance;
    float ssr_thickness;
    float diffusion_intensity;
    float diffusion_radius_pixels;
};

bool TruthScreenSpaceFinite3(float3 value)
{
    return all((asuint(value) & 0x7fffffffu) < 0x7f800000u.xxx);
}

bool TruthScreenSpaceGeometryValid(float raw_depth, float sky_depth_threshold)
{
    return TruthFinite1(raw_depth)
        && raw_depth >= 0.0
        && raw_depth < clamp(sky_depth_threshold, 0.99, 1.0);
}

float TruthScreenSpaceSampleDepth(
    Texture2D depth_texture,
    SamplerState screen_sampler,
    float2 texcoord)
{
    return depth_texture.SampleLevel(screen_sampler, saturate(texcoord), 0.0).r;
}

float TruthScreenSpaceViewDepth(float raw_depth)
{
    return max(1.0 - saturate(raw_depth), TruthScreenSpaceMinimumDepth);
}

float3 TruthScreenSpaceNormalizeOrForward(float3 value)
{
    float length_squared = dot(value, value);
    if (!TruthScreenSpaceFinite3(value)
        || length_squared < TruthScreenSpaceMinimumNormalLength)
    {
        return float3(0.0, 0.0, 1.0);
    }
    return value * rsqrt(length_squared);
}

float3 TruthScreenSpaceDepthNormal(
    Texture2D depth_texture,
    SamplerState screen_sampler,
    float2 texcoord,
    float2 texel_size)
{
    float center = TruthScreenSpaceViewDepth(
        TruthScreenSpaceSampleDepth(depth_texture, screen_sampler, texcoord));
    float left = TruthScreenSpaceViewDepth(TruthScreenSpaceSampleDepth(
        depth_texture, screen_sampler, texcoord - float2(texel_size.x, 0.0)));
    float right = TruthScreenSpaceViewDepth(TruthScreenSpaceSampleDepth(
        depth_texture, screen_sampler, texcoord + float2(texel_size.x, 0.0)));
    float up = TruthScreenSpaceViewDepth(TruthScreenSpaceSampleDepth(
        depth_texture, screen_sampler, texcoord - float2(0.0, texel_size.y)));
    float down = TruthScreenSpaceViewDepth(TruthScreenSpaceSampleDepth(
        depth_texture, screen_sampler, texcoord + float2(0.0, texel_size.y)));
    float3 normal = float3(left - right, up - down, max(center * 2.0, 0.001));
    return TruthScreenSpaceNormalizeOrForward(normal);
}

float3 TruthScreenSpaceNormal(
    Texture2D depth_texture,
    Texture2D normal_texture,
    SamplerState screen_sampler,
    TruthScreenSpaceInput input,
    float2 texcoord)
{
    float3 native_normal = (normal_texture.SampleLevel(
        screen_sampler, saturate(texcoord), 0.0).xyz * 2.0) - 1.0;
    float native_length_squared = dot(native_normal, native_normal);
    if (input.native_normal_valid > 0.5
        && TruthScreenSpaceFinite3(native_normal)
        && native_length_squared >= TruthScreenSpaceMinimumNormalLength)
    {
        return native_normal * rsqrt(native_length_squared);
    }
    return TruthScreenSpaceDepthNormal(
        depth_texture, screen_sampler, texcoord, input.texel_size);
}

float TruthScreenSpaceBilateralWeight(
    float center_raw_depth,
    float sample_raw_depth,
    float3 center_normal,
    float3 sample_normal)
{
    if (!TruthFinite1(sample_raw_depth))
    {
        return 0.0;
    }
    float depth_delta = abs(
        TruthScreenSpaceViewDepth(center_raw_depth)
        - TruthScreenSpaceViewDepth(sample_raw_depth));
    float depth_weight = saturate(1.0 - (depth_delta * 96.0));
    float normal_weight = saturate(dot(center_normal, sample_normal));
    return depth_weight * normal_weight * normal_weight;
}

float TruthScreenSpaceEdgeConfidence(
    Texture2D depth_texture,
    SamplerState screen_sampler,
    TruthScreenSpaceInput input,
    float2 texcoord)
{
    float center_depth = TruthScreenSpaceViewDepth(input.raw_depth);
    float left_depth = TruthScreenSpaceViewDepth(TruthScreenSpaceSampleDepth(
        depth_texture, screen_sampler, texcoord - float2(input.texel_size.x, 0.0)));
    float right_depth = TruthScreenSpaceViewDepth(TruthScreenSpaceSampleDepth(
        depth_texture, screen_sampler, texcoord + float2(input.texel_size.x, 0.0)));
    float up_depth = TruthScreenSpaceViewDepth(TruthScreenSpaceSampleDepth(
        depth_texture, screen_sampler, texcoord - float2(0.0, input.texel_size.y)));
    float down_depth = TruthScreenSpaceViewDepth(TruthScreenSpaceSampleDepth(
        depth_texture, screen_sampler, texcoord + float2(0.0, input.texel_size.y)));
    float discontinuity = max(max(abs(center_depth - left_depth), abs(center_depth - right_depth)),
                              max(abs(center_depth - up_depth), abs(center_depth - down_depth)));
    return saturate(1.0 - (discontinuity * 128.0));
}

float3 TruthApplyGTAO(
    float3 scene,
    Texture2D depth_texture,
    Texture2D normal_texture,
    SamplerState screen_sampler,
    TruthScreenSpaceInput input,
    float2 texcoord)
{
    if (input.ao_intensity <= 0.0
        || !TruthScreenSpaceGeometryValid(input.raw_depth, input.sky_depth_threshold))
    {
        return scene;
    }

    float3 center_normal = TruthScreenSpaceNormal(
        depth_texture, normal_texture, screen_sampler, input, texcoord);
    float center_depth = TruthScreenSpaceViewDepth(input.raw_depth);
    float radius = clamp(input.ao_radius_pixels, 0.0, TruthScreenSpaceMaximumRadiusPixels);
    float occlusion = 0.0;
    float sample_count = 0.0;

    [loop]
    for (uint direction_index = 0u;
         direction_index < TruthQualityAODirections;
         ++direction_index)
    {
        float angle = TruthScreenSpaceTwoPi
            * (float(direction_index) / float(TruthQualityAODirections));
        float2 direction = float2(cos(angle), sin(angle));
        [loop]
        for (uint step_index = 0u;
             step_index < TruthQualityAOSteps;
             ++step_index)
        {
            float step_fraction = (float(step_index) + 1.0)
                / float(TruthQualityAOSteps);
            float2 sample_uv = texcoord
                + (direction * input.texel_size * radius * step_fraction);
            float sample_raw_depth = TruthScreenSpaceSampleDepth(
                depth_texture, screen_sampler, sample_uv);
            if (!TruthScreenSpaceGeometryValid(
                    sample_raw_depth, input.sky_depth_threshold))
            {
                continue;
            }

            float3 sample_normal = TruthScreenSpaceNormal(
                depth_texture, normal_texture, screen_sampler, input, sample_uv);
            float sample_depth = TruthScreenSpaceViewDepth(sample_raw_depth);
            float projected_occluder = max(sample_depth - center_depth, 0.0);
            float horizon = saturate(projected_occluder / max(step_fraction * radius * 0.03, 0.001));
            float bilateral = TruthScreenSpaceBilateralWeight(
                input.raw_depth, sample_raw_depth, center_normal, sample_normal);
            occlusion += horizon * bilateral;
            sample_count += 1.0;
        }
    }

    if (occlusion <= 0.0 || sample_count <= 0.0)
    {
        return scene;
    }

    float contact_scale = 1.0 + (saturate(input.contact_intensity) * 0.5);
    float visibility = 1.0 - saturate(
        (occlusion / sample_count) * saturate(input.ao_intensity) * contact_scale);
    if (visibility >= 1.0)
    {
        return scene;
    }
    return scene * visibility;
}

float3 TruthApplySSR(
    float3 scene,
    Texture2D scene_texture,
    Texture2D depth_texture,
    Texture2D normal_texture,
    SamplerState screen_sampler,
    TruthScreenSpaceInput input,
    float2 texcoord)
{
#if TRUTH_QUALITY_TIER <= 1
    return scene;
#else
    if (input.ssr_intensity <= 0.0
        || input.ssr_max_distance <= 0.0
        || input.ssr_thickness <= 0.0
        || !TruthScreenSpaceGeometryValid(input.raw_depth, input.sky_depth_threshold))
    {
        return scene;
    }

    float3 normal = TruthScreenSpaceNormal(
        depth_texture, normal_texture, screen_sampler, input, texcoord);
    float3 view_direction = TruthScreenSpaceNormalizeOrForward(input.view_direction);
    float3 reflection_direction = reflect(view_direction, normal);
    float reflection_xy_length = dot(reflection_direction.xy, reflection_direction.xy);
    if (reflection_xy_length < 0.000001)
    {
        return scene;
    }

    float2 ray_uv_direction = reflection_direction.xy * rsqrt(reflection_xy_length);
    float center_depth = TruthScreenSpaceViewDepth(input.raw_depth);
    float previous_distance = 0.0;
    float hit_distance = 0.0;
    bool hit = false;

    [loop]
    for (uint step_index = 0u;
         step_index < TruthQualitySSRSteps;
         ++step_index)
    {
        float distance = (float(step_index) + 1.0)
            * (input.ssr_max_distance / float(TruthQualitySSRSteps));
        float2 sample_uv = texcoord
            + (ray_uv_direction * input.texel_size * distance);
        if (any(sample_uv <= 0.0.xx) || any(sample_uv >= 1.0.xx))
        {
            break;
        }

        float sample_raw_depth = TruthScreenSpaceSampleDepth(
            depth_texture, screen_sampler, sample_uv);
        if (TruthScreenSpaceGeometryValid(sample_raw_depth, input.sky_depth_threshold))
        {
            float ray_depth = center_depth
                + (max(reflection_direction.z, 0.0) * distance * 0.01);
            float depth_difference = abs(
                TruthScreenSpaceViewDepth(sample_raw_depth) - ray_depth);
            if (depth_difference <= input.ssr_thickness)
            {
                hit_distance = distance;
                hit = true;
                break;
            }
        }
        previous_distance = distance;
    }

    if (!hit)
    {
        return scene;
    }

    float refinement_low = previous_distance;
    float refinement_high = hit_distance;
    [unroll]
    for (uint refinement_index = 0u; refinement_index < 3u; ++refinement_index)
    {
        float refinement_distance = 0.5 * (refinement_low + refinement_high);
        float2 refinement_uv = texcoord
            + (ray_uv_direction * input.texel_size * refinement_distance);
        float refinement_raw_depth = TruthScreenSpaceSampleDepth(
            depth_texture, screen_sampler, refinement_uv);
        float refinement_ray_depth = center_depth
            + (max(reflection_direction.z, 0.0) * refinement_distance * 0.01);
        float refinement_delta = TruthScreenSpaceViewDepth(refinement_raw_depth)
            - refinement_ray_depth;
        if (refinement_delta > 0.0)
        {
            refinement_high = refinement_distance;
        }
        else
        {
            refinement_low = refinement_distance;
        }
    }

    float2 hit_uv = texcoord
        + (ray_uv_direction * input.texel_size * (0.5 * (refinement_low + refinement_high)));
    float hit_raw_depth = TruthScreenSpaceSampleDepth(depth_texture, screen_sampler, hit_uv);
    float hit_ray_depth = center_depth
        + (max(reflection_direction.z, 0.0)
           * (0.5 * (refinement_low + refinement_high)) * 0.01);
    float thickness_confidence = saturate(1.0 - (abs(
        TruthScreenSpaceViewDepth(hit_raw_depth) - hit_ray_depth)
        / max(input.ssr_thickness, TruthScreenSpaceMinimumDepth)));
    float edge_confidence = TruthScreenSpaceEdgeConfidence(
        depth_texture, screen_sampler, input, texcoord)
        * TruthScreenSpaceEdgeConfidence(
            depth_texture, screen_sampler, input, hit_uv);
    float confidence = saturate(input.ssr_intensity)
        * thickness_confidence * edge_confidence;
    if (confidence <= 0.0)
    {
        return scene;
    }
    float3 reflection = scene_texture.SampleLevel(screen_sampler, hit_uv, 0.0).rgb;
    return lerp(scene, reflection, confidence);
#endif
}

float3 TruthApplySkinDiffusion(
    float3 scene,
    Texture2D scene_texture,
    Texture2D depth_texture,
    Texture2D normal_texture,
    Texture2D material_mask_texture,
    SamplerState screen_sampler,
    TruthScreenSpaceInput input,
    float2 texcoord)
{
    if (input.diffusion_intensity <= 0.0
        || input.skin_mask_valid <= 0.5
        || !TruthScreenSpaceGeometryValid(input.raw_depth, input.sky_depth_threshold))
    {
        return scene;
    }

    float skin_mask = material_mask_texture.SampleLevel(
        screen_sampler, saturate(texcoord), 0.0).r;
    if (skin_mask <= 0.0)
    {
        return scene;
    }

    float3 center_normal = TruthScreenSpaceNormal(
        depth_texture, normal_texture, screen_sampler, input, texcoord);
    float radius = clamp(
        input.diffusion_radius_pixels, 0.0, TruthScreenSpaceMaximumRadiusPixels);
    float3 accumulated = scene;
    float accumulated_weight = 1.0;
    const float2 offsets[4] = {
        float2(-1.0, 0.0), float2(1.0, 0.0),
        float2(0.0, -1.0), float2(0.0, 1.0)};

    [unroll]
    for (uint sample_index = 0u; sample_index < 4u; ++sample_index)
    {
        float2 sample_uv = texcoord
            + (offsets[sample_index] * input.texel_size * radius);
        float sample_mask = material_mask_texture.SampleLevel(
            screen_sampler, saturate(sample_uv), 0.0).r;
        float sample_raw_depth = TruthScreenSpaceSampleDepth(
            depth_texture, screen_sampler, sample_uv);
        if (sample_mask <= 0.0
            || !TruthScreenSpaceGeometryValid(
                sample_raw_depth, input.sky_depth_threshold))
        {
            continue;
        }
        float3 sample_normal = TruthScreenSpaceNormal(
            depth_texture, normal_texture, screen_sampler, input, sample_uv);
        float weight = TruthScreenSpaceBilateralWeight(
            input.raw_depth, sample_raw_depth, center_normal, sample_normal)
            * saturate(sample_mask);
        if (weight <= 0.0)
        {
            continue;
        }
        accumulated += scene_texture.SampleLevel(
            screen_sampler, saturate(sample_uv), 0.0).rgb * weight;
        accumulated_weight += weight;
    }

    if (accumulated_weight <= 1.0)
    {
        return scene;
    }
    float blend = saturate(input.diffusion_intensity) * saturate(skin_mask);
    if (blend <= 0.0)
    {
        return scene;
    }
    return lerp(scene, accumulated / accumulated_weight, blend);
}

float3 TruthApplyScreenSpaceEffects(
    float3 scene,
    Texture2D scene_texture,
    Texture2D depth_texture,
    Texture2D normal_texture,
    Texture2D material_mask_texture,
    SamplerState screen_sampler,
    TruthScreenSpaceInput input,
    float2 texcoord)
{
    if (!TruthScreenSpaceGeometryValid(input.raw_depth, input.sky_depth_threshold))
    {
        return scene;
    }
    float3 shaded = TruthApplyGTAO(
        scene, depth_texture, normal_texture, screen_sampler, input, texcoord);
    shaded = TruthApplySSR(
        shaded, scene_texture, depth_texture, normal_texture,
        screen_sampler, input, texcoord);
    return TruthApplySkinDiffusion(
        shaded, scene_texture, depth_texture, normal_texture, material_mask_texture,
        screen_sampler, input, texcoord);
}

#endif
