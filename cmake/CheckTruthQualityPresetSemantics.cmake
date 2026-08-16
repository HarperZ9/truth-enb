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

set(truth_generator "${truth_source_dir}/cmake/GenerateTruthQualityPresets.cmake")
set(truth_probe_source "${truth_source_dir}/shaders/truth/TruthPresetTierProbe.hlsl")
set(truth_base_override
  "${truth_source_dir}/shaders/truth/TruthQualityPresetOverride.fxh")
foreach(required_file IN ITEMS
    "${truth_generator}"
    "${truth_probe_source}"
    "${truth_base_override}"
    "${truth_source_dir}/shaders/truth/TruthQuality.fxh")
  if(NOT EXISTS "${required_file}")
    message(FATAL_ERROR "Truth semantic preset check requires source: ${required_file}")
  endif()
endforeach()

set(truth_semantic_root "${truth_binary_dir}/truth-quality-preset-semantics")
set(truth_semantic_preset_root "${truth_semantic_root}/presets")
set(truth_semantic_install_root "${truth_semantic_root}/installed")
set(truth_semantic_output_root "${truth_semantic_root}/compiled")
file(REMOVE_RECURSE "${truth_semantic_root}")
file(MAKE_DIRECTORY
  "${truth_semantic_preset_root}"
  "${truth_semantic_install_root}"
  "${truth_semantic_output_root}")

execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DTRUTH_SOURCE_DIR=${truth_source_dir}"
    "-DTRUTH_BINARY_DIR=${truth_binary_dir}"
    "-DTRUTH_OUTPUT_DIR=${truth_semantic_preset_root}"
    -P "${truth_generator}"
  RESULT_VARIABLE truth_generator_result
  OUTPUT_VARIABLE truth_generator_stdout
  ERROR_VARIABLE truth_generator_stderr)
if(NOT truth_generator_result EQUAL 0)
  message(FATAL_ERROR
    "Truth semantic preset generation failed: "
    "${truth_generator_stdout}${truth_generator_stderr}")
endif()

set(truth_hosts enbseries effects11)
set(truth_tiers performance balanced quality ultra cinematic)

foreach(host IN LISTS truth_hosts)
  foreach(tier_index RANGE 0 4)
    list(GET truth_tiers ${tier_index} tier)
    set(installed_enbseries
      "${truth_semantic_install_root}/${host}/${tier}/ROOT/enbseries")
    set(preset_enbseries
      "${truth_semantic_preset_root}/${host}/${tier}/ROOT/enbseries")
    set(preset_override
      "${preset_enbseries}/truth/TruthQualityPresetOverride.fxh")
    if(NOT EXISTS "${preset_override}")
      message(FATAL_ERROR
        "Generated preset override is absent for semantic check: ${preset_override}")
    endif()

    file(MAKE_DIRECTORY "${installed_enbseries}")
    file(COPY "${truth_source_dir}/shaders/" DESTINATION "${installed_enbseries}")

    # Real install semantics: the full base shader tree is present first, then
    # the selected preset overlay overwrites the installed quality override.
    file(COPY_FILE "${preset_override}"
      "${installed_enbseries}/truth/TruthQualityPresetOverride.fxh")

    set(installed_probe
      "${installed_enbseries}/truth/TruthPresetTierProbe.hlsl")
    foreach(installed_source IN ITEMS
        "${installed_probe}"
        "${installed_enbseries}/truth/TruthQuality.fxh"
        "${installed_enbseries}/truth/TruthQualityPresetOverride.fxh")
      if(NOT EXISTS "${installed_source}")
        message(FATAL_ERROR
          "Installed semantic shader tree is incomplete: ${installed_source}")
      endif()
    endforeach()

    set(output_object
      "${truth_semantic_output_root}/${host}-${tier}-preset-tier.cso")
    set(output_listing
      "${truth_semantic_output_root}/${host}-${tier}-preset-tier.asm")
    set(truth_fxc_command
      "${TRUTH_FXC}"
      /nologo
      /T cs_5_0
      /E TruthPresetTierProbeMain
      /WX
      /Ges
      /O3
      /I "${installed_enbseries}"
      "/DTRUTH_EXPECTED_TIER=${tier_index}"
      /Fo "${output_object}"
      /Fc "${output_listing}"
      "${installed_probe}")
    string(JOIN " " truth_fxc_command_text ${truth_fxc_command})
    string(FIND "${truth_fxc_command_text}" "TRUTH_QUALITY_TIER"
      forbidden_tier_define_position)
    if(NOT forbidden_tier_define_position EQUAL -1)
      message(FATAL_ERROR
        "Semantic preset check must not pass /DTRUTH_QUALITY_TIER")
    endif()

    execute_process(
      COMMAND ${truth_fxc_command}
      RESULT_VARIABLE truth_fxc_result
      OUTPUT_VARIABLE truth_fxc_stdout
      ERROR_VARIABLE truth_fxc_stderr)
    if(NOT truth_fxc_result EQUAL 0)
      message(FATAL_ERROR
        "Truth preset semantic compile failed for host ${host} tier ${tier} "
        "without /DTRUTH_QUALITY_TIER.\nstdout:\n${truth_fxc_stdout}\n"
        "stderr:\n${truth_fxc_stderr}")
    endif()
    foreach(output_file IN ITEMS "${output_object}" "${output_listing}")
      if(NOT EXISTS "${output_file}")
        message(FATAL_ERROR "FXC reported success without output: ${output_file}")
      endif()
      file(SIZE "${output_file}" output_size)
      if(output_size EQUAL 0)
        message(FATAL_ERROR "FXC emitted an empty semantic output: ${output_file}")
      endif()
    endforeach()
  endforeach()
endforeach()

message(STATUS
  "Truth preset semantic gate compiled 10 installed-tree tier probes without "
  "/DTRUTH_QUALITY_TIER")
