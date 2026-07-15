cmake_minimum_required(VERSION 3.28)

foreach(required_variable IN ITEMS TRUTH_FXC TRUTH_SHADER TRUTH_INCLUDE TRUTH_OUTPUT TRUTH_LISTING)
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

if(NOT IS_DIRECTORY "${TRUTH_INCLUDE}")
  message(FATAL_ERROR "Truth shader include directory is absent: ${TRUTH_INCLUDE}")
endif()

cmake_path(GET TRUTH_OUTPUT PARENT_PATH truth_output_directory)
file(MAKE_DIRECTORY "${truth_output_directory}")

execute_process(
  COMMAND "${TRUTH_FXC}"
    /nologo
    /T fx_5_0
    /I "${TRUTH_INCLUDE}"
    /Fo "${TRUTH_OUTPUT}"
    /Fc "${TRUTH_LISTING}"
    "${TRUTH_SHADER}"
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
message(STATUS "FXC object: ${TRUTH_OUTPUT}")
message(STATUS "FXC listing: ${TRUTH_LISTING}")
