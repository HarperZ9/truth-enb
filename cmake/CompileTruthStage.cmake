cmake_minimum_required(VERSION 3.30)

foreach(required_variable IN ITEMS
    TRUTH_FXC
    TRUTH_SOURCE_DIR
    TRUTH_STAGE_SOURCE
    TRUTH_STAGE_TECHNIQUE
    TRUTH_QUALITY_TIER
    TRUTH_OUTPUT
    TRUTH_LISTING)
  if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
    message(FATAL_ERROR "Missing required variable: ${required_variable}")
  endif()
endforeach()

if(NOT EXISTS "${TRUTH_FXC}")
  message(FATAL_ERROR "Exact x64 FXC executable is absent: ${TRUTH_FXC}")
endif()
if(NOT EXISTS "${TRUTH_STAGE_SOURCE}")
  message(FATAL_ERROR "Truth stage source is absent: ${TRUTH_STAGE_SOURCE}")
endif()
if(NOT "${TRUTH_QUALITY_TIER}" MATCHES "^[0-4]$")
  message(FATAL_ERROR "TRUTH_QUALITY_TIER must be an integer in [0,4]")
endif()

file(READ "${TRUTH_STAGE_SOURCE}" truth_stage_contents)
foreach(required_token IN ITEMS
    "#define TRUTH_STAGE_CAPABILITY"
    "#define TRUTH_STAGE_OWNS_COLOR"
    "#define TRUTH_STAGE_OWNS_DEPTH"
    "#define TRUTH_STAGE_OWNS_NORMAL"
    "#define TRUTH_STAGE_OWNS_MASK"
    "#define TRUTH_STAGE_OWNS_NATIVE_CELESTIAL_VIEW"
    "#define TRUTH_STAGE_OWNS_PREVIOUS_SCALAR_ADAPTATION"
    "#define TRUTH_STAGE_SCRATCH_OWNER"
    "#define TRUTH_STAGE_SCRATCH_READ"
    "#define TRUTH_STAGE_OWNS_FULL_FRAME_HISTORY 0"
    "#define TRUTH_STAGE_OWNS_OBJECT_MOTION 0"
    "#define TRUTH_STAGE_TREATS_SCRATCH_AS_HISTORY 0"
    "#define TRUTH_STAGE_CROSS_EFFECT_ALPHA_PACKING 0"
    "#include \"truth/TruthHostCapabilities.fxh\""
    "#include \"truth/TruthPipelineCommon.fxh\""
    "#include \"truth/TruthStageParameters.fxh\""
    "technique11 ${TRUTH_STAGE_TECHNIQUE}")
  string(FIND "${truth_stage_contents}" "${required_token}" token_position)
  if(token_position EQUAL -1)
    message(FATAL_ERROR
      "Truth stage ${TRUTH_STAGE_SOURCE} is missing required contract token: ${required_token}")
  endif()
endforeach()

get_filename_component(truth_stage_name "${TRUTH_STAGE_SOURCE}" NAME)
if(NOT truth_stage_name STREQUAL "enbeffect.fx")
  string(FIND "${truth_stage_contents}" "TruthStageIdentity" identity_position)
  if(identity_position EQUAL -1)
    message(FATAL_ERROR
      "Identity stage ${truth_stage_name} must use the exact TruthStageIdentity output")
  endif()
endif()
string(FIND "${truth_stage_contents}" "ORIGINALPOSTPROCESS" original_postprocess_position)
if(NOT truth_stage_name STREQUAL "enbeffect.fx" AND NOT original_postprocess_position EQUAL -1)
  message(FATAL_ERROR
    "Only enbeffect.fx may define the ENB-reserved ORIGINALPOSTPROCESS technique")
endif()

cmake_path(GET TRUTH_OUTPUT PARENT_PATH truth_output_directory)
cmake_path(GET TRUTH_LISTING PARENT_PATH truth_listing_directory)
file(MAKE_DIRECTORY "${truth_output_directory}" "${truth_listing_directory}")
file(REMOVE "${TRUTH_OUTPUT}" "${TRUTH_LISTING}")

execute_process(
  COMMAND
    "${TRUTH_FXC}"
    /nologo
    /T fx_5_0
    /WX
    /Ges
    /O3
    /I "${TRUTH_SOURCE_DIR}/shaders"
    "/DTRUTH_QUALITY_TIER=${TRUTH_QUALITY_TIER}"
    /Fo "${TRUTH_OUTPUT}"
    /Fc "${TRUTH_LISTING}"
    "${TRUTH_STAGE_SOURCE}"
  RESULT_VARIABLE truth_fxc_result
  OUTPUT_VARIABLE truth_fxc_stdout
  ERROR_VARIABLE truth_fxc_stderr)

string(TOLOWER "${truth_fxc_stdout}${truth_fxc_stderr}" truth_fxc_output)
if(NOT truth_fxc_result EQUAL 0)
  message(FATAL_ERROR
    "FXC failed for ${truth_stage_name}, tier ${TRUTH_QUALITY_TIER}, technique ${TRUTH_STAGE_TECHNIQUE} with exit code ${truth_fxc_result}\n"
    "stdout:\n${truth_fxc_stdout}\n"
    "stderr:\n${truth_fxc_stderr}")
endif()
# FXC emits X4717 for every valid effect-profile compile with D3DCompiler_47.
# It is a compiler deprecation notice, not a source diagnostic, and FXC leaves
# its exit code at zero even with /WX. Every other warning remains fatal.
string(REGEX REPLACE
  "warning x4717: effects deprecated for d3dcompiler_47"
  ""
  truth_fxc_source_diagnostics
  "${truth_fxc_output}")
if(truth_fxc_source_diagnostics MATCHES "warning")
  message(FATAL_ERROR
    "FXC emitted a warning for ${truth_stage_name}, tier ${TRUTH_QUALITY_TIER}:\n${truth_fxc_stdout}${truth_fxc_stderr}")
endif()

foreach(output_file IN ITEMS "${TRUTH_OUTPUT}" "${TRUTH_LISTING}")
  if(NOT EXISTS "${output_file}")
    message(FATAL_ERROR "FXC reported success without output: ${output_file}")
  endif()
  file(SIZE "${output_file}" output_size)
  if(output_size EQUAL 0)
    message(FATAL_ERROR "FXC emitted an empty output: ${output_file}")
  endif()
endforeach()

message(STATUS
  "FXC stage: ${truth_stage_name}; tier: ${TRUTH_QUALITY_TIER}; technique: ${TRUTH_STAGE_TECHNIQUE}")
