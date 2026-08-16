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
  "TruthBloom.fxh|TruthApplyBloom|TruthQualityBloomRadius|TruthBloomAdditiveNeutral"
  "TruthAdaptation.fxh|TruthUpdateAdaptedLuminance|TruthAdaptationSafeLuminance|TruthAdaptationDeltaSeconds"
  "TruthLens.fxh|TruthApplyLens|TruthQualityLensGhosts|TruthLensAdditiveNeutral")
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

file(READ "${truth_source_dir}/shaders/enbbloom.fx" truth_bloom_stage_source)
foreach(required_bloom_stage_token IN ITEMS
    "TruthBloomAdditiveNeutral(source.a)"
    "TruthApplyBloom(input.texcoord, source.rgb)")
  string(FIND "${truth_bloom_stage_source}" "${required_bloom_stage_token}" token_position)
  if(token_position EQUAL -1)
    message(FATAL_ERROR
      "Bloom scratch stage is missing additive-neutral contract token: ${required_bloom_stage_token}")
  endif()
endforeach()
string(FIND "${truth_bloom_stage_source}"
  "return TruthStageIdentity(source, false, 0.0)"
  bloom_wrong_identity_position)
if(NOT bloom_wrong_identity_position EQUAL -1)
  message(FATAL_ERROR
    "Bloom scratch stage may not return the scene as a disabled/zero-intensity identity; TextureBloom is additive in enbeffect.fx")
endif()

file(READ "${truth_source_dir}/shaders/enblens.fx" truth_lens_stage_source)
foreach(required_lens_stage_token IN ITEMS
    "TruthLensAdditiveNeutral(bloom.a)"
    "TruthApplyLens(input.texcoord, max(bloom.rgb, 0.0), 0.0)")
  string(FIND "${truth_lens_stage_source}" "${required_lens_stage_token}" token_position)
  if(token_position EQUAL -1)
    message(FATAL_ERROR
      "Lens scratch stage is missing additive-neutral contract token: ${required_lens_stage_token}")
  endif()
endforeach()
string(FIND "${truth_lens_stage_source}"
  "return TruthStageIdentity(bloom, false, 0.0)"
  lens_wrong_identity_position)
if(NOT lens_wrong_identity_position EQUAL -1)
  message(FATAL_ERROR
    "Lens scratch stage may not return bloom as a disabled/zero-intensity identity; TextureLens is additive in enbeffect.fx")
endif()

file(READ "${truth_source_dir}/shaders/enbadaptation.fx" truth_adaptation_stage_source)
foreach(required_adaptation_stage_token IN ITEMS
    "float4 Timer;"
    "TruthAdaptationSafeMeasuredColor("
    "previous_scalar"
    "TruthAdaptationDeltaSeconds(Timer.w)")
  string(FIND "${truth_adaptation_stage_source}" "${required_adaptation_stage_token}" token_position)
  if(token_position EQUAL -1)
    message(FATAL_ERROR
      "Adaptation stage is missing finite sample/delta contract token: ${required_adaptation_stage_token}")
  endif()
endforeach()
string(FIND "${truth_adaptation_stage_source}" "1.0 / 60.0)" fixed_delta_position)
if(NOT fixed_delta_position EQUAL -1)
  message(FATAL_ERROR
    "Adaptation stage may not use a hard-coded fixed delta when Timer.w is available")
endif()

file(READ "${truth_source_dir}/shaders/enbeffect.fx" truth_main_source)
foreach(required_main_adaptation_token IN ITEMS
    "TruthResolveMainAdaptationLuminance("
    "TextureAdaptation.SampleLevel(Sampler0, input.txcoord0, 0.0).x")
  string(FIND "${truth_main_source}" "${required_main_adaptation_token}" token_position)
  if(token_position EQUAL -1)
    message(FATAL_ERROR
      "Main effect is missing robust adaptation sample contract token: ${required_main_adaptation_token}")
  endif()
endforeach()
foreach(required_main_optical_token IN ITEMS
    "float3 bloom_payload"
    "float3 lens_payload"
    "return scene + bloom_payload + lens_payload;")
  string(FIND "${truth_main_source}" "${required_main_optical_token}" token_position)
  if(token_position EQUAL -1)
    message(FATAL_ERROR
      "Main effect is missing exact additive optical composition token: ${required_main_optical_token}")
  endif()
endforeach()
string(FIND "${truth_main_source}" "bloom - color" bloom_subtract_position)
if(NOT bloom_subtract_position EQUAL -1)
  message(FATAL_ERROR
    "Main effect may not subtract the scene from TextureBloom; bloom is an additive payload")
endif()

message(STATUS "Truth optical contracts enforce bounded modules, additive-neutral bloom/lens scratch, and finite adaptation sampling")
