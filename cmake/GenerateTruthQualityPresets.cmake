cmake_minimum_required(VERSION 3.30)

foreach(required_variable IN ITEMS TRUTH_SOURCE_DIR TRUTH_BINARY_DIR)
  if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
    message(FATAL_ERROR "Missing required variable: ${required_variable}")
  endif()
endforeach()

if(NOT IS_DIRECTORY "${TRUTH_SOURCE_DIR}")
  message(FATAL_ERROR "Truth source directory is absent: ${TRUTH_SOURCE_DIR}")
endif()
if(NOT IS_DIRECTORY "${TRUTH_BINARY_DIR}")
  message(FATAL_ERROR "Truth binary directory is absent: ${TRUTH_BINARY_DIR}")
endif()

file(REAL_PATH "${TRUTH_SOURCE_DIR}" truth_source_dir)
file(REAL_PATH "${TRUTH_BINARY_DIR}" truth_binary_dir)

if(NOT DEFINED TRUTH_QUALITY_CSV OR "${TRUTH_QUALITY_CSV}" STREQUAL "")
  set(TRUTH_QUALITY_CSV "${truth_source_dir}/config/quality-tiers.csv")
endif()
if(NOT EXISTS "${TRUTH_QUALITY_CSV}")
  message(FATAL_ERROR "Truth quality manifest is absent: ${TRUTH_QUALITY_CSV}")
endif()
file(REAL_PATH "${TRUTH_QUALITY_CSV}" truth_quality_csv)

if(NOT DEFINED TRUTH_OUTPUT_DIR OR "${TRUTH_OUTPUT_DIR}" STREQUAL "")
  set(TRUTH_OUTPUT_DIR "${truth_binary_dir}/presets")
endif()
set(truth_output_candidate "${TRUTH_OUTPUT_DIR}")
cmake_path(ABSOLUTE_PATH truth_output_candidate
  BASE_DIRECTORY "${truth_binary_dir}"
  NORMALIZE
  OUTPUT_VARIABLE truth_output_absolute)
file(REAL_PATH "${truth_output_absolute}" truth_output_dir)
cmake_path(IS_PREFIX truth_binary_dir "${truth_output_dir}" NORMALIZE truth_output_is_owned)
if(NOT truth_output_is_owned OR truth_output_dir STREQUAL truth_binary_dir)
  message(FATAL_ERROR
    "Truth quality output must be a descendant of the owned build tree: ${truth_binary_dir}")
endif()

file(STRINGS "${truth_quality_csv}" quality_lines ENCODING UTF-8)
list(LENGTH quality_lines quality_line_count)
if(NOT quality_line_count EQUAL 6)
  message(FATAL_ERROR
    "Truth quality manifest must contain one header and exactly five tiers; found ${quality_line_count} lines")
endif()

list(GET quality_lines 0 quality_header)
set(expected_header
  "tier,id,label,cloud_mode,cloud_primary_steps,cloud_light_steps,aurora_samples,ao_directions,ao_steps,dof_rings,bloom_radius,ssr_steps")
if(NOT quality_header STREQUAL expected_header)
  message(FATAL_ERROR "Truth quality manifest header does not match the canonical contract")
endif()

set(expected_ids performance balanced quality ultra cinematic)
set(expected_labels Performance Balanced Quality Ultra Cinematic)
set(expected_cloud_modes analytic analytic volume volume volume)
set(expected_quality_rows
  "0,performance,Performance,analytic,0,0,1,4,2,0,2,0"
  "1,balanced,Balanced,analytic,0,0,2,6,3,2,3,0"
  "2,quality,Quality,volume,8,2,4,8,4,3,4,8"
  "3,ultra,Ultra,volume,12,3,7,12,5,4,5,12"
  "4,cinematic,Cinematic,volume,16,4,10,16,6,5,6,16")
set(seen_tiers)
set(truth_numeric_columns
  tier
  cloud_primary_steps
  cloud_light_steps
  aurora_samples
  ao_directions
  ao_steps
  dof_rings
  bloom_radius
  ssr_steps)

foreach(line_index RANGE 1 5)
  list(GET quality_lines ${line_index} quality_line)
  string(REPLACE "," ";" quality_fields "${quality_line}")
  list(LENGTH quality_fields quality_field_count)
  if(NOT quality_field_count EQUAL 12)
    message(FATAL_ERROR "Truth quality manifest row ${line_index} must contain 12 fields")
  endif()

  list(GET quality_fields 0 tier)
  list(GET quality_fields 1 tier_id)
  list(GET quality_fields 2 tier_label)
  list(GET quality_fields 3 cloud_mode)
  list(GET quality_fields 4 cloud_primary_steps)
  list(GET quality_fields 5 cloud_light_steps)
  list(GET quality_fields 6 aurora_samples)
  list(GET quality_fields 7 ao_directions)
  list(GET quality_fields 8 ao_steps)
  list(GET quality_fields 9 dof_rings)
  list(GET quality_fields 10 bloom_radius)
  list(GET quality_fields 11 ssr_steps)

  foreach(numeric_column IN LISTS truth_numeric_columns)
    if(NOT "${${numeric_column}}" MATCHES "^[0-9]+$")
      message(FATAL_ERROR
        "Truth quality manifest row ${line_index} has an invalid integer in ${numeric_column}: ${${numeric_column}}")
    endif()
  endforeach()
  if(NOT tier MATCHES "^[0-4]$")
    message(FATAL_ERROR "Truth quality manifest has an unexpected tier: ${tier}")
  endif()
  list(FIND seen_tiers "${tier}" seen_tier_index)
  if(NOT seen_tier_index EQUAL -1)
    message(FATAL_ERROR "Truth quality manifest has a duplicate tier: ${tier}")
  endif()
  list(APPEND seen_tiers "${tier}")

  list(GET expected_ids ${tier} expected_id)
  list(GET expected_labels ${tier} expected_label)
  list(GET expected_cloud_modes ${tier} expected_cloud_mode)
  list(GET expected_quality_rows ${tier} expected_quality_row)
  if(NOT tier_id STREQUAL expected_id)
    message(FATAL_ERROR
      "Truth quality manifest has an unexpected id for tier ${tier}: ${tier_id}")
  endif()
  if(NOT tier_label STREQUAL expected_label)
    message(FATAL_ERROR
      "Truth quality manifest has an unexpected label for tier ${tier}: ${tier_label}")
  endif()
  if(NOT cloud_mode STREQUAL expected_cloud_mode)
    message(FATAL_ERROR
      "Truth quality manifest has an unexpected cloud mode for tier ${tier}: ${cloud_mode}")
  endif()
  if(NOT quality_line STREQUAL expected_quality_row)
    message(FATAL_ERROR
      "Truth quality manifest row for tier ${tier} does not match the canonical contract")
  endif()

  set("truth_quality_row_${tier}" "${quality_fields}")
endforeach()

foreach(expected_tier RANGE 0 4)
  list(FIND seen_tiers "${expected_tier}" expected_tier_index)
  if(expected_tier_index EQUAL -1)
    message(FATAL_ERROR "Truth quality manifest is missing tier ${expected_tier}")
  endif()
endforeach()

# Host manifest. Effects 11 injects no preprocessor defines into preset shaders
# (both D3DCompile sites pass nullptr for pDefines), so a preset cannot detect
# its host at compile time. Host selection travels through the generated INIs,
# which is why this is a second generator axis rather than a shader fork.
if(NOT DEFINED TRUTH_HOSTS_CSV OR "${TRUTH_HOSTS_CSV}" STREQUAL "")
  set(TRUTH_HOSTS_CSV "${truth_source_dir}/config/hosts.csv")
endif()
if(NOT EXISTS "${TRUTH_HOSTS_CSV}")
  message(FATAL_ERROR "Truth host manifest is absent: ${TRUTH_HOSTS_CSV}")
endif()
file(REAL_PATH "${TRUTH_HOSTS_CSV}" truth_hosts_csv)

file(STRINGS "${truth_hosts_csv}" host_lines ENCODING UTF-8)
list(LENGTH host_lines host_line_count)
if(NOT host_line_count EQUAL 3)
  message(FATAL_ERROR
    "Truth host manifest must contain one header and exactly two hosts; found ${host_line_count} lines")
endif()

list(GET host_lines 0 host_header)
set(expected_host_header
  "host,id,label,postpass_intensity,postpass_vignette_strength,postpass_grain_shape")
if(NOT host_header STREQUAL expected_host_header)
  message(FATAL_ERROR "Truth host manifest header does not match the canonical contract")
endif()

set(expected_host_rows
  "0,enbseries,ENBSeries,1.0,0.18,0.0"
  "1,effects11,Effects 11,1.0,0.0,0.0")
foreach(host_index RANGE 0 1)
  math(EXPR host_line_index "${host_index} + 1")
  list(GET host_lines ${host_line_index} host_line)
  list(GET expected_host_rows ${host_index} expected_host_row)
  if(NOT host_line STREQUAL expected_host_row)
    message(FATAL_ERROR
      "Truth host manifest row ${host_line_index} does not match the canonical contract")
  endif()
  string(REPLACE "," ";" host_fields "${host_line}")
  list(LENGTH host_fields host_field_count)
  if(NOT host_field_count EQUAL 6)
    message(FATAL_ERROR "Truth host manifest row ${host_line_index} must contain 6 fields")
  endif()
  set("truth_host_row_${host_index}" "${host_fields}")
endforeach()

file(REMOVE_RECURSE "${truth_output_dir}")
file(MAKE_DIRECTORY "${truth_output_dir}")

set(truth_stage_files
  enbeffectprepass.fx
  enbdepthoffield.fx
  enbbloom.fx
  enbadaptation.fx
  enblens.fx
  enbeffect.fx
  enbeffectpostpass.fx
  enbsunsprite.fx
  enbunderwater.fx)

function(set_truth_preset_values tier)
  if(tier EQUAL 0)
    set(preset_master_enabled true)
    set(preset_manual_exposure_ev 0.0)
    set(preset_auto_exposure_blend 0.25)
    set(preset_use_enb_bloom false)
    set(preset_use_enb_lens false)
    set(preset_procedural_sky_enabled true)
    set(preset_sky_replacement_strength 0.62)
    set(preset_sky_depth_threshold 0.9998)
    set(preset_sky_depth_feather 0.0002)
    set(preset_sky_radiance_scale 1.0)
    set(preset_weather_density 0.25)
    set(preset_cloud_coverage 0.45)
    set(preset_cloud_density 0.62)
    set(preset_fog_density 0.12)
    set(preset_aurora_activity 0.25)
    set(preset_aurora_mask 0.80)
    set(preset_sky_wind_x 0.62)
    set(preset_sky_wind_y -0.27)
    set(preset_prepass_enabled true)
    set(preset_prepass_intensity 0.52)
    set(preset_prepass_depth_shape 0.50)
    set(preset_dof_enabled false)
    set(preset_dof_intensity 0.0)
    set(preset_dof_focus_shape 0.45)
    set(preset_bloom_enabled false)
    set(preset_bloom_intensity 0.0)
    set(preset_bloom_threshold_shape 1.20)
    set(preset_adaptation_enabled true)
    set(preset_adaptation_intensity 0.35)
    set(preset_adaptation_response_shape 0.35)
    set(preset_lens_enabled false)
    set(preset_lens_intensity 0.0)
    set(preset_lens_aperture_shape 0.35)
    set(preset_postpass_enabled true)
    set(preset_postpass_intensity 0.35)
    set(preset_sun_sprite_enabled true)
    set(preset_sun_sprite_intensity 0.12)
    set(preset_sun_sprite_disc_shape 0.40)
    set(preset_underwater_enabled true)
    set(preset_underwater_intensity 0.25)
    set(preset_underwater_density_shape 0.20)
  elseif(tier EQUAL 1)
    set(preset_master_enabled true)
    set(preset_manual_exposure_ev 0.0)
    set(preset_auto_exposure_blend 0.25)
    set(preset_use_enb_bloom true)
    set(preset_use_enb_lens false)
    set(preset_procedural_sky_enabled true)
    set(preset_sky_replacement_strength 0.62)
    set(preset_sky_depth_threshold 0.9998)
    set(preset_sky_depth_feather 0.0002)
    set(preset_sky_radiance_scale 1.0)
    set(preset_weather_density 0.25)
    set(preset_cloud_coverage 0.45)
    set(preset_cloud_density 0.62)
    set(preset_fog_density 0.12)
    set(preset_aurora_activity 0.25)
    set(preset_aurora_mask 0.80)
    set(preset_sky_wind_x 0.62)
    set(preset_sky_wind_y -0.27)
    set(preset_prepass_enabled true)
    set(preset_prepass_intensity 0.52)
    set(preset_prepass_depth_shape 0.50)
    set(preset_dof_enabled false)
    set(preset_dof_intensity 0.12)
    set(preset_dof_focus_shape 0.50)
    set(preset_bloom_enabled true)
    set(preset_bloom_intensity 0.20)
    set(preset_bloom_threshold_shape 1.15)
    set(preset_adaptation_enabled true)
    set(preset_adaptation_intensity 0.50)
    set(preset_adaptation_response_shape 0.50)
    set(preset_lens_enabled false)
    set(preset_lens_intensity 0.08)
    set(preset_lens_aperture_shape 0.45)
    set(preset_postpass_enabled true)
    set(preset_postpass_intensity 0.45)
    set(preset_sun_sprite_enabled true)
    set(preset_sun_sprite_intensity 0.18)
    set(preset_sun_sprite_disc_shape 0.50)
    set(preset_underwater_enabled true)
    set(preset_underwater_intensity 0.32)
    set(preset_underwater_density_shape 0.25)
  elseif(tier EQUAL 2)
    set(preset_master_enabled true)
    set(preset_manual_exposure_ev 0.0)
    set(preset_auto_exposure_blend 0.25)
    set(preset_use_enb_bloom true)
    set(preset_use_enb_lens true)
    set(preset_procedural_sky_enabled true)
    set(preset_sky_replacement_strength 0.62)
    set(preset_sky_depth_threshold 0.9998)
    set(preset_sky_depth_feather 0.0002)
    set(preset_sky_radiance_scale 1.0)
    set(preset_weather_density 0.25)
    set(preset_cloud_coverage 0.45)
    set(preset_cloud_density 0.62)
    set(preset_fog_density 0.12)
    set(preset_aurora_activity 0.25)
    set(preset_aurora_mask 0.80)
    set(preset_sky_wind_x 0.62)
    set(preset_sky_wind_y -0.27)
    set(preset_prepass_enabled true)
    set(preset_prepass_intensity 0.52)
    set(preset_prepass_depth_shape 0.50)
    set(preset_dof_enabled true)
    set(preset_dof_intensity 0.18)
    set(preset_dof_focus_shape 0.52)
    set(preset_bloom_enabled true)
    set(preset_bloom_intensity 0.24)
    set(preset_bloom_threshold_shape 1.12)
    set(preset_adaptation_enabled true)
    set(preset_adaptation_intensity 0.55)
    set(preset_adaptation_response_shape 0.52)
    set(preset_lens_enabled true)
    set(preset_lens_intensity 0.10)
    set(preset_lens_aperture_shape 0.48)
    set(preset_postpass_enabled true)
    set(preset_postpass_intensity 0.52)
    set(preset_sun_sprite_enabled true)
    set(preset_sun_sprite_intensity 0.24)
    set(preset_sun_sprite_disc_shape 0.52)
    set(preset_underwater_enabled true)
    set(preset_underwater_intensity 0.40)
    set(preset_underwater_density_shape 0.27)
  elseif(tier EQUAL 3)
    set(preset_master_enabled true)
    set(preset_manual_exposure_ev 0.0)
    set(preset_auto_exposure_blend 0.25)
    set(preset_use_enb_bloom true)
    set(preset_use_enb_lens true)
    set(preset_procedural_sky_enabled true)
    set(preset_sky_replacement_strength 0.62)
    set(preset_sky_depth_threshold 0.9998)
    set(preset_sky_depth_feather 0.0002)
    set(preset_sky_radiance_scale 1.0)
    set(preset_weather_density 0.25)
    set(preset_cloud_coverage 0.45)
    set(preset_cloud_density 0.62)
    set(preset_fog_density 0.12)
    set(preset_aurora_activity 0.25)
    set(preset_aurora_mask 0.80)
    set(preset_sky_wind_x 0.62)
    set(preset_sky_wind_y -0.27)
    set(preset_prepass_enabled true)
    set(preset_prepass_intensity 0.52)
    set(preset_prepass_depth_shape 0.50)
    set(preset_dof_enabled true)
    set(preset_dof_intensity 0.25)
    set(preset_dof_focus_shape 0.55)
    set(preset_bloom_enabled true)
    set(preset_bloom_intensity 0.30)
    set(preset_bloom_threshold_shape 1.08)
    set(preset_adaptation_enabled true)
    set(preset_adaptation_intensity 0.60)
    set(preset_adaptation_response_shape 0.55)
    set(preset_lens_enabled true)
    set(preset_lens_intensity 0.16)
    set(preset_lens_aperture_shape 0.52)
    set(preset_postpass_enabled true)
    set(preset_postpass_intensity 0.58)
    set(preset_sun_sprite_enabled true)
    set(preset_sun_sprite_intensity 0.30)
    set(preset_sun_sprite_disc_shape 0.55)
    set(preset_underwater_enabled true)
    set(preset_underwater_intensity 0.46)
    set(preset_underwater_density_shape 0.30)
  elseif(tier EQUAL 4)
    set(preset_master_enabled true)
    set(preset_manual_exposure_ev 0.0)
    set(preset_auto_exposure_blend 0.25)
    set(preset_use_enb_bloom true)
    set(preset_use_enb_lens true)
    set(preset_procedural_sky_enabled true)
    set(preset_sky_replacement_strength 0.62)
    set(preset_sky_depth_threshold 0.9998)
    set(preset_sky_depth_feather 0.0002)
    set(preset_sky_radiance_scale 1.0)
    set(preset_weather_density 0.25)
    set(preset_cloud_coverage 0.45)
    set(preset_cloud_density 0.62)
    set(preset_fog_density 0.12)
    set(preset_aurora_activity 0.25)
    set(preset_aurora_mask 0.80)
    set(preset_sky_wind_x 0.62)
    set(preset_sky_wind_y -0.27)
    set(preset_prepass_enabled true)
    set(preset_prepass_intensity 0.52)
    set(preset_prepass_depth_shape 0.50)
    set(preset_dof_enabled true)
    set(preset_dof_intensity 0.34)
    set(preset_dof_focus_shape 0.58)
    set(preset_bloom_enabled true)
    set(preset_bloom_intensity 0.36)
    set(preset_bloom_threshold_shape 1.02)
    set(preset_adaptation_enabled true)
    set(preset_adaptation_intensity 0.65)
    set(preset_adaptation_response_shape 0.58)
    set(preset_lens_enabled true)
    set(preset_lens_intensity 0.22)
    set(preset_lens_aperture_shape 0.55)
    set(preset_postpass_enabled true)
    set(preset_postpass_intensity 0.64)
    set(preset_sun_sprite_enabled true)
    set(preset_sun_sprite_intensity 0.38)
    set(preset_sun_sprite_disc_shape 0.58)
    set(preset_underwater_enabled true)
    set(preset_underwater_intensity 0.50)
    set(preset_underwater_density_shape 0.32)
  else()
    message(FATAL_ERROR "Unexpected Truth quality tier: ${tier}")
  endif()

  foreach(preset_variable IN ITEMS
      preset_master_enabled
      preset_manual_exposure_ev
      preset_auto_exposure_blend
      preset_use_enb_bloom
      preset_use_enb_lens
      preset_procedural_sky_enabled
      preset_sky_replacement_strength
      preset_sky_depth_threshold
      preset_sky_depth_feather
      preset_sky_radiance_scale
      preset_weather_density
      preset_cloud_coverage
      preset_cloud_density
      preset_fog_density
      preset_aurora_activity
      preset_aurora_mask
      preset_sky_wind_x
      preset_sky_wind_y
      preset_prepass_enabled
      preset_prepass_intensity
      preset_prepass_depth_shape
      preset_dof_enabled
      preset_dof_intensity
      preset_dof_focus_shape
      preset_bloom_enabled
      preset_bloom_intensity
      preset_bloom_threshold_shape
      preset_adaptation_enabled
      preset_adaptation_intensity
      preset_adaptation_response_shape
      preset_lens_enabled
      preset_lens_intensity
      preset_lens_aperture_shape
      preset_postpass_enabled
      preset_postpass_intensity
      preset_sun_sprite_enabled
      preset_sun_sprite_intensity
      preset_sun_sprite_disc_shape
      preset_underwater_enabled
      preset_underwater_intensity
      preset_underwater_density_shape)
    set(${preset_variable} "${${preset_variable}}" PARENT_SCOPE)
  endforeach()
endfunction()

function(write_truth_stage_ini output_dir stage_file host_vignette_strength host_grain_shape)
  string(TOUPPER "${stage_file}" stage_section)
  string(CONCAT stage_contents
    "; Generated Truth ENB stage settings.\n"
    "[${stage_section}]\n")

  if(stage_file STREQUAL "enbeffectprepass.fx")
    string(APPEND stage_contents
      "[Truth 10] Prepass | Enabled=${preset_prepass_enabled}\n"
      "[Truth 10] Prepass | Intensity=${preset_prepass_intensity}\n"
      "[Truth 10] Prepass | Depth Shape=${preset_prepass_depth_shape}\n"
      "[Truth 10] Sky | Procedural Replacement=${preset_procedural_sky_enabled}\n"
      "[Truth 10] Sky | Replacement Strength=${preset_sky_replacement_strength}\n"
      "[Truth 10] Sky | Depth Threshold=${preset_sky_depth_threshold}\n"
      "[Truth 10] Sky | Depth Feather=${preset_sky_depth_feather}\n"
      "[Truth 10] Sky | Radiance Scale=${preset_sky_radiance_scale}\n"
      "[Truth 11] Weather | Density=${preset_weather_density}\n"
      "[Truth 12] Clouds | Coverage=${preset_cloud_coverage}\n"
      "[Truth 12] Clouds | Density=${preset_cloud_density}\n"
      "[Truth 13] Atmosphere | Fog Density=${preset_fog_density}\n"
      "[Truth 14] Aurora | Activity=${preset_aurora_activity}\n"
      "[Truth 14] Aurora | Weather Mask=${preset_aurora_mask}\n"
      "[Truth 15] Motion | Wind X=${preset_sky_wind_x}\n"
      "[Truth 15] Motion | Wind Y=${preset_sky_wind_y}\n"
      "[Truth 16] World | Aurora Origin=0,0,0\n")
  elseif(stage_file STREQUAL "enbdepthoffield.fx")
    string(APPEND stage_contents
      "[Truth 20] Depth of Field | Enabled=${preset_dof_enabled}\n"
      "[Truth 20] Depth of Field | Intensity=${preset_dof_intensity}\n"
      "[Truth 20] Depth of Field | Focus Shape=${preset_dof_focus_shape}\n")
  elseif(stage_file STREQUAL "enbbloom.fx")
    string(APPEND stage_contents
      "[Truth 30] Bloom | Enabled=${preset_bloom_enabled}\n"
      "[Truth 30] Bloom | Intensity=${preset_bloom_intensity}\n"
      "[Truth 30] Bloom | Threshold Shape=${preset_bloom_threshold_shape}\n")
  elseif(stage_file STREQUAL "enbadaptation.fx")
    string(APPEND stage_contents
      "[Truth 40] Adaptation | Enabled=${preset_adaptation_enabled}\n"
      "[Truth 40] Adaptation | Intensity=${preset_adaptation_intensity}\n"
      "[Truth 40] Adaptation | Response Shape=${preset_adaptation_response_shape}\n")
  elseif(stage_file STREQUAL "enblens.fx")
    string(APPEND stage_contents
      "[Truth 50] Lens | Enabled=${preset_lens_enabled}\n"
      "[Truth 50] Lens | Intensity=${preset_lens_intensity}\n"
      "[Truth 50] Lens | Aperture Shape=${preset_lens_aperture_shape}\n")
  elseif(stage_file STREQUAL "enbeffect.fx")
    string(APPEND stage_contents
      "[Truth 00] Master | Enabled=${preset_master_enabled}\n"
      "[Truth 02] Optical | ENB Bloom=${preset_use_enb_bloom}\n"
      "[Truth 02] Optical | ENB Lens=${preset_use_enb_lens}\n"
      "[Truth 60] Main Effect | Manual EV=${preset_manual_exposure_ev}\n"
      "[Truth 60] Main Effect | Auto Blend=${preset_auto_exposure_blend}\n")
  elseif(stage_file STREQUAL "enbeffectpostpass.fx")
    string(APPEND stage_contents
      "[Truth 70] Postpass | Enabled=${preset_postpass_enabled}\n"
      "[Truth 70] Postpass | Intensity=${preset_postpass_intensity}\n"
      "[Truth 70] Postpass | Grain Shape=${host_grain_shape}\n"
      "[Truth 70] Postpass | Vignette Strength=${host_vignette_strength}\n")
  elseif(stage_file STREQUAL "enbsunsprite.fx")
    string(APPEND stage_contents
      "[Truth 80] Sun Sprite | Enabled=${preset_sun_sprite_enabled}\n"
      "[Truth 80] Sun Sprite | Intensity=${preset_sun_sprite_intensity}\n"
      "[Truth 80] Sun Sprite | Disc Shape=${preset_sun_sprite_disc_shape}\n")
  elseif(stage_file STREQUAL "enbunderwater.fx")
    string(APPEND stage_contents
      "[Truth 90] Underwater | Enabled=${preset_underwater_enabled}\n"
      "[Truth 90] Underwater | Intensity=${preset_underwater_intensity}\n"
      "[Truth 90] Underwater | Density Shape=${preset_underwater_density_shape}\n")
  else()
    message(FATAL_ERROR "Unexpected Truth stage file: ${stage_file}")
  endif()

  file(WRITE "${output_dir}/${stage_file}.ini" "${stage_contents}")
endfunction()

foreach(host_index RANGE 0 1)
  set(host_fields "${truth_host_row_${host_index}}")
  list(GET host_fields 1 host_id)
  list(GET host_fields 2 host_label)
  list(GET host_fields 3 host_postpass_intensity)
  list(GET host_fields 4 host_vignette_strength)
  list(GET host_fields 5 host_grain_shape)

foreach(tier RANGE 0 4)
  set(quality_fields "${truth_quality_row_${tier}}")
  list(GET quality_fields 1 tier_id)
  list(GET quality_fields 2 tier_label)
  list(GET quality_fields 3 cloud_mode)
  list(GET quality_fields 4 cloud_primary_steps)
  list(GET quality_fields 5 cloud_light_steps)
  list(GET quality_fields 6 aurora_samples)
  list(GET quality_fields 7 ao_directions)
  list(GET quality_fields 8 ao_steps)
  list(GET quality_fields 9 dof_rings)
  list(GET quality_fields 10 bloom_radius)
  list(GET quality_fields 11 ssr_steps)
  set_truth_preset_values(${tier})

  set(tier_enbseries_dir "${truth_output_dir}/${host_id}/${tier_id}/ROOT/enbseries")
  file(MAKE_DIRECTORY "${tier_enbseries_dir}/truth")
  string(CONCAT tier_quality_values
    "; Generated from config/quality-tiers.csv and config/hosts.csv\n"
    "; Human-readable metadata only; shader tier selection is in truth/TruthQualityPresetOverride.fxh.\n"
    "[Truth Quality]\n"
    "Product=Truth ENB\n"
    "Host=${host_id}\n"
    "HostLabel=${host_label}\n"
    "Tier=${tier}\n"
    "TierId=${tier_id}\n"
    "TierLabel=${tier_label}\n"
    "CloudMode=${cloud_mode}\n"
    "CloudPrimarySteps=${cloud_primary_steps}\n"
    "CloudLightSteps=${cloud_light_steps}\n"
    "AuroraSamples=${aurora_samples}\n"
    "AODirections=${ao_directions}\n"
    "AOSteps=${ao_steps}\n"
    "DOFRings=${dof_rings}\n"
    "BloomRadius=${bloom_radius}\n"
    "SSRSteps=${ssr_steps}\n")
  string(CONCAT tier_override_contents
    "#ifndef TRUTH_QUALITY_PRESET_OVERRIDE_FXH\n"
    "#define TRUTH_QUALITY_PRESET_OVERRIDE_FXH\n"
    "\n"
    "// Generated Truth ENB quality preset override.\n"
    "// Host: ${host_id}\n"
    "// Tier: ${tier_id} (${tier})\n"
    "// Command-line /DTRUTH_QUALITY_TIER=N remains authoritative.\n"
    "\n"
    "#ifndef TRUTH_QUALITY_TIER\n"
    "#define TRUTH_QUALITY_TIER ${tier}\n"
    "#endif\n"
    "\n"
    "#endif  // TRUTH_QUALITY_PRESET_OVERRIDE_FXH\n")

  file(WRITE "${tier_enbseries_dir}/truth-quality.ini"
    "${tier_quality_values}")
  file(WRITE "${tier_enbseries_dir}/truth/TruthQualityPresetOverride.fxh"
    "${tier_override_contents}")
  foreach(stage_file IN LISTS truth_stage_files)
    write_truth_stage_ini(
      "${tier_enbseries_dir}"
      "${stage_file}"
      "${host_vignette_strength}"
      "${host_grain_shape}")
  endforeach()
endforeach()
endforeach()
