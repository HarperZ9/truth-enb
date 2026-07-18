cmake_minimum_required(VERSION 3.30)

if(NOT DEFINED TRUTH_SOURCE_DIR OR "${TRUTH_SOURCE_DIR}" STREQUAL "")
  message(FATAL_ERROR "Missing required variable: TRUTH_SOURCE_DIR")
endif()

file(REAL_PATH "${TRUTH_SOURCE_DIR}" truth_source_dir)
set(truth_quality "${truth_source_dir}/shaders/truth/TruthQuality.fxh")
set(truth_optical_modules
  "${truth_source_dir}/shaders/truth/TruthDepthOfField.fxh"
  "${truth_source_dir}/shaders/truth/TruthBloom.fxh"
  "${truth_source_dir}/shaders/truth/TruthAdaptation.fxh"
  "${truth_source_dir}/shaders/truth/TruthLens.fxh")

foreach(required_source IN LISTS truth_optical_modules)
  if(NOT EXISTS "${required_source}")
    message(FATAL_ERROR "Truth optical module is absent: ${required_source}")
  endif()
endforeach()

file(READ "${truth_quality}" truth_quality_source)
foreach(required_quality_token IN ITEMS
    "TruthQualityDOFRings = 0u"
    "TruthQualityDOFRings = 2u"
    "TruthQualityDOFRings = 3u"
    "TruthQualityDOFRings = 4u"
    "TruthQualityDOFRings = 5u"
    "TruthQualityBloomRadius = 2u"
    "TruthQualityBloomRadius = 3u"
    "TruthQualityBloomRadius = 4u"
    "TruthQualityBloomRadius = 5u"
    "TruthQualityBloomRadius = 6u"
    "TruthQualityLensGhosts = 0u"
    "TruthQualityLensGhosts = 1u"
    "TruthQualityLensGhosts = 2u"
    "TruthQualityLensGhosts = 3u")
  string(FIND "${truth_quality_source}" "${required_quality_token}" token_position)
  if(token_position EQUAL -1)
    message(FATAL_ERROR
      "Truth quality contract is missing bounded optical token: ${required_quality_token}")
  endif()
endforeach()

set(required_module_tokens
  "TruthDepthOfField.fxh|TruthApplyDepthOfField|TruthQualityDOFRings|return scene"
  "TruthBloom.fxh|TruthApplyBloom|TruthQualityBloomRadius|TruthBloomSoftKnee|return hdr_source"
  "TruthAdaptation.fxh|TruthUpdateAdaptedLuminance|3.0|1.5|return measured"
  "TruthLens.fxh|TruthApplyLens|TruthQualityLensGhosts|return scene")
foreach(module_contract IN LISTS required_module_tokens)
  string(REPLACE "|" ";" module_fields "${module_contract}")
  list(GET module_fields 0 module_name)
  set(module_path "${truth_source_dir}/shaders/truth/${module_name}")
  file(READ "${module_path}" module_source)
  foreach(field_index RANGE 1 3)
    list(GET module_fields ${field_index} required_token)
    string(FIND "${module_source}" "${required_token}" token_position)
    if(token_position EQUAL -1)
      message(FATAL_ERROR
        "${module_name} is missing optical contract token: ${required_token}")
    endif()
  endforeach()
  string(FIND "${module_source}" "discard" discard_position)
  if(NOT discard_position EQUAL -1)
    message(FATAL_ERROR "${module_name} may not discard pixels")
  endif()
endforeach()

set(stage_contracts
  "enbdepthoffield.fx|TruthDepthOfField.fxh|TruthApplyDepthOfField"
  "enbbloom.fx|TruthBloom.fxh|TruthApplyBloom"
  "enbadaptation.fx|TruthAdaptation.fxh|TruthUpdateAdaptedLuminance"
  "enblens.fx|TruthLens.fxh|TruthApplyLens")
foreach(stage_contract IN LISTS stage_contracts)
  string(REPLACE "|" ";" stage_fields "${stage_contract}")
  list(GET stage_fields 0 stage_name)
  list(GET stage_fields 1 include_name)
  list(GET stage_fields 2 function_name)
  file(READ "${truth_source_dir}/shaders/${stage_name}" stage_source)
  foreach(required_stage_token IN ITEMS
      "#include \"truth/${include_name}\""
      "${function_name}"
      "TruthStageIsActive()"
      "TRUTH_STAGE_INTENSITY")
    string(FIND "${stage_source}" "${required_stage_token}" token_position)
    if(token_position EQUAL -1)
      message(FATAL_ERROR
        "${stage_name} is missing optical stage token: ${required_stage_token}")
    endif()
  endforeach()
endforeach()

message(STATUS "Truth optical contracts enforce bounded five-tier modules and exact identities")
