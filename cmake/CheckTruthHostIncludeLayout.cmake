cmake_minimum_required(VERSION 3.30)

if(NOT DEFINED TRUTH_SOURCE_DIR OR "${TRUTH_SOURCE_DIR}" STREQUAL "")
  message(FATAL_ERROR "Missing required variable: TRUTH_SOURCE_DIR")
endif()
if(NOT IS_DIRECTORY "${TRUTH_SOURCE_DIR}")
  message(FATAL_ERROR "Truth source directory is absent: ${TRUTH_SOURCE_DIR}")
endif()

file(REAL_PATH "${TRUTH_SOURCE_DIR}" truth_source_dir)
set(truth_shader_root "${truth_source_dir}/shaders")
set(truth_public_include_root "${truth_shader_root}/truth")
if(NOT IS_DIRECTORY "${truth_public_include_root}")
  message(FATAL_ERROR
    "Truth public include root is absent: ${truth_public_include_root}")
endif()

file(GLOB truth_public_includes
  RELATIVE "${truth_public_include_root}"
  "${truth_public_include_root}/*.fxh")
list(SORT truth_public_includes)
if(NOT truth_public_includes)
  message(FATAL_ERROR "No public Truth .fxh includes were found")
endif()

set(unqualified_truth_includes)
foreach(public_include IN LISTS truth_public_includes)
  set(public_include_path "${truth_public_include_root}/${public_include}")
  file(READ "${public_include_path}" public_include_source)
  string(REGEX MATCHALL "#[ \t]*include[ \t]+\"[^\"]+\""
    include_directives "${public_include_source}")
  foreach(include_directive IN LISTS include_directives)
    string(REGEX REPLACE ".*\"([^\"]+)\".*" "\\1" include_operand
      "${include_directive}")
    if(include_operand MATCHES "(^|/)Truth[^/]*\\.fxh$")
      if(NOT include_operand MATCHES "^truth/Truth[^/]*\\.fxh$")
        list(APPEND unqualified_truth_includes
          "${public_include}: ${include_operand}")
      endif()
    endif()
    if(include_operand MATCHES "^truth/Truth[^/]*\\.fxh$")
      set(host_resolved_source "${truth_shader_root}/${include_operand}")
      if(NOT EXISTS "${host_resolved_source}")
        message(FATAL_ERROR
          "Public nested include does not resolve from packaged ENB root: "
          "${public_include}: ${include_operand}")
      endif()
    endif()
  endforeach()
endforeach()

if(unqualified_truth_includes)
  string(JOIN "\n  " unqualified_text ${unqualified_truth_includes})
  message(FATAL_ERROR
    "Public nested Truth .fxh includes must be root-qualified for the "
    "Root/enbseries package layout:\n  ${unqualified_text}")
endif()

message(STATUS
  "Truth public nested .fxh includes resolve from the packaged ENB root")
