cmake_minimum_required(VERSION 3.28)

foreach(required_variable IN ITEMS
    TRUTH_FXC
    TRUTH_SHADER
    TRUTH_REQUIRED_SOURCE
    TRUTH_INCLUDE
    TRUTH_DEFINE
    TRUTH_OUTPUT
    TRUTH_LISTING)
  if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
    message(FATAL_ERROR "Missing required variable: ${required_variable}")
  endif()
endforeach()

if(NOT EXISTS "${TRUTH_FXC}")
  message(FATAL_ERROR "Exact x64 FXC executable is absent: ${TRUTH_FXC}")
endif()

if(NOT EXISTS "${TRUTH_SHADER}")
  message(FATAL_ERROR "Truth effect source is absent: ${TRUTH_SHADER}")
endif()

if(NOT EXISTS "${TRUTH_REQUIRED_SOURCE}")
  message(FATAL_ERROR "Required Truth shader source is absent: ${TRUTH_REQUIRED_SOURCE}")
endif()

if(DEFINED TRUTH_REQUIRED_SOURCE_2
    AND NOT "${TRUTH_REQUIRED_SOURCE_2}" STREQUAL ""
    AND NOT EXISTS "${TRUTH_REQUIRED_SOURCE_2}")
  message(FATAL_ERROR "Second required Truth shader source is absent: ${TRUTH_REQUIRED_SOURCE_2}")
endif()

if(DEFINED TRUTH_REQUIRED_SOURCE_3
    AND NOT "${TRUTH_REQUIRED_SOURCE_3}" STREQUAL ""
    AND NOT EXISTS "${TRUTH_REQUIRED_SOURCE_3}")
  message(FATAL_ERROR "Third required Truth shader source is absent: ${TRUTH_REQUIRED_SOURCE_3}")
endif()

if(DEFINED TRUTH_REQUIRED_SOURCE_4
    AND NOT "${TRUTH_REQUIRED_SOURCE_4}" STREQUAL ""
    AND NOT EXISTS "${TRUTH_REQUIRED_SOURCE_4}")
  message(FATAL_ERROR "Fourth required Truth shader source is absent: ${TRUTH_REQUIRED_SOURCE_4}")
endif()

if(NOT IS_DIRECTORY "${TRUTH_INCLUDE}")
  message(FATAL_ERROR "Truth shader include directory is absent: ${TRUTH_INCLUDE}")
endif()

cmake_path(GET TRUTH_OUTPUT PARENT_PATH truth_output_directory)
file(MAKE_DIRECTORY "${truth_output_directory}")

set(truth_fxc_command
  "${TRUTH_FXC}"
  /nologo
  /T fx_5_0
  /I "${TRUTH_INCLUDE}"
  "/D${TRUTH_DEFINE}"
)
if(DEFINED TRUTH_SECOND_DEFINE AND NOT "${TRUTH_SECOND_DEFINE}" STREQUAL "")
  list(APPEND truth_fxc_command "/D${TRUTH_SECOND_DEFINE}")
endif()
list(APPEND truth_fxc_command
  /Fo "${TRUTH_OUTPUT}"
  /Fc "${TRUTH_LISTING}"
  "${TRUTH_SHADER}"
)

execute_process(
  COMMAND ${truth_fxc_command}
  RESULT_VARIABLE truth_fxc_result
  OUTPUT_VARIABLE truth_fxc_stdout
  ERROR_VARIABLE truth_fxc_stderr
)

if(NOT truth_fxc_result EQUAL 0)
  message(FATAL_ERROR
    "FXC failed with exit code ${truth_fxc_result}\n"
    "stdout:\n${truth_fxc_stdout}\n"
    "stderr:\n${truth_fxc_stderr}"
  )
endif()

foreach(output_file IN ITEMS "${TRUTH_OUTPUT}" "${TRUTH_LISTING}")
  if(NOT EXISTS "${output_file}")
    message(FATAL_ERROR "FXC reported success without output: ${output_file}")
  endif()
  file(SIZE "${output_file}" output_size)
  if(output_size EQUAL 0)
    message(FATAL_ERROR "FXC emitted an empty output: ${output_file}")
  endif()
endforeach()

message(STATUS "FXC compiler: ${TRUTH_FXC}")
message(STATUS "FXC target: fx_5_0")
message(STATUS "FXC define: ${TRUTH_DEFINE}")
if(DEFINED TRUTH_SECOND_DEFINE AND NOT "${TRUTH_SECOND_DEFINE}" STREQUAL "")
  message(STATUS "FXC second define: ${TRUTH_SECOND_DEFINE}")
endif()
message(STATUS "FXC object: ${TRUTH_OUTPUT}")
message(STATUS "FXC listing: ${TRUTH_LISTING}")
