cmake_minimum_required(VERSION 3.28)

foreach(required_variable IN ITEMS TRUTH_AURORA_SOURCE TRUTH_QUALITY_SOURCE)
  if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
    message(FATAL_ERROR "Missing required variable: ${required_variable}")
  endif()
  if(NOT EXISTS "${${required_variable}}")
    message(FATAL_ERROR "Truth quality contract source is absent: ${${required_variable}}")
  endif()
endforeach()

file(READ "${TRUTH_AURORA_SOURCE}" aurora_source)
file(READ "${TRUTH_QUALITY_SOURCE}" quality_source)

if(NOT aurora_source MATCHES
    "static const uint TruthAuroraCurtainSamples = TruthQualityAuroraSamples;")
  message(FATAL_ERROR
    "Truth aurora must consume the canonical five-tier quality contract")
endif()
if(aurora_source MATCHES "TRUTH_AURORA_QUALITY")
  message(FATAL_ERROR
    "Truth aurora still contains the removed standalone quality macro")
endif()
if(NOT quality_source MATCHES
    "#ifndef[ \t]+TRUTH_QUALITY_TIER[\r\n]+#define[ \t]+TRUTH_QUALITY_TIER[ \t]+1")
  message(FATAL_ERROR "Truth quality must default to the Balanced tier")
endif()
if(NOT quality_source MATCHES
    "#elif[ \t]+TRUTH_QUALITY_TIER[ \t]+==[ \t]+1[\r\n]+static const uint TruthQualityCloudPrimarySteps = 0u;[\r\n]+static const uint TruthQualityCloudLightSteps = 0u;[\r\n]+static const uint TruthQualityAuroraSamples = 2u;")
  message(FATAL_ERROR
    "Truth Balanced tier must retain its authored two-sample aurora budget")
endif()
