cmake_minimum_required(VERSION 3.30)

if(NOT DEFINED TRUTH_SOURCE_DIR OR "${TRUTH_SOURCE_DIR}" STREQUAL "")
  message(FATAL_ERROR "Missing required variable: TRUTH_SOURCE_DIR")
endif()
if(NOT IS_DIRECTORY "${TRUTH_SOURCE_DIR}")
  message(FATAL_ERROR "Truth source directory is absent: ${TRUTH_SOURCE_DIR}")
endif()

file(REAL_PATH "${TRUTH_SOURCE_DIR}" truth_source_dir)
set(truth_screen_space "${truth_source_dir}/shaders/truth/TruthScreenSpace.fxh")
set(truth_prepass_core "${truth_source_dir}/shaders/truth/TruthPrepassCore.fxh")
set(truth_prepass_stage "${truth_source_dir}/shaders/enbeffectprepass.fx")
set(truth_main_effect "${truth_source_dir}/shaders/enbeffect.fx")
set(truth_capabilities "${truth_source_dir}/shaders/truth/TruthHostCapabilities.fxh")
set(truth_pipeline_common "${truth_source_dir}/shaders/truth/TruthPipelineCommon.fxh")
foreach(required_scene_source IN ITEMS
    "${truth_screen_space}"
    "${truth_prepass_core}"
    "${truth_prepass_stage}"
    "${truth_main_effect}")
  if(NOT EXISTS "${required_scene_source}")
    message(FATAL_ERROR "Truth scene source is absent: ${required_scene_source}")
  endif()
endforeach()
foreach(required_contract_source IN ITEMS
    "${truth_capabilities}"
    "${truth_pipeline_common}")
  if(NOT EXISTS "${required_contract_source}")
    message(FATAL_ERROR "Truth screen-space contract dependency is absent: ${required_contract_source}")
  endif()
endforeach()

file(READ "${truth_capabilities}" truth_capability_source)
file(READ "${truth_pipeline_common}" truth_pipeline_source)
foreach(ownership_token IN ITEMS
    "TRUTH_STAGE_OWNS_DEPTH"
    "TRUTH_STAGE_OWNS_NORMAL"
    "TRUTH_STAGE_OWNS_MASK"
    "TRUTH_STAGE_OWNS_FULL_FRAME_HISTORY"
    "TRUTH_STAGE_OWNS_OBJECT_MOTION"
    "TRUTH_STAGE_TREATS_SCRATCH_AS_HISTORY"
    "TRUTH_STAGE_CROSS_EFFECT_ALPHA_PACKING")
  string(FIND "${truth_capability_source}" "${ownership_token}" ownership_position)
  if(ownership_position EQUAL -1)
    message(FATAL_ERROR
      "Truth capability ownership contract is missing token: ${ownership_token}")
  endif()
endforeach()
foreach(pipeline_token IN ITEMS
    "TruthFinite1"
    "TruthSkyMask"
    "TRUTH_DEPTH_CONVENTION_DEVICE_Z_SKY_AT_ONE")
  string(FIND "${truth_pipeline_source}" "${pipeline_token}" pipeline_position)
  if(pipeline_position EQUAL -1)
    message(FATAL_ERROR
      "Truth pipeline contract is missing token: ${pipeline_token}")
  endif()
endforeach()

function(validate_truth_screen_contract candidate_source is_valid rejection_reason)
  set(valid TRUE)
  set(reason "")

  foreach(required_token IN ITEMS
      "#ifndef TRUTH_PIPELINE_COMMON_FXH"
      "#include \"TruthQuality.fxh\""
      "struct TruthScreenSpaceInput"
      "Texture2D scene_texture"
      "Texture2D depth_texture"
      "Texture2D normal_texture"
      "Texture2D material_mask_texture"
      "TruthQualityAODirections"
      "TruthQualityAOSteps"
      "TruthQualitySSRSteps"
      "TruthApplyGTAO"
      "TruthApplySSR"
      "TruthApplySkinDiffusion"
      "TruthApplyScreenSpaceEffects"
      "TruthScreenSpaceGeometryValid"
      "TruthScreenSpaceDepthNormal"
      "TruthScreenSpaceBilateralWeight"
      "#if TRUTH_QUALITY_TIER <= 1"
      "step_index < TruthQualitySSRSteps"
      "return scene;")
    string(FIND "${candidate_source}" "${required_token}" token_position)
    if(token_position EQUAL -1)
      set(valid FALSE)
      set(reason "missing required token: ${required_token}")
      break()
    endif()
  endforeach()

  if(valid)
    foreach(forbidden_token IN ITEMS
        "Texture2D TruthPreviousFrame"
        "Texture2D TruthTemporalHistory"
        "Texture2D TruthMotionVectors"
        "TruthFrameRandom"
        "TruthScreenSpaceJitter"
        "TruthPackedAlpha"
        "roughness")
      string(FIND "${candidate_source}" "${forbidden_token}" token_position)
      if(NOT token_position EQUAL -1)
        set(valid FALSE)
        set(reason "contains forbidden resource or state: ${forbidden_token}")
        break()
      endif()
    endforeach()
  endif()

  set(${is_valid} "${valid}" PARENT_SCOPE)
  set(${rejection_reason} "${reason}" PARENT_SCOPE)
endfunction()

file(READ "${truth_screen_space}" truth_screen_source)
validate_truth_screen_contract("${truth_screen_source}" truth_screen_valid truth_screen_reason)
if(NOT truth_screen_valid)
  message(FATAL_ERROR "Truth screen-space positive contract failed: ${truth_screen_reason}")
endif()

function(expect_truth_screen_contract_rejection case_name candidate_source)
  validate_truth_screen_contract("${candidate_source}" candidate_valid candidate_reason)
  if(candidate_valid)
    message(FATAL_ERROR
      "Truth screen-space contract accepted synthetic negative case: ${case_name}")
  endif()
endfunction()

expect_truth_screen_contract_rejection(
  "previous-frame-resource"
  "${truth_screen_source}\nTexture2D TruthPreviousFrame;")
expect_truth_screen_contract_rejection(
  "temporal-history-resource"
  "${truth_screen_source}\nTexture2D TruthTemporalHistory;")
expect_truth_screen_contract_rejection(
  "motion-vector-resource"
  "${truth_screen_source}\nTexture2D TruthMotionVectors;")
expect_truth_screen_contract_rejection(
  "random-sampling-state"
  "${truth_screen_source}\nfloat TruthFrameRandom = 0.0;")
expect_truth_screen_contract_rejection(
  "screen-jitter-state"
  "${truth_screen_source}\nfloat TruthScreenSpaceJitter = 0.0;")
expect_truth_screen_contract_rejection(
  "cross-effect-packed-channel"
  "${truth_screen_source}\nfloat TruthPackedAlpha = 0.0;")
string(REPLACE "TruthApplySSR" "TruthMissingSSR" missing_ssr_api_source
  "${truth_screen_source}")
expect_truth_screen_contract_rejection(
  "missing-ssr-api"
  "${missing_ssr_api_source}")

file(READ "${truth_prepass_core}" truth_prepass_core_source)
file(READ "${truth_prepass_stage}" truth_prepass_stage_source)
file(READ "${truth_main_effect}" truth_main_effect_source)

foreach(required_prepass_core_token IN ITEMS
    "struct TruthPrepassResult"
    "TruthComposePrepass("
    "TruthRuntimeReady()"
    "TruthEvaluateSkyViewAdapter"
    "TruthResolveCapability("
    "TruthSkyMask("
    "TruthEvaluateInteriorLight"
    "TruthApplyScreenSpaceEffects"
    "TruthFiniteOrBlack"
    "return output;")
  string(FIND "${truth_prepass_core_source}" "${required_prepass_core_token}" token_position)
  if(token_position EQUAL -1)
    message(FATAL_ERROR
      "Truth HDR prepass core is missing required contract token: ${required_prepass_core_token}")
  endif()
endforeach()

foreach(required_prepass_stage_token IN ITEMS
    "#include \"truth/TruthPrepassCore.fxh\""
    "Texture2D TextureColor;"
    "Texture2D TextureDepth;"
    "Texture2D TextureNormal;"
    "Texture2D TextureMask;"
    "TruthComposePrepass("
    "TruthStageIdentity")
  string(FIND "${truth_prepass_stage_source}" "${required_prepass_stage_token}" token_position)
  if(token_position EQUAL -1)
    message(FATAL_ERROR
      "Truth HDR prepass stage is missing required contract token: ${required_prepass_stage_token}")
  endif()
endforeach()

foreach(forbidden_main_environment_token IN ITEMS
    "TruthResolveSkyRadiance"
    "TruthEvaluateSkyViewAdapter"
    "TruthSkyMask("
    "Texture2D TextureDepth;")
  string(FIND "${truth_main_effect_source}" "${forbidden_main_environment_token}" token_position)
  if(NOT token_position EQUAL -1)
    message(FATAL_ERROR
      "Truth main effect retains HDR prepass environment ownership: ${forbidden_main_environment_token}")
  endif()
endforeach()

message(STATUS "Truth scene contracts accepted current-frame screen-space and single-owner HDR prepass")
