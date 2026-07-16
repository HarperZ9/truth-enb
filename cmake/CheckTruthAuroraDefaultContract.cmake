cmake_minimum_required(VERSION 3.28)

if(NOT DEFINED TRUTH_AURORA_SOURCE OR "${TRUTH_AURORA_SOURCE}" STREQUAL "")
  message(FATAL_ERROR "Missing required variable: TRUTH_AURORA_SOURCE")
endif()
if(NOT EXISTS "${TRUTH_AURORA_SOURCE}")
  message(FATAL_ERROR "Truth aurora source is absent: ${TRUTH_AURORA_SOURCE}")
endif()

file(READ "${TRUTH_AURORA_SOURCE}" aurora_source)
if(NOT aurora_source MATCHES
    "#ifndef[ \t]+TRUTH_AURORA_QUALITY[\r\n]+#define[ \t]+TRUTH_AURORA_QUALITY[ \t]+1")
  message(FATAL_ERROR
    "Truth aurora must default to the practical four-sample low tier")
endif()
