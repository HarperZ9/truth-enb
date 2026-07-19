cmake_minimum_required(VERSION 3.28)

foreach(required_variable IN ITEMS
    TRUTH_EFFECT
    TRUTH_PREPASS
    TRUTH_ADAPTER
    TRUTH_RUNTIME
    TRUTH_ENB_VANILLA
    TRUTH_COMPILE_SCRIPT)
  if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
    message(FATAL_ERROR "Missing required variable: ${required_variable}")
  endif()
  if(NOT EXISTS "${${required_variable}}")
    message(FATAL_ERROR "Required sky-view contract source is absent: ${${required_variable}}")
  endif()
endforeach()

file(READ "${TRUTH_EFFECT}" effect_source)
file(READ "${TRUTH_PREPASS}" prepass_source)
get_filename_component(truth_shader_dir "${TRUTH_PREPASS}" DIRECTORY)
file(READ "${truth_shader_dir}/truth/TruthPrepassCore.fxh"
  prepass_core_source)
string(APPEND prepass_source "\n${prepass_core_source}")
file(READ "${TRUTH_ADAPTER}" adapter_source)
file(READ "${TRUTH_RUNTIME}" runtime_source)
file(READ "${TRUTH_ENB_VANILLA}" enb_vanilla_source)
file(READ "${TRUTH_COMPILE_SCRIPT}" compile_script_source)
file(SHA256 "${TRUTH_ENB_VANILLA}" enb_vanilla_sha256)

set(expected_enb_vanilla_sha256
  "caf0cf145034474a5a5f4f630ddd95701f16dfad0f1d9732916ed21f0b510f24")
if(NOT enb_vanilla_sha256 STREQUAL expected_enb_vanilla_sha256)
  message(FATAL_ERROR
    "ENBSeries 0.504 vanilla fallback changed: ${enb_vanilla_sha256}")
endif()

foreach(token IN ITEMS "/WX" "/Ges" "/Gis" "/O3")
  string(FIND "${compile_script_source}" "${token}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR
      "Production Truth FXC policy is missing required flag: ${token}")
  endif()
endforeach()

set(required_effect_tokens
  "TextureColor"
  "EInteriorFactor"
  "#include \"enb/ENBSeries0504VanillaPostProcess.fxh\""
  "technique11 TRUTHPASSTHROUGH"
  "TruthEnbFallbackPixel"
  "technique11 ORIGINALPOSTPROCESS <string UIName=\"Vanilla\";> //do not modify this technique"
  "SetPixelShader(CompileShader(ps_5_0, PS_DrawOriginal()))"
)
foreach(token IN LISTS required_effect_tokens)
  string(FIND "${effect_source}" "${token}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR "ENB effect is missing sky-view adapter token: ${token}")
  endif()
endforeach()

set(required_prepass_tokens
  "#include \"truth/TruthSkyViewAdapter.fxh\""
  "#include \"truth/TruthRuntimeParameters.fxh\""
  "#include \"truth/TruthPrepassCore.fxh\""
  "TruthRuntimeBuildInverseViewProjection"
  "TruthRuntimeCameraWorld"
  "TruthAuroraWorldOrigin"
  "TruthRuntimeStatus.w"
  "TruthEvaluateSkyViewAdapter"
  "TruthRuntimeCelestialReady"
  "TextureColor"
  "TextureDepth"
  "ENightDayFactor"
  "EInteriorFactor")
foreach(token IN LISTS required_prepass_tokens)
  string(FIND "${prepass_source}" "${token}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR
      "ENB prepass is missing sky-view adapter token: ${token}")
  endif()
endforeach()

foreach(token IN ITEMS
    "struct VS_INPUT_POST"
    "struct VS_OUTPUT_POST"
    "VS_OUTPUT_POST\tVS_Draw(VS_INPUT_POST IN)"
    "//Vanilla post process. Do not modify"
    "PS_DrawOriginal(VS_OUTPUT_POST IN, float4 v0 : SV_Position0)"
    "scaleduv=Params01[6].xy*IN.txcoord0.xy"
    "TextureAdaptation.Sample(Sampler1, IN.txcoord0.xy).xy"
    "res=Params01[5].w * r1 + r0")
  string(FIND "${enb_vanilla_source}" "${token}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR
      "ENBSeries 0.504 vanilla fallback is missing required token: ${token}")
  endif()
endforeach()

set(required_runtime_tokens
  "Truth Runtime | Inverse VP Row 0"
  "Truth Runtime | Inverse VP Row 1"
  "Truth Runtime | Inverse VP Row 2"
  "Truth Runtime | Inverse VP Row 3"
  "Truth Runtime | Camera World"
  "Truth Runtime | Celestial"
  "Truth Runtime | Status"
  "int UIHidden = 1"
  "TruthRuntimeStatus.x == 1.0 || TruthRuntimeStatus.x == 1.1"
  "TruthRuntimeStatus.y > 0.5"
)
foreach(token IN LISTS required_runtime_tokens)
  string(FIND "${runtime_source}" "${token}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR "Truth runtime shader ABI is missing token: ${token}")
  endif()
endforeach()

foreach(forbidden_token IN ITEMS
    "TruthSkyProjectionScale"
    "cbuffer TruthSkyFieldParameters"
    "TruthInverseViewProjection"
    "float3(projected_view, view_vertical)")
  string(FIND "${effect_source}" "${forbidden_token}" position)
  if(NOT position EQUAL -1)
    message(FATAL_ERROR "ENB effect retains mixed-space path: ${forbidden_token}")
  endif()
endforeach()

set(required_adapter_tokens
  "row_major float4x4"
  "float4 clip_position"
  "mul(input.inverse_view_projection, clip_position)"
  "input.camera_world_position - input.aurora_world_origin"
  "input.engine_world_units_per_aurora_unit"
)
foreach(token IN LISTS required_adapter_tokens)
  string(FIND "${adapter_source}" "${token}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR "HLSL sky-view adapter is missing contract token: ${token}")
  endif()
endforeach()
