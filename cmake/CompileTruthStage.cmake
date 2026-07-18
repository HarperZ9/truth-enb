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

function(truth_fxc_diagnostics_allowed diagnostics result_variable)
  string(REPLACE "\r\n" "\n" normalized_diagnostics "${diagnostics}")
  string(REPLACE "\r" "\n" normalized_diagnostics "${normalized_diagnostics}")
  string(REPLACE "\n" ";" diagnostic_lines "${normalized_diagnostics}")
  set(diagnostics_allowed TRUE)
  foreach(diagnostic_line IN LISTS diagnostic_lines)
    if(diagnostic_line STREQUAL
        "warning X4717: Effects deprecated for D3DCompiler_47")
      continue()
    endif()
    string(TOLOWER "${diagnostic_line}" normalized_line)
    if(normalized_line MATCHES "warning")
      set(diagnostics_allowed FALSE)
    endif()
  endforeach()
  set(${result_variable} ${diagnostics_allowed} PARENT_SCOPE)
endfunction()

# Keep the D3DCompiler_47 effect-profile exception exact. These embedded
# negatives stop a broad replacement from silently accepting source warnings.
truth_fxc_diagnostics_allowed(
  "warning X4717: Effects deprecated for D3DCompiler_47\n"
  truth_x4717_allowed)
if(NOT truth_x4717_allowed)
  message(FATAL_ERROR "The exact FXC X4717 effect-profile diagnostic must be accepted")
endif()
truth_fxc_diagnostics_allowed(
  "C:/stage.fx(1): warning X4717: Effects deprecated for D3DCompiler_47\n"
  truth_x4717_attributed_allowed)
if(truth_x4717_attributed_allowed)
  message(FATAL_ERROR "The FXC X4717 whitelist accepted a file-attributed warning")
endif()
truth_fxc_diagnostics_allowed(
  "warning X4717: Effects deprecated for D3DCompiler_47 extended\n"
  truth_x4717_extended_allowed)
if(truth_x4717_extended_allowed)
  message(FATAL_ERROR "The FXC X4717 whitelist accepted an extended warning")
endif()
truth_fxc_diagnostics_allowed("warning X3206: implicit truncation\n"
  truth_source_warning_allowed)
if(truth_source_warning_allowed)
  message(FATAL_ERROR "The FXC diagnostic filter accepted a source warning")
endif()

get_filename_component(truth_stage_name "${TRUTH_STAGE_SOURCE}" NAME)
file(READ "${TRUTH_STAGE_SOURCE}" truth_stage_contents)
foreach(required_token IN ITEMS
    "#define TRUTH_STAGE_CAPABILITY"
    "#define TRUTH_STAGE_OWNS_COLOR"
    "#define TRUTH_STAGE_OWNS_DEPTH"
    "#define TRUTH_STAGE_OWNS_NORMAL"
    "#define TRUTH_STAGE_OWNS_MASK"
    "#define TRUTH_STAGE_OWNS_NATIVE_CELESTIAL_VIEW"
    "#define TRUTH_STAGE_OWNS_PREVIOUS_SCALAR_ADAPTATION"
    "#define TRUTH_STAGE_OWNS_BRIDGE_VALUE"
    "#define TRUTH_STAGE_NATIVE_CAPABILITY_AVAILABLE"
    "#define TRUTH_STAGE_BRIDGE_CAPABILITY_AVAILABLE"
    "#define TRUTH_STAGE_SPATIAL_CAPABILITY_AVAILABLE"
    "#define TRUTH_STAGE_SCRATCH_OWNER"
    "#define TRUTH_STAGE_SCRATCH_READ"
    "#define TRUTH_STAGE_OWNS_FULL_FRAME_HISTORY 0"
    "#define TRUTH_STAGE_OWNS_OBJECT_MOTION 0"
    "#define TRUTH_STAGE_TREATS_SCRATCH_AS_HISTORY 0"
    "#define TRUTH_STAGE_CROSS_EFFECT_ALPHA_PACKING 0"
    "#include \"truth/TruthHostCapabilities.fxh\""
    "#include \"truth/TruthPipelineCommon.fxh\""
    "technique11 ${TRUTH_STAGE_TECHNIQUE}")
  string(FIND "${truth_stage_contents}" "${required_token}" token_position)
  if(token_position EQUAL -1)
    message(FATAL_ERROR
      "Truth stage ${TRUTH_STAGE_SOURCE} is missing required contract token: ${required_token}")
  endif()
endforeach()

if(truth_stage_name STREQUAL "enbeffect.fx")
  foreach(required_main_token IN ITEMS
      "#include \"truth/TruthEffectParameters.fxh\""
      "TruthResolveCapabilityColor")
    string(FIND "${truth_stage_contents}" "${required_main_token}" main_token_position)
    if(main_token_position EQUAL -1)
      message(FATAL_ERROR
        "Main effect ${TRUTH_STAGE_SOURCE} is missing required capability/ABI token: ${required_main_token}")
    endif()
  endforeach()
else()
  string(FIND "${truth_stage_contents}"
    "#include \"truth/TruthStageParameters.fxh\""
    stage_parameters_position)
  if(stage_parameters_position EQUAL -1)
    message(FATAL_ERROR
      "Identity stage ${TRUTH_STAGE_SOURCE} is missing TruthStageParameters.fxh")
  endif()
endif()

if(NOT truth_stage_name STREQUAL "enbeffect.fx"
    AND NOT truth_stage_name STREQUAL "enbeffectprepass.fx")
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

string(REGEX MATCHALL "Texture2D[ \t]+[A-Za-z0-9_]+" truth_texture_declarations
  "${truth_stage_contents}")
if(truth_stage_name STREQUAL "enbadaptation.fx")
  list(LENGTH truth_texture_declarations truth_texture_count)
  if(NOT truth_texture_count EQUAL 2)
    message(FATAL_ERROR
      "Adaptation may declare only TextureCurrent and scalar TexturePrevious in this identity release")
  endif()
  foreach(required_texture IN ITEMS "Texture2D TextureCurrent" "Texture2D TexturePrevious")
    list(FIND truth_texture_declarations "${required_texture}" texture_position)
    if(texture_position EQUAL -1)
      message(FATAL_ERROR "Adaptation is missing required scalar-history texture: ${required_texture}")
    endif()
  endforeach()
  string(FIND "${truth_stage_contents}"
    "#define TRUTH_STAGE_OWNS_PREVIOUS_SCALAR_ADAPTATION 1"
    truth_scalar_history_owner_position)
  if(truth_scalar_history_owner_position EQUAL -1)
    message(FATAL_ERROR "TexturePrevious requires the adaptation scalar-history owner declaration")
  endif()
elseif(truth_stage_name STREQUAL "enbeffectprepass.fx")
  list(LENGTH truth_texture_declarations truth_texture_count)
  if(NOT truth_texture_count EQUAL 4)
    message(FATAL_ERROR
      "HDR prepass must declare only its current-frame color, depth, normal, and mask inputs")
  endif()
  foreach(required_texture IN ITEMS
      "Texture2D TextureColor"
      "Texture2D TextureDepth"
      "Texture2D TextureNormal"
      "Texture2D TextureMask")
    list(FIND truth_texture_declarations "${required_texture}" texture_position)
    if(texture_position EQUAL -1)
      message(FATAL_ERROR "HDR prepass is missing required current-frame resource: ${required_texture}")
    endif()
  endforeach()
  string(FIND "${truth_stage_contents}" "TruthComposePrepass" prepass_composition_position)
  if(prepass_composition_position EQUAL -1)
    message(FATAL_ERROR "HDR prepass must use the single TruthComposePrepass owner")
  endif()
  string(FIND "${truth_stage_contents}" "TexturePrevious" truth_previous_position)
  if(NOT truth_previous_position EQUAL -1)
    message(FATAL_ERROR "HDR prepass may not declare previous-frame history")
  endif()
  string(FIND "${truth_stage_contents}"
    "#define TRUTH_STAGE_OWNS_PREVIOUS_SCALAR_ADAPTATION 1"
    truth_prepass_history_owner_position)
  if(NOT truth_prepass_history_owner_position EQUAL -1)
    message(FATAL_ERROR "HDR prepass may not own scalar adaptation history")
  endif()
elseif(truth_stage_name STREQUAL "enbdepthoffield.fx")
  list(LENGTH truth_texture_declarations truth_texture_count)
  if(NOT truth_texture_count EQUAL 2)
    message(FATAL_ERROR
      "Depth of field must declare only current-frame color and depth")
  endif()
  foreach(required_texture IN ITEMS "Texture2D TextureColor" "Texture2D TextureDepth")
    list(FIND truth_texture_declarations "${required_texture}" texture_position)
    if(texture_position EQUAL -1)
      message(FATAL_ERROR "Depth of field is missing required texture: ${required_texture}")
    endif()
  endforeach()
elseif(NOT truth_stage_name STREQUAL "enbeffect.fx")
  list(LENGTH truth_texture_declarations truth_texture_count)
  if(NOT truth_texture_count EQUAL 1
      OR NOT "${truth_texture_declarations}" STREQUAL "Texture2D TextureColor")
    message(FATAL_ERROR
      "Identity stage ${truth_stage_name} may declare only the TextureColor source it reads")
  endif()
  string(FIND "${truth_stage_contents}" "TexturePrevious" truth_previous_position)
  if(NOT truth_previous_position EQUAL -1)
    message(FATAL_ERROR "Only adaptation may declare TexturePrevious scalar history")
  endif()
  string(FIND "${truth_stage_contents}"
    "#define TRUTH_STAGE_OWNS_PREVIOUS_SCALAR_ADAPTATION 1"
    truth_non_adaptation_history_owner_position)
  if(NOT truth_non_adaptation_history_owner_position EQUAL -1)
    message(FATAL_ERROR "Only adaptation may own scalar history")
  endif()
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

if(NOT truth_fxc_result EQUAL 0)
  message(FATAL_ERROR
    "FXC failed for ${truth_stage_name}, tier ${TRUTH_QUALITY_TIER}, technique ${TRUTH_STAGE_TECHNIQUE} with exit code ${truth_fxc_result}\n"
    "stdout:\n${truth_fxc_stdout}\n"
    "stderr:\n${truth_fxc_stderr}")
endif()
truth_fxc_diagnostics_allowed("${truth_fxc_stdout}${truth_fxc_stderr}"
  truth_fxc_diagnostics_are_allowed)
if(NOT truth_fxc_diagnostics_are_allowed)
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
