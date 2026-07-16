cmake_minimum_required(VERSION 3.28)

foreach(required IN ITEMS
    TRUTH_BUILD_ROOT
    TRUTH_CONFIGURATION
    TRUTH_WORK_ROOT
    TRUTH_SOURCE_ROOT
    TRUTH_RUNTIME_PLUGIN
    TRUTH_CPACK)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "${required} is required")
  endif()
endforeach()

set(expected_files
  "Truth ENB Documentation/LICENSE"
  "Truth ENB Documentation/README.md"
  "Truth ENB Documentation/Runtime.md"
  "Truth ENB Documentation/docs/architecture.md"
  "Truth ENB Documentation/docs/release-validation.md"
  "Truth ENB Documentation/enb-runtime-core.lock"
  "Truth ENB Documentation/enb-upstream.lock"
  "Root/enbseries/TruthENBRuntime.dllplugin"
  "Root/enbseries/enb/ENBSeries0504VanillaPostProcess.fxh"
  "Root/enbseries/enbeffect.fx"
  "Root/enbseries/truth/TruthAtmosphereCore.fxh"
  "Root/enbseries/truth/TruthAuroraCurtain.fxh"
  "Root/enbseries/truth/TruthCloudLighting.fxh"
  "Root/enbseries/truth/TruthColorCore.fxh"
  "Root/enbseries/truth/TruthEffectParameters.fxh"
  "Root/enbseries/truth/TruthRuntimeParameters.fxh"
  "Root/enbseries/truth/TruthSkyFields.fxh"
  "Root/enbseries/truth/TruthSkyViewAdapter.fxh"
)
list(SORT expected_files)

function(truth_expected_source relative output)
  if(relative STREQUAL "Truth ENB Documentation/LICENSE")
    set(source "${TRUTH_SOURCE_ROOT}/LICENSE")
  elseif(relative STREQUAL "Truth ENB Documentation/README.md")
    set(source "${TRUTH_SOURCE_ROOT}/README.md")
  elseif(relative STREQUAL "Truth ENB Documentation/Runtime.md")
    set(source "${TRUTH_SOURCE_ROOT}/runtime/README.md")
  elseif(relative STREQUAL "Truth ENB Documentation/docs/architecture.md")
    set(source "${TRUTH_SOURCE_ROOT}/docs/architecture.md")
  elseif(relative STREQUAL "Truth ENB Documentation/enb-runtime-core.lock")
    set(source "${TRUTH_SOURCE_ROOT}/runtime/enb-runtime-core.lock")
  elseif(relative STREQUAL "Truth ENB Documentation/enb-upstream.lock")
    set(source "${TRUTH_SOURCE_ROOT}/runtime/enb-upstream.lock")
  elseif(relative STREQUAL "Truth ENB Documentation/docs/release-validation.md")
    set(source "${TRUTH_SOURCE_ROOT}/docs/release-validation.md")
  elseif(relative STREQUAL "Root/enbseries/TruthENBRuntime.dllplugin")
    set(source "${TRUTH_RUNTIME_PLUGIN}")
  elseif(relative STREQUAL "Root/enbseries/enbeffect.fx")
    set(source "${TRUTH_SOURCE_ROOT}/shaders/enbeffect.fx")
  elseif(relative MATCHES "^Root/enbseries/enb/(.+)$")
    set(source "${TRUTH_SOURCE_ROOT}/shaders/enb/${CMAKE_MATCH_1}")
  elseif(relative MATCHES "^Root/enbseries/truth/(.+)$")
    set(source "${TRUTH_SOURCE_ROOT}/shaders/truth/${CMAKE_MATCH_1}")
  else()
    message(FATAL_ERROR "package test has no source mapping for ${relative}")
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
      "Truth package manifest mismatch\nExpected:\n  ${expected_text}"
      "\nActual:\n  ${actual_text}")
  endif()

  set(manifest "")
  foreach(relative IN LISTS actual_files)
    truth_expected_source("${relative}" source)
    set(installed "${root}/${relative}")
    if(NOT EXISTS "${source}")
      message(FATAL_ERROR "package source is missing: ${relative}")
    endif()
    execute_process(
      COMMAND "${CMAKE_COMMAND}" -E compare_files "${source}" "${installed}"
      RESULT_VARIABLE different)
    if(NOT different EQUAL 0)
      message(FATAL_ERROR "installed bytes differ from source: ${relative}")
    endif()
    file(SIZE "${installed}" size)
    if(size EQUAL 0)
      message(FATAL_ERROR "package artifact is empty: ${relative}")
    endif()
    file(SHA256 "${installed}" sha256)
    string(APPEND manifest "${relative},${size},${sha256}\n")
  endforeach()
  file(WRITE "${manifest_output}" "${manifest}")
endfunction()

function(truth_validate_documentation_links root)
  set(documentation_root "${root}/Truth ENB Documentation")
  set(readme "${documentation_root}/README.md")
  file(READ "${readme}" readme_text)
  string(REGEX MATCHALL "\\]\\([^)]*\\)" markdown_links "${readme_text}")
  foreach(markdown_link IN LISTS markdown_links)
    string(REGEX REPLACE "^\\]\\((.*)\\)$" "\\1" link_target
      "${markdown_link}")
    if(link_target MATCHES "^(https?://|mailto:|#)")
      continue()
    endif()
    string(REGEX REPLACE "#.*$" "" link_path "${link_target}")
    if(link_path STREQUAL "" OR NOT EXISTS "${documentation_root}/${link_path}")
      message(FATAL_ERROR
        "packaged README has an unresolved local link: ${link_target}")
    endif()
  endforeach()
endfunction()

file(REMOVE_RECURSE "${TRUTH_WORK_ROOT}")
file(MAKE_DIRECTORY "${TRUTH_WORK_ROOT}")

foreach(run IN ITEMS first second)
  set(install_root "${TRUTH_WORK_ROOT}/install-${run}")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${TRUTH_BUILD_ROOT}"
      --config "${TRUTH_CONFIGURATION}"
      --component TruthPrivateRelease
      --prefix "${install_root}"
    RESULT_VARIABLE install_result
    OUTPUT_VARIABLE install_output
    ERROR_VARIABLE install_error)
  if(NOT install_result EQUAL 0)
    message(FATAL_ERROR
      "Truth release install failed (${install_result})\n"
      "${install_output}\n${install_error}")
  endif()
  truth_validate_tree(
    "${install_root}" "${TRUTH_WORK_ROOT}/manifest-${run}.csv")
  truth_validate_documentation_links("${install_root}")
endforeach()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E compare_files
    "${TRUTH_WORK_ROOT}/manifest-first.csv"
    "${TRUTH_WORK_ROOT}/manifest-second.csv"
  RESULT_VARIABLE manifest_changed)
if(NOT manifest_changed EQUAL 0)
  message(FATAL_ERROR "repeated Truth installs changed content hashes")
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

  set(package_archive
    "${package_root}/Truth-ENB-Private-RC-win64.zip")
  if(NOT EXISTS "${package_archive}")
    message(FATAL_ERROR "Truth package archive was not produced")
  endif()
  file(SIZE "${package_archive}" package_size)
  if(package_size LESS 8192)
    message(FATAL_ERROR "Truth package archive is implausibly small")
  endif()
  file(SHA256 "${package_archive}" package_sha256)
  set(checksum_file "${package_archive}.sha256")
  if(NOT EXISTS "${checksum_file}")
    message(FATAL_ERROR "CPack did not emit the Truth ZIP checksum")
  endif()
  file(READ "${checksum_file}" checksum_text)
  string(STRIP "${checksum_text}" checksum_text)
  set(expected_checksum
    "${package_sha256}  Truth-ENB-Private-RC-win64.zip")
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
    truth_validate_documentation_links("${extracted}")
    execute_process(
      COMMAND "${CMAKE_COMMAND}" -E compare_files
        "${TRUTH_WORK_ROOT}/manifest-first.csv"
        "${TRUTH_WORK_ROOT}/manifest-archive.csv"
      RESULT_VARIABLE archive_changed)
    if(NOT archive_changed EQUAL 0)
      message(FATAL_ERROR "Truth ZIP differs from the install boundary")
    endif()
  elseif(NOT package_sha256 STREQUAL reference_package_sha256)
    message(FATAL_ERROR "repeated Truth ZIP packages are not byte-identical")
  endif()
endforeach()

file(COPY_FILE
  "${TRUTH_WORK_ROOT}/manifest-first.csv"
  "${TRUTH_WORK_ROOT}/package-content-manifest.csv"
  ONLY_IF_DIFFERENT)
message(STATUS
  "Verified Truth private RC: 2 byte-identical archives, ${package_size} "
  "bytes, SHA-256 ${reference_package_sha256}, 18 exact content hashes")
