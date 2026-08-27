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
set(truth_generator "${truth_source_dir}/cmake/GenerateTruthQualityPresets.cmake")
set(truth_quality_include "${truth_source_dir}/shaders/truth/TruthQuality.fxh")
set(truth_stage_parameters "${truth_source_dir}/shaders/truth/TruthStageParameters.fxh")
set(truth_effect_parameters "${truth_source_dir}/shaders/truth/TruthEffectParameters.fxh")
set(truth_environment_parameters
  "${truth_source_dir}/shaders/truth/TruthEnvironmentParameters.fxh")
if(NOT EXISTS "${truth_generator}")
  message(FATAL_ERROR "Truth quality generator is absent: ${truth_generator}")
endif()
if(NOT EXISTS "${truth_quality_include}")
  message(FATAL_ERROR "Truth quality include is absent: ${truth_quality_include}")
endif()
foreach(required_parameter_file IN ITEMS
    "${truth_stage_parameters}"
    "${truth_effect_parameters}"
    "${truth_environment_parameters}")
  if(NOT EXISTS "${required_parameter_file}")
    message(FATAL_ERROR
      "Truth quality preset check requires parameter source: ${required_parameter_file}")
  endif()
endforeach()

function(require_truth_source_contains source_contents required_text context)
  string(FIND "${source_contents}" "${required_text}" required_position)
  if(required_position EQUAL -1)
    message(FATAL_ERROR "${context} is missing required text: ${required_text}")
  endif()
endfunction()

file(READ "${truth_quality_include}" quality_include_source)
foreach(required_token IN ITEMS
    "#include \"truth/TruthQualityPresetOverride.fxh\""
    "#define TRUTH_QUALITY_TIER 1"
    "#error TRUTH_QUALITY_TIER must be in [0,4]"
    "static const uint TruthQualityTier = TRUTH_QUALITY_TIER;"
    "static const uint TruthQualityCloudPrimarySteps = 16u;"
    "static const uint TruthQualityCloudLightSteps = 4u;"
    "static const uint TruthQualityAuroraSamples = 10u;")
  string(FIND "${quality_include_source}" "${required_token}" token_position)
  if(token_position EQUAL -1)
    message(FATAL_ERROR "Truth quality include is missing required contract token: ${required_token}")
  endif()
endforeach()

file(READ "${truth_stage_parameters}" stage_parameter_source)
require_truth_source_contains("${stage_parameter_source}"
  [=[TruthPrepassIntensity <string UIName = "[Truth 10] Prepass | Intensity"; string UIWidget = "Spinner"; float UIMin = 0.0; float UIMax = 1.0; float UIStep = 0.01;> = 0.52;]=]
  "Truth stage parameter defaults must match the Balanced preset")
require_truth_source_contains("${stage_parameter_source}"
  [=[bool TruthDepthOfFieldEnabled <string UIName = "[Truth 20] Depth of Field | Enabled";> = false;]=]
  "Truth stage parameter defaults must match the Balanced preset")
require_truth_source_contains("${stage_parameter_source}"
  [=[TruthDepthOfFieldIntensity <string UIName = "[Truth 20] Depth of Field | Intensity"; string UIWidget = "Spinner"; float UIMin = 0.0; float UIMax = 1.0; float UIStep = 0.01;> = 0.12;]=]
  "Truth stage parameter defaults must match the Balanced preset")
require_truth_source_contains("${stage_parameter_source}"
  [=[TruthBloomIntensity <string UIName = "[Truth 30] Bloom | Intensity"; string UIWidget = "Spinner"; float UIMin = 0.0; float UIMax = 1.0; float UIStep = 0.01;> = 0.20;]=]
  "Truth stage parameter defaults must match the Balanced preset")
require_truth_source_contains("${stage_parameter_source}"
  [=[TruthBloomThresholdShape <string UIName = "[Truth 30] Bloom | Threshold Shape"; string UIWidget = "Spinner"; float UIMin = 0.0; float UIMax = 4.0; float UIStep = 0.01;> = 1.15;]=]
  "Truth stage parameter defaults must match the Balanced preset")
require_truth_source_contains("${stage_parameter_source}"
  [=[TruthAdaptationIntensity <string UIName = "[Truth 40] Adaptation | Intensity"; string UIWidget = "Spinner"; float UIMin = 0.0; float UIMax = 1.0; float UIStep = 0.01;> = 0.50;]=]
  "Truth stage parameter defaults must match the Balanced preset")
require_truth_source_contains("${stage_parameter_source}"
  [=[bool TruthLensEnabled <string UIName = "[Truth 50] Lens | Enabled";> = false;]=]
  "Truth stage parameter defaults must match the Balanced preset")
require_truth_source_contains("${stage_parameter_source}"
  [=[TruthLensIntensity <string UIName = "[Truth 50] Lens | Intensity"; string UIWidget = "Spinner"; float UIMin = 0.0; float UIMax = 1.0; float UIStep = 0.01;> = 0.08;]=]
  "Truth stage parameter defaults must match the Balanced preset")
require_truth_source_contains("${stage_parameter_source}"
  [=[TruthPostpassIntensity <string UIName = "[Truth 70] Postpass | Intensity"; string UIWidget = "Spinner"; float UIMin = 0.0; float UIMax = 1.0; float UIStep = 0.01;> = 0.45;]=]
  "Truth stage parameter defaults must match the Balanced preset")
require_truth_source_contains("${stage_parameter_source}"
  [=[TruthSunSpriteIntensity <string UIName = "[Truth 80] Sun Sprite | Intensity"; string UIWidget = "Spinner"; float UIMin = 0.0; float UIMax = 1.0; float UIStep = 0.01;> = 0.18;]=]
  "Truth stage parameter defaults must match the Balanced preset")
require_truth_source_contains("${stage_parameter_source}"
  [=[TruthUnderwaterIntensity <string UIName = "[Truth 90] Underwater | Intensity"; string UIWidget = "Spinner"; float UIMin = 0.0; float UIMax = 1.0; float UIStep = 0.01;> = 0.32;]=]
  "Truth stage parameter defaults must match the Balanced preset")

file(READ "${truth_effect_parameters}" effect_parameter_source)
file(READ "${truth_environment_parameters}" environment_parameter_source)
require_truth_source_contains("${effect_parameter_source}" [=[
float TruthAutoExposureBlend
<
    string UIName = "[Truth 60] Main Effect | Auto Blend";
    string UIWidget = "Spinner";
    float UIMin = 0.0;
    float UIMax = 1.0;
    float UIStep = 0.01;
> = 0.25;
]=]
  "Truth effect parameter defaults must match the Balanced preset")
require_truth_source_contains("${effect_parameter_source}" [=[
bool TruthUseEnbLens
<
    string UIName = "[Truth 02] Optical | ENB Lens";
> = false;
]=]
  "Truth effect parameter defaults must match the Balanced preset")
require_truth_source_contains("${environment_parameter_source}" [=[
float TruthSkyReplacementStrength
<
    string UIName = "[Truth 10] Sky | Replacement Strength";
    string UIWidget = "Spinner";
    float UIMin = 0.0;
    float UIMax = 1.0;
    float UIStep = 0.01;
> = 0.62;
]=]
  "Truth effect parameter defaults must match the Balanced preset")
require_truth_source_contains("${environment_parameter_source}" [=[
float TruthWeatherDensity
<
    string UIName = "[Truth 11] Weather | Density";
    string UIWidget = "Spinner";
    float UIMin = 0.0;
    float UIMax = 1.0;
    float UIStep = 0.01;
> = 0.25;
]=]
  "Truth effect parameter defaults must match the Balanced preset")
require_truth_source_contains("${environment_parameter_source}" [=[
float TruthCloudCoverage
<
    string UIName = "[Truth 12] Clouds | Coverage";
    string UIWidget = "Spinner";
    float UIMin = 0.0;
    float UIMax = 1.0;
    float UIStep = 0.01;
> = 0.45;
]=]
  "Truth effect parameter defaults must match the Balanced preset")
require_truth_source_contains("${environment_parameter_source}" [=[
float TruthCloudDensity
<
    string UIName = "[Truth 12] Clouds | Density";
    string UIWidget = "Spinner";
    float UIMin = 0.0;
    float UIMax = 1.0;
    float UIStep = 0.01;
> = 0.62;
]=]
  "Truth effect parameter defaults must match the Balanced preset")
require_truth_source_contains("${environment_parameter_source}" [=[
float TruthFogDensity
<
    string UIName = "[Truth 13] Atmosphere | Fog Density";
    string UIWidget = "Spinner";
    float UIMin = 0.0;
    float UIMax = 1.0;
    float UIStep = 0.01;
> = 0.12;
]=]
  "Truth effect parameter defaults must match the Balanced preset")
require_truth_source_contains("${environment_parameter_source}" [=[
float TruthAuroraActivity
<
    string UIName = "[Truth 14] Aurora | Activity";
    string UIWidget = "Spinner";
    float UIMin = 0.0;
    float UIMax = 1.0;
    float UIStep = 0.01;
> = 0.25;
]=]
  "Truth effect parameter defaults must match the Balanced preset")
require_truth_source_contains("${environment_parameter_source}" [=[
float TruthAuroraMask
<
    string UIName = "[Truth 14] Aurora | Weather Mask";
    string UIWidget = "Spinner";
    float UIMin = 0.0;
    float UIMax = 1.0;
    float UIStep = 0.01;
> = 0.80;
]=]
  "Truth effect parameter defaults must match the Balanced preset")

set(canonical_quality_rows
  "0,performance,Performance,analytic,0,0,1,4,2,0,2,0"
  "1,balanced,Balanced,analytic,0,0,2,6,3,2,3,0"
  "2,quality,Quality,volume,8,2,4,8,4,3,4,8"
  "3,ultra,Ultra,volume,12,3,7,12,5,4,5,12"
  "4,cinematic,Cinematic,volume,16,4,10,16,6,5,6,16")
foreach(tier RANGE 0 4)
  list(GET canonical_quality_rows ${tier} canonical_quality_row)
  string(REPLACE "," ";" canonical_quality_fields "${canonical_quality_row}")
  list(GET canonical_quality_fields 3 cloud_mode)
  list(GET canonical_quality_fields 4 cloud_primary_steps)
  list(GET canonical_quality_fields 5 cloud_light_steps)
  list(GET canonical_quality_fields 6 aurora_samples)
  list(GET canonical_quality_fields 7 ao_directions)
  list(GET canonical_quality_fields 8 ao_steps)
  list(GET canonical_quality_fields 9 dof_rings)
  list(GET canonical_quality_fields 10 bloom_radius)
  list(GET canonical_quality_fields 11 ssr_steps)
  if(cloud_mode STREQUAL "volume")
    set(uses_volume_clouds 1)
  else()
    set(uses_volume_clouds 0)
  endif()
  if(tier EQUAL 0)
    set(lens_ghosts 0)
  elseif(tier EQUAL 1)
    set(lens_ghosts 1)
  elseif(tier LESS 4)
    set(lens_ghosts 2)
  else()
    set(lens_ghosts 3)
  endif()
  if(tier EQUAL 0)
    set(quality_branch "#if TRUTH_QUALITY_TIER == ${tier}")
  elseif(tier LESS 4)
    set(quality_branch "#elif TRUTH_QUALITY_TIER == ${tier}")
  else()
    set(quality_branch "#else")
  endif()
  string(CONCAT expected_hlsl_branch
    "${quality_branch}\n"
    "static const uint TruthQualityCloudPrimarySteps = ${cloud_primary_steps}u;\n"
    "static const uint TruthQualityCloudLightSteps = ${cloud_light_steps}u;\n"
    "static const uint TruthQualityAuroraSamples = ${aurora_samples}u;\n"
    "static const uint TruthQualityAODirections = ${ao_directions}u;\n"
    "static const uint TruthQualityAOSteps = ${ao_steps}u;\n"
    "static const uint TruthQualityDOFRings = ${dof_rings}u;\n"
    "static const uint TruthQualityBloomRadius = ${bloom_radius}u;\n"
    "static const uint TruthQualityLensGhosts = ${lens_ghosts}u;\n"
    "static const uint TruthQualitySSRSteps = ${ssr_steps}u;\n"
    "static const uint TruthQualityUsesVolumeClouds = ${uses_volume_clouds}u;")
  string(FIND "${quality_include_source}" "${expected_hlsl_branch}"
    quality_branch_position)
  if(quality_branch_position EQUAL -1)
    message(FATAL_ERROR
      "Truth quality constants do not match the canonical tier ${tier} contract")
  endif()
endforeach()

set(truth_check_root "${truth_binary_dir}/truth-quality-presets-check")
file(REMOVE_RECURSE "${truth_check_root}")
file(MAKE_DIRECTORY "${truth_check_root}")
set(first_output "${truth_binary_dir}/presets")
set(second_output "${truth_check_root}/second")

function(run_truth_quality_generator output_directory)
  execute_process(
    COMMAND "${CMAKE_COMMAND}"
      "-DTRUTH_SOURCE_DIR=${truth_source_dir}"
      "-DTRUTH_BINARY_DIR=${truth_binary_dir}"
      "-DTRUTH_OUTPUT_DIR=${output_directory}"
      -P "${truth_generator}"
    RESULT_VARIABLE generator_result
    OUTPUT_VARIABLE generator_stdout
    ERROR_VARIABLE generator_stderr)
  if(NOT generator_result EQUAL 0)
    message(FATAL_ERROR
      "Truth quality preset generation failed: ${generator_stdout}${generator_stderr}")
  endif()
endfunction()

function(expect_truth_quality_generator_rejection case_name manifest_contents)
  set(manifest_path "${truth_check_root}/${case_name}.csv")
  set(output_directory "${truth_check_root}/${case_name}-output")
  file(WRITE "${manifest_path}" "${manifest_contents}")
  execute_process(
    COMMAND "${CMAKE_COMMAND}"
      "-DTRUTH_SOURCE_DIR=${truth_source_dir}"
      "-DTRUTH_BINARY_DIR=${truth_binary_dir}"
      "-DTRUTH_QUALITY_CSV=${manifest_path}"
      "-DTRUTH_OUTPUT_DIR=${output_directory}"
      -P "${truth_generator}"
    RESULT_VARIABLE generator_result
    OUTPUT_VARIABLE generator_stdout
    ERROR_VARIABLE generator_stderr)
  if(generator_result EQUAL 0)
    message(FATAL_ERROR
      "Truth quality generator accepted invalid manifest case: ${case_name}")
  endif()
endfunction()

run_truth_quality_generator("${first_output}")
run_truth_quality_generator("${second_output}")

function(hash_truth_quality_tree root output_hash)
  file(GLOB_RECURSE tree_files
    LIST_DIRECTORIES false
    RELATIVE "${root}"
    "${root}/*")
  list(SORT tree_files)
  set(tree_manifest "")
  foreach(relative_path IN LISTS tree_files)
    file(SHA256 "${root}/${relative_path}" file_hash)
    string(APPEND tree_manifest "${relative_path}|${file_hash}\n")
  endforeach()
  string(SHA256 tree_hash "${tree_manifest}")
  set(${output_hash} "${tree_hash}" PARENT_SCOPE)
endfunction()

hash_truth_quality_tree("${first_output}" first_hash)
hash_truth_quality_tree("${second_output}" second_hash)
if(NOT first_hash STREQUAL second_hash)
  message(FATAL_ERROR "Truth quality preset trees are not byte-identical")
endif()

set(expected_tiers performance balanced quality ultra cinematic)
set(canonical_tiers ${expected_tiers})
list(SORT expected_tiers)
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

# The root now holds hosts, and each host holds the five tiers.
set(canonical_hosts enbseries effects11)
set(sorted_hosts ${canonical_hosts})
list(SORT sorted_hosts)
file(GLOB first_host_entries LIST_DIRECTORIES true "${first_output}/*")
set(actual_hosts)
foreach(entry IN LISTS first_host_entries)
  if(NOT IS_DIRECTORY "${entry}")
    message(FATAL_ERROR "Truth quality preset root contains a non-host artifact: ${entry}")
  endif()
  get_filename_component(host_name "${entry}" NAME)
  list(APPEND actual_hosts "${host_name}")
endforeach()
list(SORT actual_hosts)
if(NOT "${actual_hosts}" STREQUAL "${sorted_hosts}")
  message(FATAL_ERROR "Truth quality preset root must contain exactly the two canonical hosts")
endif()

foreach(host_name IN LISTS canonical_hosts)
  file(GLOB first_entries LIST_DIRECTORIES true "${first_output}/${host_name}/*")
  set(actual_tiers)
  foreach(entry IN LISTS first_entries)
    if(NOT IS_DIRECTORY "${entry}")
      message(FATAL_ERROR "Truth quality preset host contains a non-tier artifact: ${entry}")
    endif()
    get_filename_component(tier_name "${entry}" NAME)
    list(APPEND actual_tiers "${tier_name}")
  endforeach()
  list(SORT actual_tiers)
  if(NOT "${actual_tiers}" STREQUAL "${expected_tiers}")
    message(FATAL_ERROR
      "Truth quality host ${host_name} must contain exactly five canonical tiers")
  endif()
endforeach()

file(GLOB_RECURSE first_ini_files
  LIST_DIRECTORIES false
  RELATIVE "${first_output}"
  "${first_output}/*.ini")
list(LENGTH first_ini_files first_ini_count)
if(NOT first_ini_count EQUAL 100)
  message(FATAL_ERROR "Truth quality presets must contain exactly 100 INI files; found ${first_ini_count}")
endif()

file(GLOB_RECURSE first_override_files
  LIST_DIRECTORIES false
  RELATIVE "${first_output}"
  "${first_output}/*/*/ROOT/enbseries/truth/TruthQualityPresetOverride.fxh")
list(LENGTH first_override_files first_override_count)
if(NOT first_override_count EQUAL 10)
  message(FATAL_ERROR
    "Truth quality presets must contain exactly 10 tier override includes; found ${first_override_count}")
endif()

function(require_truth_ini_contains contents required_text context)
  string(FIND "${contents}" "${required_text}" required_position)
  if(required_position EQUAL -1)
    message(FATAL_ERROR "${context} is missing required text: ${required_text}")
  endif()
endfunction()

function(reject_truth_ini_contains contents rejected_text context)
  string(FIND "${contents}" "${rejected_text}" rejected_position)
  if(NOT rejected_position EQUAL -1)
    message(FATAL_ERROR "${context} contains forbidden text: ${rejected_text}")
  endif()
endfunction()

set(truth_visible_preset_ui_names
  "[Truth 00] Master | Enabled"
  "[Truth 02] Optical | ENB Bloom"
  "[Truth 02] Optical | ENB Lens"
  "[Truth 10] Prepass | Enabled"
  "[Truth 10] Prepass | Intensity"
  "[Truth 10] Prepass | Depth Shape"
  "[Truth 10] Sky | Procedural Replacement"
  "[Truth 10] Sky | Replacement Strength"
  "[Truth 10] Sky | Depth Threshold"
  "[Truth 10] Sky | Depth Feather"
  "[Truth 10] Sky | Radiance Scale"
  "[Truth 11] Weather | Density"
  "[Truth 12] Clouds | Coverage"
  "[Truth 12] Clouds | Density"
  "[Truth 13] Atmosphere | Fog Density"
  "[Truth 14] Aurora | Activity"
  "[Truth 14] Aurora | Weather Mask"
  "[Truth 15] Motion | Wind X"
  "[Truth 15] Motion | Wind Y"
  "[Truth 16] World | Aurora Origin"
  "[Truth 20] Depth of Field | Enabled"
  "[Truth 20] Depth of Field | Intensity"
  "[Truth 20] Depth of Field | Focus Shape"
  "[Truth 30] Bloom | Enabled"
  "[Truth 30] Bloom | Intensity"
  "[Truth 30] Bloom | Threshold Shape"
  "[Truth 40] Adaptation | Enabled"
  "[Truth 40] Adaptation | Intensity"
  "[Truth 40] Adaptation | Response Shape"
  "[Truth 50] Lens | Enabled"
  "[Truth 50] Lens | Intensity"
  "[Truth 50] Lens | Aperture Shape"
  "[Truth 60] Main Effect | Manual EV"
  "[Truth 60] Main Effect | Auto Blend"
  "[Truth 70] Postpass | Enabled"
  "[Truth 70] Postpass | Intensity"
  "[Truth 70] Postpass | Grain Shape"
  "[Truth 70] Postpass | Vignette Strength"
  "[Truth 80] Sun Sprite | Enabled"
  "[Truth 80] Sun Sprite | Intensity"
  "[Truth 80] Sun Sprite | Disc Shape"
  "[Truth 90] Underwater | Enabled"
  "[Truth 90] Underwater | Intensity"
  "[Truth 90] Underwater | Density Shape")
list(LENGTH truth_visible_preset_ui_names truth_visible_preset_ui_count)
if(NOT truth_visible_preset_ui_count EQUAL 44)
  message(FATAL_ERROR
    "Truth preset checker must cover exactly 44 visible UI controls; "
    "found ${truth_visible_preset_ui_count}")
endif()

string(REGEX MATCHALL "string UIName = \"[^\"]+\""
  truth_source_ui_matches
  "${stage_parameter_source}\n${effect_parameter_source}\n${environment_parameter_source}")
set(truth_source_ui_names)
foreach(truth_source_ui_match IN LISTS truth_source_ui_matches)
  string(REGEX REPLACE "^string UIName = \"([^\"]+)\"$" "\\1"
    truth_source_ui_name "${truth_source_ui_match}")
  list(APPEND truth_source_ui_names "${truth_source_ui_name}")
endforeach()
list(SORT truth_source_ui_names)
list(REMOVE_DUPLICATES truth_source_ui_names)
set(truth_expected_ui_names ${truth_visible_preset_ui_names})
list(SORT truth_expected_ui_names)
if(NOT truth_source_ui_names STREQUAL truth_expected_ui_names)
  string(JOIN "\n  " expected_ui_names_text ${truth_expected_ui_names})
  string(JOIN "\n  " source_ui_names_text ${truth_source_ui_names})
  message(FATAL_ERROR
    "Truth preset checker UIName coverage no longer matches shader controls\n"
    "Expected:\n  ${expected_ui_names_text}\n"
    "Source:\n  ${source_ui_names_text}")
endif()

function(require_truth_stage_ini_contract stage_file contents context)
  string(TOUPPER "${stage_file}" stage_section)
  # TECHNIQUE=1 must sit directly under the section header, matching both the
  # generator and ENB's own save format; index 1 activates the first declared
  # Truth technique instead of ENB's internal DEFAULT shader.
  require_truth_ini_contains("${contents}" "[${stage_section}]\nTECHNIQUE=1\n" "${context}")
  foreach(forbidden_text IN ITEMS
      "[TRUTH QUALITY]"
      "[TRUTH HOST]"
      "[TRUTH STAGE]"
      "TRUTH_QUALITY_TIER="
      "TruthPostpassIntensity="
      "TruthPostpassVignetteStrength="
      "TruthPostpassGrainShape="
      "TruthBloomIntensity="
      "TruthCloudDensity=")
    reject_truth_ini_contains("${contents}" "${forbidden_text}" "${context}")
  endforeach()
  if("${contents}" MATCHES "(^|\n)Truth[A-Za-z0-9_]+=")
    message(FATAL_ERROR
      "${context} contains an HLSL identifier key instead of an ENB UIName key")
  endif()
  if(NOT stage_file STREQUAL "enbeffectpostpass.fx")
    reject_truth_ini_contains("${contents}" "[Truth 70] Postpass |" "${context}")
  endif()

  if(stage_file STREQUAL "enbeffectprepass.fx")
    set(required_keys
      "[Truth 10] Prepass | Enabled="
      "[Truth 10] Prepass | Intensity="
      "[Truth 10] Prepass | Depth Shape="
      "[Truth 10] Sky | Procedural Replacement="
      "[Truth 10] Sky | Replacement Strength="
      "[Truth 10] Sky | Depth Threshold="
      "[Truth 10] Sky | Depth Feather="
      "[Truth 10] Sky | Radiance Scale="
      "[Truth 11] Weather | Density="
      "[Truth 12] Clouds | Coverage="
      "[Truth 12] Clouds | Density="
      "[Truth 13] Atmosphere | Fog Density="
      "[Truth 14] Aurora | Activity="
      "[Truth 14] Aurora | Weather Mask="
      "[Truth 15] Motion | Wind X="
      "[Truth 15] Motion | Wind Y="
      "[Truth 16] World | Aurora Origin=")
  elseif(stage_file STREQUAL "enbdepthoffield.fx")
    set(required_keys
      "[Truth 20] Depth of Field | Enabled="
      "[Truth 20] Depth of Field | Intensity="
      "[Truth 20] Depth of Field | Focus Shape=")
  elseif(stage_file STREQUAL "enbbloom.fx")
    set(required_keys
      "[Truth 30] Bloom | Enabled="
      "[Truth 30] Bloom | Intensity="
      "[Truth 30] Bloom | Threshold Shape=")
  elseif(stage_file STREQUAL "enbadaptation.fx")
    set(required_keys
      "[Truth 40] Adaptation | Enabled="
      "[Truth 40] Adaptation | Intensity="
      "[Truth 40] Adaptation | Response Shape=")
  elseif(stage_file STREQUAL "enblens.fx")
    set(required_keys
      "[Truth 50] Lens | Enabled="
      "[Truth 50] Lens | Intensity="
      "[Truth 50] Lens | Aperture Shape=")
  elseif(stage_file STREQUAL "enbeffect.fx")
    set(required_keys
      "[Truth 00] Master | Enabled="
      "[Truth 02] Optical | ENB Bloom="
      "[Truth 02] Optical | ENB Lens="
      "[Truth 60] Main Effect | Manual EV="
      "[Truth 60] Main Effect | Auto Blend=")
  elseif(stage_file STREQUAL "enbeffectpostpass.fx")
    set(required_keys
      "[Truth 70] Postpass | Enabled="
      "[Truth 70] Postpass | Intensity="
      "[Truth 70] Postpass | Grain Shape="
      "[Truth 70] Postpass | Vignette Strength=")
  elseif(stage_file STREQUAL "enbsunsprite.fx")
    set(required_keys
      "[Truth 80] Sun Sprite | Enabled="
      "[Truth 80] Sun Sprite | Intensity="
      "[Truth 80] Sun Sprite | Disc Shape=")
  elseif(stage_file STREQUAL "enbunderwater.fx")
    set(required_keys
      "[Truth 90] Underwater | Enabled="
      "[Truth 90] Underwater | Intensity="
      "[Truth 90] Underwater | Density Shape=")
  else()
    message(FATAL_ERROR "Unexpected Truth stage file in preset contract: ${stage_file}")
  endif()
  foreach(required_key IN LISTS required_keys)
    require_truth_ini_contains("${contents}" "${required_key}" "${context}")
  endforeach()
  if(stage_file STREQUAL "enbeffectprepass.fx")
    require_truth_ini_contains("${contents}"
      "[Truth 16] World | Aurora Origin=0,0,0\n"
      "${context}")
  elseif(stage_file STREQUAL "enbeffect.fx")
    foreach(environment_prefix IN ITEMS
        "[Truth 10] Sky |"
        "[Truth 11] Weather |"
        "[Truth 12] Clouds |"
        "[Truth 13] Atmosphere |"
        "[Truth 14] Aurora |"
        "[Truth 15] Motion |"
        "[Truth 16] World |")
      reject_truth_ini_contains("${contents}" "${environment_prefix}" "${context}")
    endforeach()
  endif()
endfunction()

# Effects 11 injects no preprocessor defines into preset shaders, so a preset
# cannot detect its host at compile time. Host selection travels through these
# generated INIs instead, which is why the tier axis gained a host axis rather
# than the shader tree gaining a fork.
set(expected_hosts enbseries effects11)

foreach(host_name IN LISTS expected_hosts)
  foreach(tier_name IN LISTS expected_tiers)
    list(FIND canonical_tiers "${tier_name}" tier_index)
    file(GLOB tier_ini_files
      LIST_DIRECTORIES false
      "${first_output}/${host_name}/${tier_name}/ROOT/enbseries/*.ini")
    list(LENGTH tier_ini_files tier_ini_count)
    if(NOT tier_ini_count EQUAL 10)
      message(FATAL_ERROR
        "Truth quality host ${host_name} tier ${tier_name} must contain nine stage INIs and truth-quality.ini")
    endif()
    file(READ "${first_output}/${host_name}/${tier_name}/ROOT/enbseries/truth-quality.ini"
      truth_quality_contents)
    require_truth_ini_contains("${truth_quality_contents}" "[Truth Quality]\n"
      "Truth quality metadata for host ${host_name} tier ${tier_name}")
    require_truth_ini_contains("${truth_quality_contents}" "Tier=${tier_index}\n"
      "Truth quality metadata for host ${host_name} tier ${tier_name}")
    require_truth_ini_contains("${truth_quality_contents}" "TierId=${tier_name}\n"
      "Truth quality metadata for host ${host_name} tier ${tier_name}")
    reject_truth_ini_contains("${truth_quality_contents}" "TRUTH_QUALITY_TIER="
      "Truth quality metadata for host ${host_name} tier ${tier_name}")
    reject_truth_ini_contains("${truth_quality_contents}" "Postpass"
      "Truth quality metadata for host ${host_name} tier ${tier_name}")
    require_truth_ini_contains("${truth_quality_contents}" "Host=${host_name}\n"
      "Truth quality metadata for host ${host_name} tier ${tier_name}")

    set(tier_override
      "${first_output}/${host_name}/${tier_name}/ROOT/enbseries/truth/TruthQualityPresetOverride.fxh")
    if(NOT EXISTS "${tier_override}")
      message(FATAL_ERROR
        "Truth quality preset override is absent for host ${host_name} tier ${tier_name}: ${tier_override}")
    endif()
    file(READ "${tier_override}" tier_override_contents)
    require_truth_ini_contains("${tier_override_contents}"
      "// Generated Truth ENB quality preset override."
      "Truth quality preset override for host ${host_name} tier ${tier_name}")
    require_truth_ini_contains("${tier_override_contents}"
      "#ifndef TRUTH_QUALITY_TIER\n#define TRUTH_QUALITY_TIER ${tier_index}\n#endif"
      "Truth quality preset override for host ${host_name} tier ${tier_name}")
    reject_truth_ini_contains("${tier_override_contents}" "; Generated"
      "Truth quality preset override for host ${host_name} tier ${tier_name}")

    foreach(stage_file IN LISTS truth_stage_files)
      file(READ
        "${first_output}/${host_name}/${tier_name}/ROOT/enbseries/${stage_file}.ini"
        stage_ini_contents)
      require_truth_stage_ini_contract("${stage_file}" "${stage_ini_contents}"
        "Truth quality stage INI for host ${host_name} tier ${tier_name} stage ${stage_file}")
    endforeach()

    set(tier_stage_root
      "${first_output}/${host_name}/${tier_name}/ROOT/enbseries")
    file(READ "${tier_stage_root}/enbeffectprepass.fx.ini" tier_prepass)
    file(READ "${tier_stage_root}/enblens.fx.ini" tier_lens)
    file(READ "${tier_stage_root}/enbeffect.fx.ini" tier_main)
    foreach(stable_environment_value IN ITEMS
        "[Truth 10] Prepass | Intensity=0.52\n"
        "[Truth 10] Prepass | Depth Shape=0.50\n"
        "[Truth 10] Sky | Procedural Replacement=true\n"
        "[Truth 10] Sky | Replacement Strength=0.62\n"
        "[Truth 10] Sky | Depth Threshold=0.9998\n"
        "[Truth 10] Sky | Depth Feather=0.0002\n"
        "[Truth 10] Sky | Radiance Scale=1.0\n"
        "[Truth 11] Weather | Density=0.25\n"
        "[Truth 12] Clouds | Coverage=0.45\n"
        "[Truth 12] Clouds | Density=0.62\n"
        "[Truth 13] Atmosphere | Fog Density=0.12\n"
        "[Truth 14] Aurora | Activity=0.25\n"
        "[Truth 14] Aurora | Weather Mask=0.80\n"
        "[Truth 15] Motion | Wind X=0.62\n"
        "[Truth 15] Motion | Wind Y=-0.27\n"
        "[Truth 16] World | Aurora Origin=0,0,0\n")
      require_truth_ini_contains("${tier_prepass}" "${stable_environment_value}"
        "Stable authored environment for host ${host_name} tier ${tier_name}")
    endforeach()
    require_truth_ini_contains("${tier_main}"
      "[Truth 60] Main Effect | Auto Blend=0.25\n"
      "Stable authored exposure for host ${host_name} tier ${tier_name}")
    if(tier_name STREQUAL "performance" OR tier_name STREQUAL "balanced")
      set(expected_lens_enabled false)
    else()
      set(expected_lens_enabled true)
    endif()
    require_truth_ini_contains("${tier_lens}"
      "[Truth 50] Lens | Enabled=${expected_lens_enabled}\n"
      "Lens producer for host ${host_name} tier ${tier_name}")
    require_truth_ini_contains("${tier_main}"
      "[Truth 02] Optical | ENB Lens=${expected_lens_enabled}\n"
      "Lens consumer for host ${host_name} tier ${tier_name}")

    if(tier_name STREQUAL "performance")
      file(READ "${tier_stage_root}/enbdepthoffield.fx.ini" performance_dof)
      file(READ "${tier_stage_root}/enbbloom.fx.ini" performance_bloom)
      file(READ "${tier_stage_root}/enblens.fx.ini" performance_lens)
      file(READ "${tier_stage_root}/enbeffect.fx.ini" performance_main)
      require_truth_ini_contains("${performance_dof}"
        "[Truth 20] Depth of Field | Enabled=false\n"
        "Performance depth-of-field preset")
      require_truth_ini_contains("${performance_bloom}"
        "[Truth 30] Bloom | Enabled=false\n"
        "Performance bloom preset")
      require_truth_ini_contains("${performance_lens}"
        "[Truth 50] Lens | Enabled=false\n"
        "Performance lens preset")
      require_truth_ini_contains("${performance_main}"
        "[Truth 02] Optical | ENB Bloom=false\n"
        "Performance main-effect preset")
      require_truth_ini_contains("${performance_main}"
        "[Truth 02] Optical | ENB Lens=false\n"
        "Performance main-effect preset")
    elseif(tier_name STREQUAL "balanced")
      file(READ "${tier_stage_root}/enbeffectprepass.fx.ini" balanced_prepass)
      file(READ "${tier_stage_root}/enbdepthoffield.fx.ini" balanced_dof)
      file(READ "${tier_stage_root}/enbbloom.fx.ini" balanced_bloom)
      file(READ "${tier_stage_root}/enblens.fx.ini" balanced_lens)
      file(READ "${tier_stage_root}/enbeffectpostpass.fx.ini" balanced_postpass)
      file(READ "${tier_stage_root}/enbsunsprite.fx.ini" balanced_sun)
      file(READ "${tier_stage_root}/enbunderwater.fx.ini" balanced_underwater)
      file(READ "${tier_stage_root}/enbeffect.fx.ini" balanced_main)
      require_truth_ini_contains("${balanced_prepass}"
        "[Truth 10] Prepass | Intensity=0.52\n"
        "Balanced prepass preset")
      require_truth_ini_contains("${balanced_dof}"
        "[Truth 20] Depth of Field | Enabled=false\n"
        "Balanced depth-of-field preset")
      require_truth_ini_contains("${balanced_dof}"
        "[Truth 20] Depth of Field | Intensity=0.12\n"
        "Balanced depth-of-field preset")
      require_truth_ini_contains("${balanced_bloom}"
        "[Truth 30] Bloom | Intensity=0.20\n"
        "Balanced bloom preset")
      require_truth_ini_contains("${balanced_lens}"
        "[Truth 50] Lens | Enabled=false\n"
        "Balanced lens preset")
      require_truth_ini_contains("${balanced_lens}"
        "[Truth 50] Lens | Intensity=0.08\n"
        "Balanced lens preset")
      require_truth_ini_contains("${balanced_postpass}"
        "[Truth 70] Postpass | Intensity=0.45\n"
        "Balanced postpass preset")
      require_truth_ini_contains("${balanced_sun}"
        "[Truth 80] Sun Sprite | Intensity=0.18\n"
        "Balanced sun-sprite preset")
      require_truth_ini_contains("${balanced_underwater}"
        "[Truth 90] Underwater | Intensity=0.32\n"
        "Balanced underwater preset")
      require_truth_ini_contains("${balanced_main}"
        "[Truth 02] Optical | ENB Lens=false\n"
        "Balanced main-effect preset")
      require_truth_ini_contains("${balanced_main}"
        "[Truth 60] Main Effect | Auto Blend=0.25\n"
        "Balanced main-effect preset")
    elseif(tier_name STREQUAL "cinematic")
      file(READ "${tier_stage_root}/enbeffectprepass.fx.ini" cinematic_prepass)
      file(READ "${tier_stage_root}/enbbloom.fx.ini" cinematic_bloom)
      file(READ "${tier_stage_root}/enblens.fx.ini" cinematic_lens)
      file(READ "${tier_stage_root}/enbsunsprite.fx.ini" cinematic_sun)
      file(READ "${tier_stage_root}/enbunderwater.fx.ini" cinematic_underwater)
      file(READ "${tier_stage_root}/enbeffect.fx.ini" cinematic_main)
      require_truth_ini_contains("${cinematic_prepass}"
        "[Truth 10] Prepass | Intensity=0.52\n"
        "Cinematic prepass preset")
      require_truth_ini_contains("${cinematic_bloom}"
        "[Truth 30] Bloom | Intensity=0.36\n"
        "Cinematic bloom preset")
      require_truth_ini_contains("${cinematic_lens}"
        "[Truth 50] Lens | Intensity=0.22\n"
        "Cinematic lens preset")
      require_truth_ini_contains("${cinematic_sun}"
        "[Truth 80] Sun Sprite | Intensity=0.38\n"
        "Cinematic sun-sprite preset")
      require_truth_ini_contains("${cinematic_underwater}"
        "[Truth 90] Underwater | Intensity=0.50\n"
        "Cinematic underwater preset")
      reject_truth_ini_contains("${cinematic_prepass}" "Intensity=1.0\n"
        "Cinematic preset boundedness")
      reject_truth_ini_contains("${cinematic_bloom}" "Intensity=1.0\n"
        "Cinematic preset boundedness")
      reject_truth_ini_contains("${cinematic_lens}" "Intensity=1.0\n"
        "Cinematic preset boundedness")
      reject_truth_ini_contains("${cinematic_main}" "Intensity=1.0\n"
        "Cinematic preset boundedness")
    endif()
  endforeach()
endforeach()

# The host axis is only real if the two hosts disagree. Assert the postpass keys
# differ, so a generator that silently wrote the same file twice fails here
# rather than shipping a variant that double-processes under Effects 11.
foreach(tier_name IN LISTS expected_tiers)
  file(READ "${first_output}/enbseries/${tier_name}/ROOT/enbseries/enbeffectpostpass.fx.ini"
    enbseries_postpass_contents)
  file(READ "${first_output}/effects11/${tier_name}/ROOT/enbseries/enbeffectpostpass.fx.ini"
    effects11_postpass_contents)
  string(FIND "${enbseries_postpass_contents}" "[Truth 70] Postpass | Vignette Strength=0.18"
    enbseries_vignette_position)
  if(enbseries_vignette_position EQUAL -1)
    message(FATAL_ERROR
      "The ENBSeries variant must keep its vignette at 0.18 for tier ${tier_name}")
  endif()
  string(FIND "${effects11_postpass_contents}" "[Truth 70] Postpass | Vignette Strength=0.0"
    effects11_vignette_position)
  if(effects11_vignette_position EQUAL -1)
    message(FATAL_ERROR
      "The Effects 11 variant must zero its vignette for tier ${tier_name}")
  endif()
endforeach()

set(manifest_header
  "tier,id,label,cloud_mode,cloud_primary_steps,cloud_light_steps,aurora_samples,ao_directions,ao_steps,dof_rings,bloom_radius,ssr_steps")
list(GET canonical_quality_rows 0 row_0)
list(GET canonical_quality_rows 1 row_1)
list(GET canonical_quality_rows 2 row_2)
list(GET canonical_quality_rows 3 row_3)
list(GET canonical_quality_rows 4 row_4)

expect_truth_quality_generator_rejection(
  "missing-tier"
  "${manifest_header}\n${row_0}\n${row_1}\n${row_2}\n${row_3}\n")
expect_truth_quality_generator_rejection(
  "duplicate-tier"
  "${manifest_header}\n${row_0}\n${row_0}\n${row_2}\n${row_3}\n${row_4}\n")
expect_truth_quality_generator_rejection(
  "unexpected-id"
  "${manifest_header}\n${row_0}\n1,unexpected,Balanced,analytic,0,0,2,6,3,2,3,0\n${row_2}\n${row_3}\n${row_4}\n")
expect_truth_quality_generator_rejection(
  "invalid-integer"
  "${manifest_header}\n${row_0}\n1,balanced,Balanced,analytic,zero,0,2,6,3,2,3,0\n${row_2}\n${row_3}\n${row_4}\n")
expect_truth_quality_generator_rejection(
  "canonical-numeric-drift"
  "${manifest_header}\n0,performance,Performance,analytic,0,0,999,4,2,0,2,0\n${row_1}\n${row_2}\n${row_3}\n${row_4}\n")

set(unsafe_output "${truth_source_dir}/truth-quality-presets-unsafe")
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DTRUTH_SOURCE_DIR=${truth_source_dir}"
    "-DTRUTH_BINARY_DIR=${truth_binary_dir}"
    "-DTRUTH_OUTPUT_DIR=${unsafe_output}"
    -P "${truth_generator}"
  RESULT_VARIABLE unsafe_output_result
  OUTPUT_VARIABLE unsafe_output_stdout
  ERROR_VARIABLE unsafe_output_stderr)
if(unsafe_output_result EQUAL 0)
  message(FATAL_ERROR "Truth quality generator accepted output outside its build tree")
endif()
