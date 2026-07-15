# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.28)

project(TruthSkyMeshTools VERSION 0.1.0 LANGUAGES CXX)

if(NOT WIN32)
  message(FATAL_ERROR "Truth Sky Mesh currently requires Windows for independent SHA-256 verification.")
endif()

set(NIFLY_SOURCE_DIR "" CACHE PATH "Path to an external nifly source checkout")
set(TRUTH_SKY_MESH_INSPECTOR "" CACHE FILEPATH "Optional independent sky_nif_inspector executable")

if(NOT NIFLY_SOURCE_DIR OR NOT EXISTS "${NIFLY_SOURCE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR
    "Set NIFLY_SOURCE_DIR to an external nifly source checkout. nifly is an optional "
    "build-time dependency and is never fetched or vendored by this project."
  )
endif()

file(REAL_PATH "${NIFLY_SOURCE_DIR}" NIFLY_SOURCE_DIR_REAL)
file(REAL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../.." TRUTH_REPOSITORY_ROOT_REAL)
string(FIND "${NIFLY_SOURCE_DIR_REAL}/" "${TRUTH_REPOSITORY_ROOT_REAL}/" NIFLY_INSIDE_REPOSITORY)
if(NIFLY_INSIDE_REPOSITORY EQUAL 0)
  message(FATAL_ERROR "NIFLY_SOURCE_DIR must point outside the Truth repository; vendoring is not supported.")
endif()

execute_process(
  COMMAND git -C "${NIFLY_SOURCE_DIR_REAL}" rev-parse HEAD
  RESULT_VARIABLE NIFLY_GIT_RESULT
  OUTPUT_VARIABLE NIFLY_GIT_REVISION
  ERROR_VARIABLE NIFLY_GIT_ERROR
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
string(LENGTH "${NIFLY_GIT_REVISION}" NIFLY_GIT_REVISION_LENGTH)
if(NOT NIFLY_GIT_RESULT EQUAL 0
    OR NOT NIFLY_GIT_REVISION_LENGTH EQUAL 40
    OR NOT NIFLY_GIT_REVISION MATCHES "^[0-9a-fA-F]+$")
  message(FATAL_ERROR
    "nifly provenance could not be determined from ${NIFLY_SOURCE_DIR_REAL}: ${NIFLY_GIT_ERROR}"
  )
endif()

execute_process(
  COMMAND git -C "${NIFLY_SOURCE_DIR_REAL}" status --porcelain --untracked-files=no
  RESULT_VARIABLE NIFLY_STATUS_RESULT
  OUTPUT_VARIABLE NIFLY_TRACKED_CHANGES
  ERROR_VARIABLE NIFLY_STATUS_ERROR
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT NIFLY_STATUS_RESULT EQUAL 0)
  message(FATAL_ERROR
    "nifly working-tree status could not be verified: ${NIFLY_STATUS_ERROR}"
  )
endif()
if(NIFLY_TRACKED_CHANGES)
  message(FATAL_ERROR
    "NIFLY_SOURCE_DIR contains tracked changes. Use a clean checkout so the recorded "
    "revision completely identifies the serializer source."
  )
endif()

set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory("${NIFLY_SOURCE_DIR_REAL}" "${CMAKE_BINARY_DIR}/nifly-build" EXCLUDE_FROM_ALL)

enable_testing()

add_executable(truth_sky_mesh_generate
  src/AtmosphereMesh.cpp
  src/main.cpp
)
target_compile_features(truth_sky_mesh_generate PRIVATE cxx_std_20)
target_include_directories(truth_sky_mesh_generate PRIVATE include)
target_link_libraries(truth_sky_mesh_generate PRIVATE nifly bcrypt)
target_compile_definitions(truth_sky_mesh_generate PRIVATE
  TRUTH_NIFLY_REVISION="${NIFLY_GIT_REVISION}"
)
set_property(TARGET truth_sky_mesh_generate PROPERTY VS_GLOBAL_VcpkgApplocalDeps false)

if(MSVC)
  target_compile_options(truth_sky_mesh_generate PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
endif()

set(TRUTH_SKY_MESH_GENERATED_ROOT "${CMAKE_BINARY_DIR}/generated")
set(TRUTH_SKY_MESH_GENERATED_NIF
  "${TRUTH_SKY_MESH_GENERATED_ROOT}/meshes/sky/atmosphere.nif"
)
set(TRUTH_SKY_MESH_GENERATED_MANIFEST
  "${TRUTH_SKY_MESH_GENERATED_ROOT}/truth-atmosphere-mesh.manifest.json"
)
add_custom_command(
  OUTPUT
    "${TRUTH_SKY_MESH_GENERATED_NIF}"
    "${TRUTH_SKY_MESH_GENERATED_MANIFEST}"
  COMMAND "$<TARGET_FILE:truth_sky_mesh_generate>"
    --output-root "${TRUTH_SKY_MESH_GENERATED_ROOT}"
  DEPENDS truth_sky_mesh_generate
  COMMENT "Generating deterministic Truth atmosphere fallback"
  VERBATIM
)
add_custom_target(truth_generate_atmosphere
  DEPENDS
    "${TRUTH_SKY_MESH_GENERATED_NIF}"
    "${TRUTH_SKY_MESH_GENERATED_MANIFEST}"
)

add_executable(truth_sky_mesh_contract_tests
  tests/AtmosphereNifContractTests.cpp
)
target_compile_features(truth_sky_mesh_contract_tests PRIVATE cxx_std_20)
target_link_libraries(truth_sky_mesh_contract_tests PRIVATE nifly bcrypt)
set_property(TARGET truth_sky_mesh_contract_tests PROPERTY VS_GLOBAL_VcpkgApplocalDeps false)

if(MSVC)
  target_compile_options(truth_sky_mesh_contract_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
endif()

# This literal path intentionally exists before the production target. It lets the
# black-box contract test fail at runtime during the TDD red phase.
if(CMAKE_CONFIGURATION_TYPES)
  set(TRUTH_SKY_MESH_EXPECTED_GENERATOR
    "${CMAKE_BINARY_DIR}/$<CONFIG>/truth_sky_mesh_generate${CMAKE_EXECUTABLE_SUFFIX}"
  )
else()
  set(TRUTH_SKY_MESH_EXPECTED_GENERATOR
    "${CMAKE_BINARY_DIR}/truth_sky_mesh_generate${CMAKE_EXECUTABLE_SUFFIX}"
  )
endif()

set(TRUTH_SKY_MESH_TEST_OUTPUT "${CMAKE_BINARY_DIR}/test-output")

add_test(
  NAME truth_sky_mesh_contract
  COMMAND truth_sky_mesh_contract_tests contract
    "${TRUTH_SKY_MESH_EXPECTED_GENERATOR}"
    "${TRUTH_SKY_MESH_TEST_OUTPUT}/contract"
)

add_test(
  NAME truth_sky_mesh_determinism
  COMMAND truth_sky_mesh_contract_tests determinism
    "${TRUTH_SKY_MESH_EXPECTED_GENERATOR}"
    "${TRUTH_SKY_MESH_TEST_OUTPUT}/determinism"
)

add_test(
  NAME truth_sky_mesh_invalid_cli
  COMMAND truth_sky_mesh_contract_tests invalid-cli
    "${TRUTH_SKY_MESH_EXPECTED_GENERATOR}"
    "${TRUTH_SKY_MESH_TEST_OUTPUT}/invalid-cli"
)

if(TRUTH_SKY_MESH_INSPECTOR)
  if(NOT EXISTS "${TRUTH_SKY_MESH_INSPECTOR}")
    message(FATAL_ERROR "TRUTH_SKY_MESH_INSPECTOR does not exist: ${TRUTH_SKY_MESH_INSPECTOR}")
  endif()

  add_test(
    NAME truth_sky_mesh_independent_inspector
    COMMAND "${CMAKE_COMMAND}"
      "-DINSPECTOR=${TRUTH_SKY_MESH_INSPECTOR}"
      "-DNIF=${TRUTH_SKY_MESH_TEST_OUTPUT}/contract/meshes/sky/atmosphere.nif"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/CheckInspectorOutput.cmake"
  )
  set_tests_properties(truth_sky_mesh_independent_inspector PROPERTIES
    DEPENDS truth_sky_mesh_contract
  )
endif()
