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

foreach(host_index RANGE 0 1)
  set(host_fields "${truth_host_row_${host_index}}")
  list(GET host_fields 1 host_id)
  list(GET host_fields 2 host_label)
  list(GET host_fields 3 host_postpass_intensity)
  list(GET host_fields 4 host_vignette_strength)
  list(GET host_fields 5 host_grain_shape)

  string(CONCAT host_values
    "[TRUTH HOST]\n"
    "Host=${host_id}\n"
    "HostLabel=${host_label}\n"
    "TruthPostpassIntensity=${host_postpass_intensity}\n"
    "TruthPostpassVignetteStrength=${host_vignette_strength}\n"
    "TruthPostpassGrainShape=${host_grain_shape}\n")

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

  set(tier_enbseries_dir "${truth_output_dir}/${host_id}/${tier_id}/ROOT/enbseries")
  file(MAKE_DIRECTORY "${tier_enbseries_dir}")
  set(tier_header
    "; Generated from config/quality-tiers.csv and config/hosts.csv\n; Product=Truth ENB\n; Host=${host_id}\n; Tier=${tier_id}\n")
  string(CONCAT tier_quality_values
    "[TRUTH QUALITY]\n"
    "TRUTH_QUALITY_TIER=${tier}\n"
    "Label=${tier_label}\n"
    "CloudMode=${cloud_mode}\n"
    "CloudPrimarySteps=${cloud_primary_steps}\n"
    "CloudLightSteps=${cloud_light_steps}\n"
    "AuroraSamples=${aurora_samples}\n"
    "AODirections=${ao_directions}\n"
    "AOSteps=${ao_steps}\n"
    "DOFRings=${dof_rings}\n"
    "BloomRadius=${bloom_radius}\n"
    "SSRSteps=${ssr_steps}\n")

  file(WRITE "${tier_enbseries_dir}/truth-quality.ini"
    "${tier_header}\n${tier_quality_values}\n${host_values}")
  foreach(stage_file IN LISTS truth_stage_files)
    file(WRITE "${tier_enbseries_dir}/${stage_file}.ini"
      "${tier_header}\n${tier_quality_values}\n${host_values}\n[TRUTH STAGE]\nName=${stage_file}\n")
  endforeach()
endforeach()
endforeach()
