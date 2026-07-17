cmake_minimum_required(VERSION 3.30)

foreach(required_variable IN ITEMS TRUTH_SOURCE_DIR TRUTH_BINARY_DIR)
  if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
    message(FATAL_ERROR "Missing required variable: ${required_variable}")
  endif()
endforeach()

if(NOT IS_DIRECTORY "${TRUTH_SOURCE_DIR}")
  message(FATAL_ERROR "Truth source directory is absent: ${TRUTH_SOURCE_DIR}")
endif()
if(NOT IS_DIRECTORY "${TRUTH_BINARY_DIR}")
  message(FATAL_ERROR "Truth binary directory is absent: ${TRUTH_BINARY_DIR}")
endif()

file(REAL_PATH "${TRUTH_SOURCE_DIR}" truth_source_dir)
file(REAL_PATH "${TRUTH_BINARY_DIR}" truth_binary_dir)
set(truth_generator "${truth_source_dir}/cmake/GenerateTruthQualityPresets.cmake")
set(truth_quality_include "${truth_source_dir}/shaders/truth/TruthQuality.fxh")
if(NOT EXISTS "${truth_generator}")
  message(FATAL_ERROR "Truth quality generator is absent: ${truth_generator}")
endif()
if(NOT EXISTS "${truth_quality_include}")
  message(FATAL_ERROR "Truth quality include is absent: ${truth_quality_include}")
endif()

file(READ "${truth_quality_include}" quality_include_source)
foreach(required_token IN ITEMS
    "#define TRUTH_QUALITY_TIER 1"
    "#error TRUTH_QUALITY_TIER must be in [0,4]"
    "static const uint TruthQualityTier = TRUTH_QUALITY_TIER;"
    "static const uint TruthQualityCloudPrimarySteps = 16u;"
    "static const uint TruthQualityCloudLightSteps = 4u;"
    "static const uint TruthQualityAuroraSamples = 10u;")
  string(FIND "${quality_include_source}" "${required_token}" token_position)
  if(token_position EQUAL -1)
    message(FATAL_ERROR "Truth quality include is missing required contract token: ${required_token}")
  endif()
endforeach()

set(truth_check_root "${truth_binary_dir}/truth-quality-presets-check")
file(REMOVE_RECURSE "${truth_check_root}")
file(MAKE_DIRECTORY "${truth_check_root}")
set(first_output "${truth_binary_dir}/presets")
set(second_output "${truth_check_root}/second")

function(run_truth_quality_generator output_directory)
  execute_process(
    COMMAND "${CMAKE_COMMAND}"
      "-DTRUTH_SOURCE_DIR=${truth_source_dir}"
      "-DTRUTH_BINARY_DIR=${truth_binary_dir}"
      "-DTRUTH_OUTPUT_DIR=${output_directory}"
      -P "${truth_generator}"
    RESULT_VARIABLE generator_result
    OUTPUT_VARIABLE generator_stdout
    ERROR_VARIABLE generator_stderr)
  if(NOT generator_result EQUAL 0)
    message(FATAL_ERROR
      "Truth quality preset generation failed: ${generator_stdout}${generator_stderr}")
  endif()
endfunction()

function(expect_truth_quality_generator_rejection case_name manifest_contents)
  set(manifest_path "${truth_check_root}/${case_name}.csv")
  set(output_directory "${truth_check_root}/${case_name}-output")
  file(WRITE "${manifest_path}" "${manifest_contents}")
  execute_process(
    COMMAND "${CMAKE_COMMAND}"
      "-DTRUTH_SOURCE_DIR=${truth_source_dir}"
      "-DTRUTH_BINARY_DIR=${truth_binary_dir}"
      "-DTRUTH_QUALITY_CSV=${manifest_path}"
      "-DTRUTH_OUTPUT_DIR=${output_directory}"
      -P "${truth_generator}"
    RESULT_VARIABLE generator_result
    OUTPUT_VARIABLE generator_stdout
    ERROR_VARIABLE generator_stderr)
  if(generator_result EQUAL 0)
    message(FATAL_ERROR
      "Truth quality generator accepted invalid manifest case: ${case_name}")
  endif()
endfunction()

run_truth_quality_generator("${first_output}")
run_truth_quality_generator("${second_output}")

function(hash_truth_quality_tree root output_hash)
  file(GLOB_RECURSE tree_files
    LIST_DIRECTORIES false
    RELATIVE "${root}"
    "${root}/*")
  list(SORT tree_files)
  set(tree_manifest "")
  foreach(relative_path IN LISTS tree_files)
    file(SHA256 "${root}/${relative_path}" file_hash)
    string(APPEND tree_manifest "${relative_path}|${file_hash}\n")
  endforeach()
  string(SHA256 tree_hash "${tree_manifest}")
  set(${output_hash} "${tree_hash}" PARENT_SCOPE)
endfunction()

hash_truth_quality_tree("${first_output}" first_hash)
hash_truth_quality_tree("${second_output}" second_hash)
if(NOT first_hash STREQUAL second_hash)
  message(FATAL_ERROR "Truth quality preset trees are not byte-identical")
endif()

set(expected_tiers performance balanced quality ultra cinematic)
set(canonical_tiers ${expected_tiers})
list(SORT expected_tiers)
file(GLOB first_entries LIST_DIRECTORIES true "${first_output}/*")
set(actual_tiers)
foreach(entry IN LISTS first_entries)
  if(NOT IS_DIRECTORY "${entry}")
    message(FATAL_ERROR "Truth quality preset root contains a non-tier artifact: ${entry}")
  endif()
  get_filename_component(tier_name "${entry}" NAME)
  list(APPEND actual_tiers "${tier_name}")
endforeach()
list(SORT actual_tiers)
if(NOT "${actual_tiers}" STREQUAL "${expected_tiers}")
  message(FATAL_ERROR "Truth quality preset root must contain exactly five canonical tiers")
endif()

file(GLOB_RECURSE first_ini_files
  LIST_DIRECTORIES false
  RELATIVE "${first_output}"
  "${first_output}/*.ini")
list(LENGTH first_ini_files first_ini_count)
if(NOT first_ini_count EQUAL 50)
  message(FATAL_ERROR "Truth quality presets must contain exactly 50 INI files; found ${first_ini_count}")
endif()

foreach(tier_name IN LISTS expected_tiers)
  list(FIND canonical_tiers "${tier_name}" tier_index)
  file(GLOB tier_ini_files
    LIST_DIRECTORIES false
    "${first_output}/${tier_name}/ROOT/enbseries/*.ini")
  list(LENGTH tier_ini_files tier_ini_count)
  if(NOT tier_ini_count EQUAL 10)
    message(FATAL_ERROR
      "Truth quality tier ${tier_name} must contain nine stage INIs and truth-quality.ini")
  endif()
  foreach(tier_ini_file IN LISTS tier_ini_files)
    file(READ "${tier_ini_file}" tier_ini_contents)
    set(expected_header
      "; Generated from config/quality-tiers.csv\n; Product=Truth ENB\n; Tier=${tier_name}\n")
    string(LENGTH "${expected_header}" expected_header_length)
    string(SUBSTRING "${tier_ini_contents}" 0 ${expected_header_length} actual_header)
    if(NOT actual_header STREQUAL expected_header)
      message(FATAL_ERROR "Truth quality preset has an invalid header: ${tier_ini_file}")
    endif()
  endforeach()
  file(READ "${first_output}/${tier_name}/ROOT/enbseries/truth-quality.ini"
    truth_quality_contents)
  set(expected_quality_values
    "[TRUTH QUALITY]\nTRUTH_QUALITY_TIER=${tier_index}\n")
  string(FIND "${truth_quality_contents}" "${expected_quality_values}"
    quality_values_position)
  if(quality_values_position EQUAL -1)
    message(FATAL_ERROR
      "Truth quality metadata is not written as active INI values for tier ${tier_name}")
  endif()
endforeach()

set(manifest_header
  "tier,id,label,cloud_mode,cloud_primary_steps,cloud_light_steps,aurora_samples,ao_directions,ao_steps,dof_rings,bloom_radius,ssr_steps")
set(row_0 "0,performance,Performance,analytic,0,0,1,4,2,0,2,0")
set(row_1 "1,balanced,Balanced,analytic,0,0,2,6,3,2,3,0")
set(row_2 "2,quality,Quality,volume,8,2,4,8,4,3,4,8")
set(row_3 "3,ultra,Ultra,volume,12,3,7,12,5,4,5,12")
set(row_4 "4,cinematic,Cinematic,volume,16,4,10,16,6,5,6,16")

expect_truth_quality_generator_rejection(
  "missing-tier"
  "${manifest_header}\n${row_0}\n${row_1}\n${row_2}\n${row_3}\n")
expect_truth_quality_generator_rejection(
  "duplicate-tier"
  "${manifest_header}\n${row_0}\n${row_0}\n${row_2}\n${row_3}\n${row_4}\n")
expect_truth_quality_generator_rejection(
  "unexpected-id"
  "${manifest_header}\n${row_0}\n1,unexpected,Balanced,analytic,0,0,2,6,3,2,3,0\n${row_2}\n${row_3}\n${row_4}\n")
expect_truth_quality_generator_rejection(
  "invalid-integer"
  "${manifest_header}\n${row_0}\n1,balanced,Balanced,analytic,zero,0,2,6,3,2,3,0\n${row_2}\n${row_3}\n${row_4}\n")

set(unsafe_output "${truth_source_dir}/truth-quality-presets-unsafe")
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DTRUTH_SOURCE_DIR=${truth_source_dir}"
    "-DTRUTH_BINARY_DIR=${truth_binary_dir}"
    "-DTRUTH_OUTPUT_DIR=${unsafe_output}"
    -P "${truth_generator}"
  RESULT_VARIABLE unsafe_output_result
  OUTPUT_VARIABLE unsafe_output_stdout
  ERROR_VARIABLE unsafe_output_stderr)
if(unsafe_output_result EQUAL 0)
  message(FATAL_ERROR "Truth quality generator accepted output outside its build tree")
endif()
