cmake_minimum_required(VERSION 3.30)

if(NOT DEFINED TRUTH_SOURCE_DIR OR "${TRUTH_SOURCE_DIR}" STREQUAL "")
  message(FATAL_ERROR "Missing required variable: TRUTH_SOURCE_DIR")
endif()
file(REAL_PATH "${TRUTH_SOURCE_DIR}" truth_source_dir)

set(truth_main "${truth_source_dir}/shaders/enbeffect.fx")
set(truth_post "${truth_source_dir}/shaders/enbeffectpostpass.fx")
set(truth_sun "${truth_source_dir}/shaders/enbsunsprite.fx")
set(truth_underwater "${truth_source_dir}/shaders/enbunderwater.fx")
set(truth_modules
  "${truth_source_dir}/shaders/truth/TruthPostFinish.fxh"
  "${truth_source_dir}/shaders/truth/TruthSunSprite.fxh"
  "${truth_source_dir}/shaders/truth/TruthUnderwater.fxh")
foreach(required_source IN LISTS truth_modules)
  if(NOT EXISTS "${required_source}")
    message(FATAL_ERROR "Truth composition module is absent: ${required_source}")
  endif()
endforeach()

file(READ "${truth_main}" truth_main_source)
foreach(required_main_token IN ITEMS
    "TruthApplyExposure("
    "TruthFilmicToneCurve3("
    "TruthCompressDisplayGamut("
    "TextureBloom.Sample"
    "TextureLens.Sample")
  string(FIND "${truth_main_source}" "${required_main_token}" token_position)
  if(token_position EQUAL -1)
    message(FATAL_ERROR "Main effect is missing single-owner token: ${required_main_token}")
  endif()
endforeach()
foreach(forbidden_duplicate IN ITEMS
    "TruthApplyExposure(TruthApplyExposure("
    "TruthFilmicToneCurve3(TruthFilmicToneCurve3("
    "TruthCompressDisplayGamut(TruthCompressDisplayGamut(")
  string(FIND "${truth_main_source}" "${forbidden_duplicate}" token_position)
  if(NOT token_position EQUAL -1)
    message(FATAL_ERROR "Main effect duplicates display operation: ${forbidden_duplicate}")
  endif()
endforeach()

file(READ "${truth_post}" truth_post_source)
foreach(required_post_token IN ITEMS
    "#include \"truth/TruthPostFinish.fxh\""
    "TruthFinishLdr("
    "return float4(finished, 1.0)")
  string(FIND "${truth_post_source}" "${required_post_token}" token_position)
  if(token_position EQUAL -1)
    message(FATAL_ERROR "Postpass is missing finishing token: ${required_post_token}")
  endif()
endforeach()
file(READ "${truth_source_dir}/shaders/truth/TruthPostFinish.fxh"
  truth_post_finish_source)
string(FIND "${truth_post_finish_source}"
  "return TruthTriangularDither(uv, finished);"
  dither_last_position)
if(dither_last_position EQUAL -1)
  message(FATAL_ERROR "Truth LDR finish must return triangular dithering last")
endif()
foreach(forbidden_post_token IN ITEMS
    "TruthApplyBloom"
    "TruthApplyLens"
    "TruthEvaluateAtmosphere"
    "TruthEvaluateFog"
    "TruthApplyExposure"
    "TruthFilmicToneCurve")
  string(FIND "${truth_post_source}" "${forbidden_post_token}" token_position)
  if(NOT token_position EQUAL -1)
    message(FATAL_ERROR "Postpass contains forbidden HDR operation: ${forbidden_post_token}")
  endif()
endforeach()

file(READ "${truth_sun}" truth_sun_source)
foreach(required_sun_token IN ITEMS
    "#include \"truth/TruthSunSprite.fxh\""
    "TruthEvaluateSunSprite("
    "TruthRuntimeCelestial")
  string(FIND "${truth_sun_source}" "${required_sun_token}" token_position)
  if(token_position EQUAL -1)
    message(FATAL_ERROR "Sun sprite is missing celestial contract token: ${required_sun_token}")
  endif()
endforeach()

file(READ "${truth_underwater}" truth_underwater_source)
foreach(required_underwater_token IN ITEMS
    "#include \"truth/TruthUnderwater.fxh\""
    "TruthEvaluateUnderwater("
    "TextureDepth")
  string(FIND "${truth_underwater_source}" "${required_underwater_token}" token_position)
  if(token_position EQUAL -1)
    message(FATAL_ERROR "Underwater stage is missing medium token: ${required_underwater_token}")
  endif()
endforeach()
foreach(forbidden_underwater_token IN ITEMS
    "TruthEvaluateAtmosphere"
    "TruthEvaluateFog"
    "TruthApplyLens"
    "TruthLensDirt"
    "GodRay")
  string(FIND "${truth_underwater_source}" "${forbidden_underwater_token}" token_position)
  if(NOT token_position EQUAL -1)
    message(FATAL_ERROR
      "Underwater stage contains incompatible air/lens operation: ${forbidden_underwater_token}")
  endif()
endforeach()

message(STATUS "Truth composition contracts enforce one HDR-to-display owner and dither-last LDR finish")
