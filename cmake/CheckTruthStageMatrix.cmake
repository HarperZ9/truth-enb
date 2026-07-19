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
set(truth_effect_parameters "${truth_source_dir}/shaders/truth/TruthEffectParameters.fxh")

foreach(required_file IN ITEMS
    "${truth_compile_script}"
    "${truth_common}"
    "${truth_capabilities}"
    "${truth_parameters}"
    "${truth_effect_parameters}")
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

function(require_truth_source_order source_file earlier_token later_token contract_name)
  file(READ "${source_file}" source_contents)
  string(FIND "${source_contents}" "${earlier_token}" earlier_position)
  string(FIND "${source_contents}" "${later_token}" later_position)
  if(earlier_position EQUAL -1 OR later_position EQUAL -1
      OR earlier_position GREATER_EQUAL later_position)
    message(FATAL_ERROR
      "${contract_name} must preserve ${earlier_token} before ${later_token}")
  endif()
endfunction()

set(truth_main_effect "${truth_source_dir}/shaders/enbeffect.fx")
file(READ "${truth_main_effect}" truth_main_effect_source)
string(FIND "${truth_main_effect_source}"
  "#include \"truth/TruthStageParameters.fxh\""
  truth_main_stage_parameters_position)
if(NOT truth_main_stage_parameters_position EQUAL -1)
  message(FATAL_ERROR
    "Main effect must not add generic stage parameters before the runtime ABI")
endif()
require_truth_source_order("${truth_main_effect}"
  "#include \"truth/TruthRuntimeParameters.fxh\""
  "#include \"truth/TruthEffectParameters.fxh\""
  "Main effect runtime ABI")
require_truth_source_order("${truth_main_effect}"
  "#include \"truth/TruthEffectParameters.fxh\""
  "#include \"truth/TruthPipelineCommon.fxh\""
  "Main effect runtime ABI")

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
  "TruthResolveCapability("
  "TruthResolveCapabilityColor(")
require_truth_contract_tokens("${truth_capabilities}" "Truth host capabilities"
  "#define TRUTH_CAPABILITY_IDENTITY 0"
  "#define TRUTH_CAPABILITY_SPATIAL  1"
  "#define TRUTH_CAPABILITY_BRIDGE   2"
  "#define TRUTH_CAPABILITY_NATIVE   3"
  "TRUTH_STAGE_NATIVE_CAPABILITY_AVAILABLE"
  "TRUTH_STAGE_BRIDGE_CAPABILITY_AVAILABLE"
  "TRUTH_STAGE_SPATIAL_CAPABILITY_AVAILABLE"
  "TRUTH_STAGE_OWNS_BRIDGE_VALUE"
  "Full-frame history is unavailable"
  "Object motion vectors are unavailable"
  "Current-frame scratch cannot be treated as persistent history"
  "Cross-effect alpha packing requires an explicit round-trip contract")
require_truth_contract_tokens("${truth_effect_parameters}" "Truth effect parameters"
  "[Truth 00]"
  "[Truth 60]"
  "TruthMasterEnabled")
require_truth_contract_tokens("${truth_parameters}" "Truth stage parameters"
  "[Truth 10]"
  "[Truth 20]"
  "[Truth 30]"
  "[Truth 40]"
  "[Truth 50]"
  "[Truth 70]"
  "[Truth 80]"
  "[Truth 90]"
  "= true;"
  "= 1.0;")
foreach(forbidden_main_parameter IN ITEMS
    "TruthMainEffectEnabled"
    "TruthMainEffectIntensity"
    "TruthMainEffectShoulderShape")
  string(FIND "${truth_main_effect_source}" "${forbidden_main_parameter}"
    forbidden_main_parameter_position)
  if(NOT forbidden_main_parameter_position EQUAL -1)
    message(FATAL_ERROR
      "Main effect ABI contains forbidden generic stage parameter: ${forbidden_main_parameter}")
  endif()
endforeach()

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
file(MAKE_DIRECTORY "${truth_matrix_root}/negative")

function(expect_truth_stage_rejection case_name source_name source_token replacement)
  set(source_path "${truth_source_dir}/shaders/${source_name}")
  file(READ "${source_path}" source_contents)
  string(FIND "${source_contents}" "${source_token}" source_token_position)
  if(source_token_position EQUAL -1)
    message(FATAL_ERROR
      "Negative matrix fixture ${case_name} cannot locate: ${source_token}")
  endif()
  string(REPLACE "${source_token}" "${replacement}" rejected_contents
    "${source_contents}")
  set(rejected_source "${truth_matrix_root}/negative/${case_name}.fx")
  file(WRITE "${rejected_source}" "${rejected_contents}")
  execute_process(
    COMMAND "${CMAKE_COMMAND}"
      "-DTRUTH_FXC=${TRUTH_FXC}"
      "-DTRUTH_SOURCE_DIR=${truth_source_dir}"
      "-DTRUTH_STAGE_SOURCE=${rejected_source}"
      "-DTRUTH_STAGE_TECHNIQUE=Draw"
      "-DTRUTH_QUALITY_TIER=1"
      "-DTRUTH_OUTPUT=${truth_matrix_root}/negative/${case_name}.fxo"
      "-DTRUTH_LISTING=${truth_matrix_root}/negative/${case_name}.asm"
      -P "${truth_compile_script}"
    RESULT_VARIABLE rejected_result
    OUTPUT_VARIABLE rejected_stdout
    ERROR_VARIABLE rejected_stderr)
  if(rejected_result EQUAL 0)
    message(FATAL_ERROR
      "Negative matrix fixture was accepted: ${case_name}\n${rejected_stdout}${rejected_stderr}")
  endif()
endfunction()

expect_truth_stage_rejection("full-frame-history"
  "enbeffectprepass.fx"
  "#define TRUTH_STAGE_OWNS_FULL_FRAME_HISTORY 0"
  "#define TRUTH_STAGE_OWNS_FULL_FRAME_HISTORY 1")
expect_truth_stage_rejection("object-motion"
  "enbeffectprepass.fx"
  "#define TRUTH_STAGE_OWNS_OBJECT_MOTION 0"
  "#define TRUTH_STAGE_OWNS_OBJECT_MOTION 1")
expect_truth_stage_rejection("foreign-scratch-read"
  "enbeffectprepass.fx"
  "#define TRUTH_STAGE_SCRATCH_READ TRUTH_SCRATCH_NONE"
  "#define TRUTH_STAGE_SCRATCH_READ TRUTH_SCRATCH_ADAPTATION")
expect_truth_stage_rejection("cross-effect-alpha-packing"
  "enbeffectprepass.fx"
  "#define TRUTH_STAGE_CROSS_EFFECT_ALPHA_PACKING 0"
  "#define TRUTH_STAGE_CROSS_EFFECT_ALPHA_PACKING 1")
expect_truth_stage_rejection("non-adaptation-scalar-owner"
  "enbeffectprepass.fx"
  "#define TRUTH_STAGE_OWNS_PREVIOUS_SCALAR_ADAPTATION 0"
  "#define TRUTH_STAGE_OWNS_PREVIOUS_SCALAR_ADAPTATION 1")
expect_truth_stage_rejection("non-adaptation-texture-previous"
  "enbeffectprepass.fx"
  "Texture2D TextureColor;"
  "Texture2D TexturePrevious;")

set(truth_capability_probe "${truth_matrix_root}/TruthCapabilityProbe.hlsl")
file(WRITE "${truth_capability_probe}" [=[
#define TRUTH_STAGE_CAPABILITY TRUTH_CAPABILITY_NATIVE
#define TRUTH_STAGE_OWNS_COLOR 0
#define TRUTH_STAGE_OWNS_DEPTH TRUTH_PROBE_SPATIAL_AVAILABLE
#define TRUTH_STAGE_OWNS_NORMAL 0
#define TRUTH_STAGE_OWNS_MASK 0
#define TRUTH_STAGE_OWNS_NATIVE_CELESTIAL_VIEW TRUTH_PROBE_NATIVE_AVAILABLE
#define TRUTH_STAGE_OWNS_PREVIOUS_SCALAR_ADAPTATION 0
#define TRUTH_STAGE_OWNS_BRIDGE_VALUE TRUTH_PROBE_BRIDGE_AVAILABLE
#define TRUTH_STAGE_NATIVE_CAPABILITY_AVAILABLE TRUTH_PROBE_NATIVE_AVAILABLE
#define TRUTH_STAGE_BRIDGE_CAPABILITY_AVAILABLE TRUTH_PROBE_BRIDGE_AVAILABLE
#define TRUTH_STAGE_SPATIAL_CAPABILITY_AVAILABLE TRUTH_PROBE_SPATIAL_AVAILABLE
#define TRUTH_STAGE_SCRATCH_OWNER TRUTH_SCRATCH_NONE
#define TRUTH_STAGE_SCRATCH_READ TRUTH_SCRATCH_NONE
#define TRUTH_STAGE_OWNS_FULL_FRAME_HISTORY 0
#define TRUTH_STAGE_OWNS_OBJECT_MOTION 0
#define TRUTH_STAGE_TREATS_SCRATCH_AS_HISTORY 0
#define TRUTH_STAGE_CROSS_EFFECT_ALPHA_PACKING 0
#include "truth/TruthHostCapabilities.fxh"
#include "truth/TruthPipelineCommon.fxh"

#if TRUTH_PROBE_NATIVE_AVAILABLE
#define TRUTH_PROBE_COMPILED_ROUTE TRUTH_CAPABILITY_NATIVE
#elif TRUTH_PROBE_BRIDGE_AVAILABLE
#define TRUTH_PROBE_COMPILED_ROUTE TRUTH_CAPABILITY_BRIDGE
#elif TRUTH_PROBE_SPATIAL_AVAILABLE
#define TRUTH_PROBE_COMPILED_ROUTE TRUTH_CAPABILITY_SPATIAL
#else
#define TRUTH_PROBE_COMPILED_ROUTE TRUTH_CAPABILITY_IDENTITY
#endif

#if TRUTH_PROBE_COMPILED_ROUTE != TRUTH_PROBE_EXPECTED_ROUTE
#error Truth capability probe did not select the expected ordered route
#endif

uint TruthCapabilityProbeMain() : SV_Target
{
    TruthCapabilityValue selected = TruthResolveCapability(
        TruthMakeCapability(float4(0.1, 0.2, 0.3, 1.0), 1.0),
        TruthMakeCapability(float4(0.2, 0.3, 0.4, 1.0), 1.0),
        TruthMakeCapability(float4(0.3, 0.4, 0.5, 1.0), 1.0),
        TruthMakeCapability(float4(0.4, 0.5, 0.6, 1.0), 1.0));
    return selected.route;
}
]=])

function(run_truth_capability_probe probe_name native_available bridge_available spatial_available expected_route)
  execute_process(
    COMMAND "${TRUTH_FXC}"
      /nologo
      /T ps_5_0
      /E TruthCapabilityProbeMain
      /WX
      /Ges
      /O3
      /I "${truth_source_dir}/shaders"
      "/DTRUTH_PROBE_NATIVE_AVAILABLE=${native_available}"
      "/DTRUTH_PROBE_BRIDGE_AVAILABLE=${bridge_available}"
      "/DTRUTH_PROBE_SPATIAL_AVAILABLE=${spatial_available}"
      "/DTRUTH_PROBE_EXPECTED_ROUTE=${expected_route}"
      /Fo "${truth_matrix_root}/${probe_name}.cso"
      "${truth_capability_probe}"
    RESULT_VARIABLE probe_result
    OUTPUT_VARIABLE probe_stdout
    ERROR_VARIABLE probe_stderr)
  if(NOT probe_result EQUAL 0)
    message(FATAL_ERROR
      "Capability probe ${probe_name} failed.\nstdout:\n${probe_stdout}\nstderr:\n${probe_stderr}")
  endif()
endfunction()

run_truth_capability_probe("capability-native" 1 1 1 3)
run_truth_capability_probe("capability-bridge" 0 1 1 2)
run_truth_capability_probe("capability-spatial" 0 0 1 1)
run_truth_capability_probe("capability-identity" 0 0 0 0)

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

set(truth_prepass_hashes)
foreach(tier RANGE 0 4)
  set(prepass_object "${truth_matrix_root}/${tier}/enbeffectprepass.fxo")
  file(SHA256 "${prepass_object}" prepass_hash)
  list(FIND truth_prepass_hashes "${prepass_hash}" duplicate_hash_index)
  if(NOT duplicate_hash_index EQUAL -1)
    message(FATAL_ERROR
      "Truth tier ${tier} produced duplicate HDR-prepass bytecode; "
      "the canonical quality permutation is not material")
  endif()
  list(APPEND truth_prepass_hashes "${prepass_hash}")
endforeach()

message(STATUS
  "Truth stage matrix compiled 45 strict FXC permutations with five "
  "bytecode-distinct HDR prepasses")

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
