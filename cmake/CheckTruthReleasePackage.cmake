cmake_minimum_required(VERSION 3.30)

foreach(required IN ITEMS
    TRUTH_BUILD_ROOT
    TRUTH_CONFIGURATION
    TRUTH_WORK_ROOT
    TRUTH_SOURCE_ROOT
    TRUTH_RUNTIME_PLUGIN
    TRUTH_PRESET_ROOT
    TRUTH_CPACK)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "${required} is required")
  endif()
endforeach()

set(truth_stage_names
  enbadaptation.fx
  enbbloom.fx
  enbdepthoffield.fx
  enbeffect.fx
  enbeffectpostpass.fx
  enbeffectprepass.fx
  enblens.fx
  enbsunsprite.fx
  enbunderwater.fx)
set(truth_include_names
  TruthAdaptation.fxh
  TruthAtmosphereCore.fxh
  TruthAuroraCurtain.fxh
  TruthBloom.fxh
  TruthCloudLighting.fxh
  TruthCloudVolume.fxh
  TruthColorCore.fxh
  TruthDepthOfField.fxh
  TruthEffectParameters.fxh
  TruthHostCapabilities.fxh
  TruthInteriorLight.fxh
  TruthLens.fxh
  TruthPipelineCommon.fxh
  TruthPostFinish.fxh
  TruthPrepassCore.fxh
  TruthQuality.fxh
  TruthRuntimeParameters.fxh
  TruthScreenSpace.fxh
  TruthSkyFields.fxh
  TruthSkyViewAdapter.fxh
  TruthStageParameters.fxh
  TruthSunSprite.fxh
  TruthUnderwater.fxh)
set(truth_tier_ids performance balanced quality ultra cinematic)
# Presets ship per host as well as per tier. Effects 11 injects no defines into
# preset shaders, so the host is carried by the generated INI rather than by a
# compile-time branch.
set(truth_host_ids enbseries effects11)
set(truth_preset_names
  enbadaptation.fx.ini
  enbbloom.fx.ini
  enbdepthoffield.fx.ini
  enbeffect.fx.ini
  enbeffectpostpass.fx.ini
  enbeffectprepass.fx.ini
  enblens.fx.ini
  enbsunsprite.fx.ini
  enbunderwater.fx.ini
  truth-quality.ini)

set(expected_files
  "Truth ENB Documentation/CREDITS-AND-PROVENANCE.md"
  "Truth ENB Documentation/LICENSE"
  "Truth ENB Documentation/README.md"
  "Truth ENB Documentation/Runtime.md"
  "Truth ENB Documentation/THIRD_PARTY_NOTICES.md"
  "Truth ENB Documentation/docs/architecture.md"
  "Truth ENB Documentation/docs/release-validation.md"
  "Truth ENB Documentation/enb-runtime-core.lock"
  "Truth ENB Documentation/enb-upstream.lock"
  "Root/enbseries/TruthENBRuntime.dllplugin"
  "Root/enbseries/enb/ENBSeries0504VanillaPostProcess.fxh")
foreach(stage IN LISTS truth_stage_names)
  list(APPEND expected_files "Root/enbseries/${stage}")
endforeach()
foreach(include IN LISTS truth_include_names)
  list(APPEND expected_files "Root/enbseries/truth/${include}")
endforeach()
foreach(host IN LISTS truth_host_ids)
  foreach(tier IN LISTS truth_tier_ids)
    foreach(preset IN LISTS truth_preset_names)
      list(APPEND expected_files
        "Presets/${host}/${tier}/ROOT/enbseries/${preset}")
    endforeach()
  endforeach()
endforeach()
list(SORT expected_files)

function(truth_expected_source relative output)
  if(relative STREQUAL "Truth ENB Documentation/LICENSE")
    set(source "${TRUTH_SOURCE_ROOT}/LICENSE")
  elseif(relative STREQUAL "Truth ENB Documentation/README.md")
    set(source "${TRUTH_SOURCE_ROOT}/README.md")
  elseif(relative STREQUAL "Truth ENB Documentation/Runtime.md")
    set(source "${TRUTH_SOURCE_ROOT}/runtime/README.md")
  elseif(relative STREQUAL "Truth ENB Documentation/CREDITS-AND-PROVENANCE.md")
    set(source "${TRUTH_SOURCE_ROOT}/CREDITS-AND-PROVENANCE.md")
  elseif(relative STREQUAL "Truth ENB Documentation/THIRD_PARTY_NOTICES.md")
    set(source "${TRUTH_SOURCE_ROOT}/THIRD_PARTY_NOTICES.md")
  elseif(relative STREQUAL "Truth ENB Documentation/docs/architecture.md")
    set(source "${TRUTH_SOURCE_ROOT}/docs/architecture.md")
  elseif(relative STREQUAL "Truth ENB Documentation/docs/release-validation.md")
    set(source "${TRUTH_SOURCE_ROOT}/docs/release-validation.md")
  elseif(relative STREQUAL "Truth ENB Documentation/enb-runtime-core.lock")
    set(source "${TRUTH_SOURCE_ROOT}/runtime/enb-runtime-core.lock")
  elseif(relative STREQUAL "Truth ENB Documentation/enb-upstream.lock")
    set(source "${TRUTH_SOURCE_ROOT}/runtime/enb-upstream.lock")
  elseif(relative STREQUAL "Root/enbseries/TruthENBRuntime.dllplugin")
    set(source "${TRUTH_RUNTIME_PLUGIN}")
  elseif(relative STREQUAL
      "Root/enbseries/enb/ENBSeries0504VanillaPostProcess.fxh")
    set(source
      "${TRUTH_SOURCE_ROOT}/shaders/enb/ENBSeries0504VanillaPostProcess.fxh")
  elseif(relative MATCHES "^Root/enbseries/truth/(.+)$")
    set(source "${TRUTH_SOURCE_ROOT}/shaders/truth/${CMAKE_MATCH_1}")
  elseif(relative MATCHES "^Root/enbseries/(.+\\.fx)$")
    set(source "${TRUTH_SOURCE_ROOT}/shaders/${CMAKE_MATCH_1}")
  elseif(relative MATCHES
      "^Presets/([^/]+)/([^/]+)/ROOT/enbseries/(.+\\.ini)$")
    set(source
      "${TRUTH_PRESET_ROOT}/${CMAKE_MATCH_1}/${CMAKE_MATCH_2}/ROOT/enbseries/${CMAKE_MATCH_3}")
  else()
    message(FATAL_ERROR "No source mapping for package file: ${relative}")
  endif()
  set(${output} "${source}" PARENT_SCOPE)
endfunction()

function(truth_validate_tree root manifest_output)
  file(GLOB_RECURSE actual_files
    RELATIVE "${root}"
    LIST_DIRECTORIES false
    "${root}/*")
  list(SORT actual_files)
  if(NOT actual_files STREQUAL expected_files)
    string(JOIN "\n  " expected_text ${expected_files})
    string(JOIN "\n  " actual_text ${actual_files})
    message(FATAL_ERROR
      "Truth public package manifest mismatch\nExpected:\n  ${expected_text}"
      "\nActual:\n  ${actual_text}")
  endif()

  set(manifest "")
  foreach(relative IN LISTS actual_files)
    string(TOLOWER "${relative}" normalized_relative)
    if(normalized_relative MATCHES
        "(^|/)(private|rc|protected)(/|$)|tools/sky-mesh|\\.(pdb|exe|dll)$")
      message(FATAL_ERROR "Forbidden public package path: ${relative}")
    endif()
    truth_expected_source("${relative}" source)
    set(installed "${root}/${relative}")
    if(NOT EXISTS "${source}")
      message(FATAL_ERROR "Package source is missing: ${source}")
    endif()
    execute_process(
      COMMAND "${CMAKE_COMMAND}" -E compare_files "${source}" "${installed}"
      RESULT_VARIABLE different)
    if(NOT different EQUAL 0)
      message(FATAL_ERROR "Installed bytes differ from source: ${relative}")
    endif()
    file(SIZE "${installed}" size)
    if(size EQUAL 0)
      message(FATAL_ERROR "Package artifact is empty: ${relative}")
    endif()
    file(SHA256 "${installed}" sha256)
    string(APPEND manifest "${relative},${size},${sha256}\n")
  endforeach()
  file(WRITE "${manifest_output}" "${manifest}")
endfunction()

file(REMOVE_RECURSE "${TRUTH_WORK_ROOT}")
file(MAKE_DIRECTORY "${TRUTH_WORK_ROOT}")

foreach(run IN ITEMS first second)
  set(install_root "${TRUTH_WORK_ROOT}/install-${run}")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${TRUTH_BUILD_ROOT}"
      --config "${TRUTH_CONFIGURATION}"
      --component TruthPublicRelease
      --prefix "${install_root}"
    RESULT_VARIABLE install_result
    OUTPUT_VARIABLE install_output
    ERROR_VARIABLE install_error)
  if(NOT install_result EQUAL 0)
    message(FATAL_ERROR
      "Truth public install failed (${install_result})\n"
      "${install_output}\n${install_error}")
  endif()
  truth_validate_tree(
    "${install_root}" "${TRUTH_WORK_ROOT}/manifest-${run}.csv")
endforeach()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E compare_files
    "${TRUTH_WORK_ROOT}/manifest-first.csv"
    "${TRUTH_WORK_ROOT}/manifest-second.csv"
  RESULT_VARIABLE manifest_changed)
if(NOT manifest_changed EQUAL 0)
  message(FATAL_ERROR "Repeated Truth installs changed content hashes")
endif()

foreach(run IN ITEMS first second)
  set(package_root "${TRUTH_WORK_ROOT}/archive-${run}")
  file(MAKE_DIRECTORY "${package_root}")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env SOURCE_DATE_EPOCH=946684800
      "${TRUTH_CPACK}"
      --config "${TRUTH_BUILD_ROOT}/CPackConfig.cmake"
      -C "${TRUTH_CONFIGURATION}"
      -G ZIP
      -B "${package_root}"
    WORKING_DIRECTORY "${TRUTH_BUILD_ROOT}"
    RESULT_VARIABLE package_result
    OUTPUT_VARIABLE package_output
    ERROR_VARIABLE package_error)
  if(NOT package_result EQUAL 0)
    message(FATAL_ERROR
      "Truth ZIP package failed (${package_result})\n"
      "${package_output}\n${package_error}")
  endif()

  set(package_archive "${package_root}/Truth-ENB-1.0.0-win64.zip")
  if(NOT EXISTS "${package_archive}")
    message(FATAL_ERROR "Truth public ZIP was not produced")
  endif()
  file(SIZE "${package_archive}" package_size)
  if(package_size LESS 32768)
    message(FATAL_ERROR "Truth public ZIP is implausibly small")
  endif()
  file(SHA256 "${package_archive}" package_sha256)
  set(checksum_file "${package_archive}.sha256")
  if(NOT EXISTS "${checksum_file}")
    message(FATAL_ERROR "CPack did not emit the Truth ZIP checksum")
  endif()
  file(READ "${checksum_file}" checksum_text)
  string(STRIP "${checksum_text}" checksum_text)
  set(expected_checksum
    "${package_sha256}  Truth-ENB-1.0.0-win64.zip")
  if(NOT checksum_text STREQUAL expected_checksum)
    message(FATAL_ERROR "Truth package checksum sidecar is invalid")
  endif()

  if(run STREQUAL "first")
    set(reference_package_sha256 "${package_sha256}")
    set(extracted "${TRUTH_WORK_ROOT}/extracted")
    file(MAKE_DIRECTORY "${extracted}")
    execute_process(
      COMMAND "${CMAKE_COMMAND}" -E tar xf "${package_archive}"
      WORKING_DIRECTORY "${extracted}"
      RESULT_VARIABLE extract_result
      ERROR_VARIABLE extract_error)
    if(NOT extract_result EQUAL 0)
      message(FATAL_ERROR "Could not extract Truth ZIP: ${extract_error}")
    endif()
    truth_validate_tree(
      "${extracted}" "${TRUTH_WORK_ROOT}/manifest-archive.csv")
  elseif(NOT package_sha256 STREQUAL reference_package_sha256)
    message(FATAL_ERROR "Repeated Truth ZIP packages are not byte-identical")
  endif()
endforeach()

list(LENGTH expected_files expected_file_count)
file(COPY_FILE
  "${TRUTH_WORK_ROOT}/manifest-first.csv"
  "${TRUTH_WORK_ROOT}/package-content-manifest.csv"
  ONLY_IF_DIFFERENT)
message(STATUS
  "Verified Truth ENB public package: ${expected_file_count} files, "
  "${package_size} bytes, SHA-256 ${reference_package_sha256}")
