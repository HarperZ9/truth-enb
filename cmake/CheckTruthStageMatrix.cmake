cmake_minimum_required(VERSION 3.30)

foreach(required_variable IN ITEMS TRUTH_FXC TRUTH_SOURCE_DIR TRUTH_BINARY_DIR)
  if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
    message(FATAL_ERROR "Missing required variable: ${required_variable}")
  endif()
endforeach()

if(NOT EXISTS "${TRUTH_FXC}")
  message(FATAL_ERROR "Exact x64 FXC executable is absent: ${TRUTH_FXC}")
endif()
if(NOT IS_DIRECTORY "${TRUTH_SOURCE_DIR}")
  message(FATAL_ERROR "Truth source directory is absent: ${TRUTH_SOURCE_DIR}")
endif()
if(NOT IS_DIRECTORY "${TRUTH_BINARY_DIR}")
  message(FATAL_ERROR "Truth binary directory is absent: ${TRUTH_BINARY_DIR}")
endif()

file(REAL_PATH "${TRUTH_SOURCE_DIR}" truth_source_dir)
file(REAL_PATH "${TRUTH_BINARY_DIR}" truth_binary_dir)
set(truth_compile_script "${truth_source_dir}/cmake/CompileTruthStage.cmake")
set(truth_common "${truth_source_dir}/shaders/truth/TruthPipelineCommon.fxh")
set(truth_capabilities "${truth_source_dir}/shaders/truth/TruthHostCapabilities.fxh")
set(truth_parameters "${truth_source_dir}/shaders/truth/TruthStageParameters.fxh")

foreach(required_file IN ITEMS
    "${truth_compile_script}"
    "${truth_common}"
    "${truth_capabilities}"
    "${truth_parameters}")
  if(NOT EXISTS "${required_file}")
    message(FATAL_ERROR "Truth stage matrix requires source: ${required_file}")
  endif()
endforeach()

function(require_truth_contract_tokens source_file contract_name)
  file(READ "${source_file}" contract_source)
  foreach(required_token IN LISTS ARGN)
    string(FIND "${contract_source}" "${required_token}" token_position)
    if(token_position EQUAL -1)
      message(FATAL_ERROR
        "${contract_name} is missing required contract token: ${required_token}")
    endif()
  endforeach()
endfunction()

require_truth_contract_tokens("${truth_common}" "Truth pipeline common"
  "TruthFinite1"
  "TruthFiniteOrBlack"
  "TruthSkyMask"
  "TRUTH_DEPTH_CONVENTION_DEVICE_Z_SKY_AT_ONE"
  "TruthIdentityColor"
  "TruthNativeCapability(TruthCapabilityValue value)"
  "TruthBridgeCapability(TruthCapabilityValue value)"
  "TruthSpatialCapability(TruthCapabilityValue value)"
  "TruthIdentityCapability(TruthCapabilityValue value)"
  "TruthResolveCapability(")
require_truth_contract_tokens("${truth_capabilities}" "Truth host capabilities"
  "#define TRUTH_CAPABILITY_IDENTITY 0"
  "#define TRUTH_CAPABILITY_SPATIAL  1"
  "#define TRUTH_CAPABILITY_BRIDGE   2"
  "#define TRUTH_CAPABILITY_NATIVE   3"
  "Full-frame history is unavailable"
  "Object motion vectors are unavailable"
  "Current-frame scratch cannot be treated as persistent history"
  "Cross-effect alpha packing requires an explicit round-trip contract")
require_truth_contract_tokens("${truth_parameters}" "Truth stage parameters"
  "[Truth 00]"
  "[Truth 10]"
  "[Truth 20]"
  "[Truth 30]"
  "[Truth 40]"
  "[Truth 50]"
  "[Truth 60]"
  "[Truth 70]"
  "[Truth 80]"
  "[Truth 90]"
  "= true;"
  "= 1.0;")

set(truth_stage_rows
  "enbeffectprepass.fx|Draw"
  "enbdepthoffield.fx|Draw"
  "enbbloom.fx|Draw"
  "enbadaptation.fx|Draw"
  "enblens.fx|Draw"
  "enbeffect.fx|Draw"
  "enbeffectpostpass.fx|Draw"
  "enbsunsprite.fx|Draw"
  "enbunderwater.fx|Draw")

set(truth_matrix_root "${truth_binary_dir}/shader-matrix")
file(REMOVE_RECURSE "${truth_matrix_root}")

foreach(tier RANGE 0 4)
  set(truth_tier_root "${truth_matrix_root}/${tier}")
  file(MAKE_DIRECTORY "${truth_tier_root}")
  foreach(stage_row IN LISTS truth_stage_rows)
    string(REPLACE "|" ";" stage_fields "${stage_row}")
    list(GET stage_fields 0 stage_name)
    list(GET stage_fields 1 stage_technique)
    set(stage_source "${truth_source_dir}/shaders/${stage_name}")
    get_filename_component(stage_stem "${stage_name}" NAME_WE)
    execute_process(
      COMMAND "${CMAKE_COMMAND}"
        "-DTRUTH_FXC=${TRUTH_FXC}"
        "-DTRUTH_SOURCE_DIR=${truth_source_dir}"
        "-DTRUTH_STAGE_SOURCE=${stage_source}"
        "-DTRUTH_STAGE_TECHNIQUE=${stage_technique}"
        "-DTRUTH_QUALITY_TIER=${tier}"
        "-DTRUTH_OUTPUT=${truth_tier_root}/${stage_stem}.fxo"
        "-DTRUTH_LISTING=${truth_tier_root}/${stage_stem}.asm"
        -P "${truth_compile_script}"
      RESULT_VARIABLE stage_result
      OUTPUT_VARIABLE stage_stdout
      ERROR_VARIABLE stage_stderr)
    if(NOT stage_result EQUAL 0)
      message(FATAL_ERROR
        "Truth stage matrix failed for ${stage_name}, tier ${tier}.\n"
        "stdout:\n${stage_stdout}\n"
        "stderr:\n${stage_stderr}")
    endif()
  endforeach()
endforeach()

message(STATUS "Truth stage matrix compiled 45 strict FXC permutations")

set(truth_stage_sources
  shaders/enbeffectprepass.fx
  shaders/enbdepthoffield.fx
  shaders/enbbloom.fx
  shaders/enbadaptation.fx
  shaders/enblens.fx
  shaders/enbeffect.fx
  shaders/enbeffectpostpass.fx
  shaders/enbsunsprite.fx
  shaders/enbunderwater.fx)

foreach(stage_source IN LISTS truth_stage_sources)
  if(NOT EXISTS "${TRUTH_SOURCE_DIR}/${stage_source}")
    message(FATAL_ERROR "Truth stage is absent: ${stage_source}")
  endif()
endforeach()
