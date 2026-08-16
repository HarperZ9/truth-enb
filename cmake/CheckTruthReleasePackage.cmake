cmake_minimum_required(VERSION 3.30)

foreach(required IN ITEMS
    TRUTH_BUILD_ROOT
    TRUTH_CONFIGURATION
    TRUTH_ARCHITECTURE
    TRUTH_WORK_ROOT
    TRUTH_OUTPUT_ROOT
    TRUTH_SOURCE_ROOT
    TRUTH_RUNTIME_PLUGIN
    TRUTH_PRESET_ROOT
    TRUTH_CPACK)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "${required} is required")
  endif()
endforeach()

if(NOT TRUTH_CONFIGURATION STREQUAL "Release")
  message(FATAL_ERROR
    "Truth public package must be produced from Release, not ${TRUTH_CONFIGURATION}")
endif()
string(TOLOWER "${TRUTH_ARCHITECTURE}" truth_architecture)
if(NOT truth_architecture STREQUAL "x64")
  message(FATAL_ERROR
    "Truth win64 public package requires x64, not ${TRUTH_ARCHITECTURE}")
endif()

file(REAL_PATH "${TRUTH_BUILD_ROOT}" truth_build_root)
set(truth_package_validation_root
  "${truth_build_root}/package-validation")
cmake_path(NORMAL_PATH truth_package_validation_root)
cmake_path(ABSOLUTE_PATH TRUTH_WORK_ROOT
  BASE_DIRECTORY "${truth_build_root}"
  NORMALIZE
  OUTPUT_VARIABLE truth_work_root)
cmake_path(IS_PREFIX truth_package_validation_root "${truth_work_root}"
  NORMALIZE truth_work_is_owned)
if(NOT truth_work_is_owned
    OR truth_work_root STREQUAL truth_package_validation_root
    OR truth_work_root STREQUAL truth_build_root)
  message(FATAL_ERROR
    "Truth package work root must be a strict descendant of "
    "${truth_package_validation_root}: ${truth_work_root}")
endif()

set(truth_packages_root "${truth_build_root}/packages")
cmake_path(NORMAL_PATH truth_packages_root)
cmake_path(ABSOLUTE_PATH TRUTH_OUTPUT_ROOT
  BASE_DIRECTORY "${truth_build_root}"
  NORMALIZE
  OUTPUT_VARIABLE truth_output_root)
cmake_path(IS_PREFIX truth_packages_root "${truth_output_root}"
  NORMALIZE truth_output_is_owned)
if(NOT truth_output_is_owned
    OR truth_output_root STREQUAL truth_packages_root
    OR truth_output_root STREQUAL truth_build_root)
  message(FATAL_ERROR
    "Truth package output must be a strict descendant of the package tree: "
    "${truth_output_root}")
endif()

cmake_path(ABSOLUTE_PATH TRUTH_PRESET_ROOT
  BASE_DIRECTORY "${truth_build_root}"
  NORMALIZE
  OUTPUT_VARIABLE truth_configured_preset_root)
set(truth_expected_preset_root "${truth_build_root}/release-presets")
cmake_path(NORMAL_PATH truth_expected_preset_root)
if(NOT truth_configured_preset_root STREQUAL truth_expected_preset_root)
  message(FATAL_ERROR
    "Truth configured preset root escaped its owned build path: "
    "${truth_configured_preset_root}")
endif()

set(TRUTH_BUILD_ROOT "${truth_build_root}")
set(TRUTH_WORK_ROOT "${truth_work_root}")
set(TRUTH_OUTPUT_ROOT "${truth_output_root}")

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
  TruthEnvironmentParameters.fxh
  TruthHostCapabilities.fxh
  TruthInteriorLight.fxh
  TruthLens.fxh
  TruthPipelineCommon.fxh
  TruthPostFinish.fxh
  TruthPrepassCore.fxh
  TruthQuality.fxh
  TruthQualityPresetOverride.fxh
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
  "Root/enbseries/TruthENBRuntime.dllplugin")
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
    list(APPEND expected_files
      "Presets/${host}/${tier}/ROOT/enbseries/truth/TruthQualityPresetOverride.fxh")
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
  elseif(relative MATCHES "^Root/enbseries/truth/(.+)$")
    set(source "${TRUTH_SOURCE_ROOT}/shaders/truth/${CMAKE_MATCH_1}")
  elseif(relative MATCHES "^Root/enbseries/(.+\\.fx)$")
    set(source "${TRUTH_SOURCE_ROOT}/shaders/${CMAKE_MATCH_1}")
  elseif(relative MATCHES
      "^Presets/([^/]+)/([^/]+)/ROOT/enbseries/(.+\\.ini)$")
    set(source
      "${TRUTH_PRESET_ROOT}/${CMAKE_MATCH_1}/${CMAKE_MATCH_2}/ROOT/enbseries/${CMAKE_MATCH_3}")
  elseif(relative MATCHES
      "^Presets/([^/]+)/([^/]+)/ROOT/enbseries/truth/(TruthQualityPresetOverride\\.fxh)$")
    set(source
      "${TRUTH_PRESET_ROOT}/${CMAKE_MATCH_1}/${CMAKE_MATCH_2}/ROOT/enbseries/truth/${CMAKE_MATCH_3}")
  else()
    message(FATAL_ERROR "No source mapping for package file: ${relative}")
  endif()
  set(${output} "${source}" PARENT_SCOPE)
endfunction()

function(truth_require_package_text contents required_text context)
  string(FIND "${contents}" "${required_text}" required_position)
  if(required_position EQUAL -1)
    message(FATAL_ERROR "${context} is missing required text: ${required_text}")
  endif()
endfunction()

function(truth_validate_markdown_links root relative installed contents)
  string(REGEX MATCHALL "\\[[^][]+\\]\\([^)]+\\)" markdown_links "${contents}")
  get_filename_component(installed_parent "${installed}" DIRECTORY)
  foreach(markdown_link IN LISTS markdown_links)
    string(REGEX REPLACE "^.*\\]\\(([^)]+)\\)$" "\\1"
      link_target "${markdown_link}")
    string(REGEX REPLACE "[#?].*$" "" link_target "${link_target}")
    if(link_target STREQUAL ""
        OR link_target MATCHES "^[A-Za-z][A-Za-z0-9+.-]*:")
      continue()
    endif()
    set(link_candidate "${installed_parent}/${link_target}")
    cmake_path(NORMAL_PATH link_candidate OUTPUT_VARIABLE normalized_link)
    cmake_path(IS_PREFIX root "${normalized_link}"
      NORMALIZE link_is_inside_package)
    if(NOT link_is_inside_package OR NOT EXISTS "${normalized_link}")
      message(FATAL_ERROR
        "Broken internal documentation link in ${relative}: ${link_target}")
    endif()
  endforeach()
endfunction()

function(truth_validate_public_text root relative installed)
  if(NOT relative MATCHES "\\.(fx|fxh|ini|md|lock)$"
      AND NOT relative MATCHES "/LICENSE$")
    return()
  endif()

  file(READ "${installed}" contents)
  string(TOLOWER "${contents}" lower_contents)
  foreach(forbidden_text IN ITEMS
      "nexusmods.com/skyrimspecialedition/mods/184607"
      "c:\\dev\\protected"
      "kitsuune-plugins-"
      "-----begin private key-----"
      "-----begin rsa private key-----"
      "-----begin openssh private key-----")
    string(FIND "${lower_contents}" "${forbidden_text}" forbidden_position)
    if(NOT forbidden_position EQUAL -1)
      message(FATAL_ERROR
        "Forbidden public-package content in ${relative}: ${forbidden_text}")
    endif()
  endforeach()
  if(lower_contents MATCHES
      "(api[_-]?key|access[_-]?token|client[_-]?secret|password|passwd)[ \\t]*[:=][ \\t]*[^ \\t\\r\\n]+")
    message(FATAL_ERROR "Possible embedded secret in public package: ${relative}")
  endif()

  if(NOT relative MATCHES "^Truth ENB Documentation/")
    foreach(peer_marker IN ITEMS
        "kreate"
        "aelas"
        "evlas"
        "nativeeditorid"
        "enbworldspaceweatherlists"
        "kiloader")
      string(FIND "${lower_contents}" "${peer_marker}" peer_position)
      if(NOT peer_position EQUAL -1)
        message(FATAL_ERROR
          "Peer-plugin implementation marker in runtime payload ${relative}: "
          "${peer_marker}")
      endif()
    endforeach()
  endif()

  if(relative MATCHES "\\.md$")
    truth_validate_markdown_links(
      "${root}" "${relative}" "${installed}" "${contents}")
  endif()

  if(relative STREQUAL "Truth ENB Documentation/README.md")
    truth_require_package_text("${contents}" "## Install the release archive"
      "Truth public README")
    truth_require_package_text("${contents}"
      "Address Library database that exactly matches"
      "Truth public README")
    truth_require_package_text("${contents}" "not bundled."
      "Truth public README")
    truth_require_package_text("${contents}" "Public upload remains blocked"
      "Truth public README")
  elseif(relative STREQUAL "Truth ENB Documentation/Runtime.md")
    truth_require_package_text("${contents}" "seven SDK `COLOR4` values"
      "Truth runtime guide")
    truth_require_package_text("${contents}" "Protocol `1.1`"
      "Truth runtime guide")
    truth_require_package_text("${contents}" "`Celestial.w` gates"
      "Truth runtime guide")
    truth_require_package_text("${contents}" "Truth Runtime | Celestial"
      "Truth runtime guide")
  elseif(relative STREQUAL
      "Truth ENB Documentation/THIRD_PARTY_NOTICES.md")
    truth_require_package_text("${contents}" "Neither the tool, nifly, nor a"
      "Truth third-party notice")
    truth_require_package_text("${contents}"
      "generated mesh is included in the Truth ENB runtime ZIP"
      "Truth third-party notice")
  elseif(relative STREQUAL
      "Truth ENB Documentation/CREDITS-AND-PROVENANCE.md")
    truth_require_package_text("${contents}" "independently authored"
      "Truth credits")
    truth_require_package_text("${contents}" "Kitsuune / LonelyKitsuune"
      "Truth credits")
  elseif(relative STREQUAL
      "Truth ENB Documentation/docs/release-validation.md")
    truth_require_package_text("${contents}"
      "Nexus Mods: TBD until a Truth ENB page is assigned"
      "Truth release validation")
    truth_require_package_text("${contents}" "Public upload stays blocked"
      "Truth release validation")
  elseif(relative STREQUAL
      "Root/enbseries/truth/TruthQualityPresetOverride.fxh")
    truth_require_package_text("${contents}" "#define TRUTH_QUALITY_TIER 1"
      "Truth base quality override")
  endif()
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
    set(forbidden_path_pattern
      "(^|/)(private|protected|recovered|tests?|tools|compiler[-_]?outputs?|rc)(/|$)|tools/sky-mesh|kitsuune|lonelykitsuune|kreate|aelas|evlas|nativeeditorid|enbworldspaceweatherlists|kiloader|(^|/)\\.env($|\\.)|\\.(pdb|exe|dll|obj|lib|exp|ilk|map|fxo|zip|7z|rar|bin|db)$")
    if(normalized_relative MATCHES "${forbidden_path_pattern}")
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
    if(relative STREQUAL "Root/enbseries/TruthENBRuntime.dllplugin")
      file(READ "${installed}" plugin_magic OFFSET 0 LIMIT 2 HEX)
      string(TOLOWER "${plugin_magic}" plugin_magic)
      if(NOT plugin_magic STREQUAL "4d5a")
        message(FATAL_ERROR "Truth runtime plugin is not a Windows PE image")
      endif()
      file(STRINGS "${installed}" plugin_ascii_strings)
      file(STRINGS "${installed}" plugin_utf16_strings ENCODING UTF-16LE)
      string(JOIN "\n" plugin_public_strings
        ${plugin_ascii_strings} ${plugin_utf16_strings})
      string(TOLOWER "${plugin_public_strings}" lower_plugin_strings)
      foreach(plugin_forbidden_marker IN ITEMS
          "c:\\dev\\protected"
          "kitsuune-plugins-"
          "kreate"
          "aelas"
          "evlas"
          "nativeeditorid"
          "enbworldspaceweatherlists"
          "kiloader"
          "-----begin private key-----"
          "-----begin rsa private key-----"
          "-----begin openssh private key-----")
        string(FIND "${lower_plugin_strings}" "${plugin_forbidden_marker}"
          plugin_marker_position)
        if(NOT plugin_marker_position EQUAL -1)
          message(FATAL_ERROR
            "Forbidden marker embedded in Truth runtime plugin: "
            "${plugin_forbidden_marker}")
        endif()
      endforeach()
      if(lower_plugin_strings MATCHES
          "(api[_-]?key|access[_-]?token|client[_-]?secret|password|passwd)[ \\t]*[:=][ \\t]*[^ \\t\\r\\n]+")
        message(FATAL_ERROR "Possible embedded secret in Truth runtime plugin")
      endif()
    endif()
    truth_validate_public_text("${root}" "${relative}" "${installed}")
    file(SHA256 "${installed}" sha256)
    string(APPEND manifest "${relative},${size},${sha256}\n")
  endforeach()
  file(WRITE "${manifest_output}" "${manifest}")
endfunction()

execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DTRUTH_SOURCE_DIR=${TRUTH_SOURCE_ROOT}"
    "-DTRUTH_BINARY_DIR=${TRUTH_BUILD_ROOT}"
    "-DTRUTH_OUTPUT_DIR=${truth_configured_preset_root}"
    -P "${TRUTH_SOURCE_ROOT}/cmake/GenerateTruthQualityPresets.cmake"
  RESULT_VARIABLE configured_preset_result
  OUTPUT_VARIABLE configured_preset_output
  ERROR_VARIABLE configured_preset_error)
if(NOT configured_preset_result EQUAL 0)
  message(FATAL_ERROR
    "Build-time Truth preset generation failed: "
    "${configured_preset_output}${configured_preset_error}")
endif()

file(REMOVE_RECURSE "${TRUTH_WORK_ROOT}")
file(MAKE_DIRECTORY "${TRUTH_WORK_ROOT}")
set(fresh_preset_root "${TRUTH_WORK_ROOT}/fresh-presets")
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DTRUTH_SOURCE_DIR=${TRUTH_SOURCE_ROOT}"
    "-DTRUTH_BINARY_DIR=${TRUTH_BUILD_ROOT}"
    "-DTRUTH_OUTPUT_DIR=${fresh_preset_root}"
    -P "${TRUTH_SOURCE_ROOT}/cmake/GenerateTruthQualityPresets.cmake"
  RESULT_VARIABLE fresh_preset_result
  OUTPUT_VARIABLE fresh_preset_output
  ERROR_VARIABLE fresh_preset_error)
if(NOT fresh_preset_result EQUAL 0)
  message(FATAL_ERROR
    "Independent Truth preset generation failed: "
    "${fresh_preset_output}${fresh_preset_error}")
endif()
set(TRUTH_PRESET_ROOT "${fresh_preset_root}")

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
    set(reference_package_archive "${package_archive}")
    set(reference_checksum_file "${checksum_file}")
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
file(MAKE_DIRECTORY "${truth_output_root}")
set(public_archive
  "${truth_output_root}/Truth-ENB-1.0.0-win64.zip")
set(public_checksum "${public_archive}.sha256")
set(public_manifest
  "${truth_output_root}/Truth-ENB-1.0.0-win64.manifest.csv")
file(COPY_FILE "${reference_package_archive}" "${public_archive}"
  ONLY_IF_DIFFERENT)
file(COPY_FILE "${reference_checksum_file}" "${public_checksum}"
  ONLY_IF_DIFFERENT)
file(COPY_FILE "${TRUTH_WORK_ROOT}/manifest-first.csv" "${public_manifest}"
  ONLY_IF_DIFFERENT)
file(SHA256 "${public_manifest}" public_manifest_sha256)
message(STATUS
  "Verified Truth ENB public package: ${expected_file_count} files, "
  "${package_size} bytes, SHA-256 ${reference_package_sha256}, "
  "manifest SHA-256 ${public_manifest_sha256}, output ${public_archive}")
