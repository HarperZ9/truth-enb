cmake_minimum_required(VERSION 3.28)

foreach(required_variable IN ITEMS
    TRUTH_COMPILE_SCRIPT
    TRUTH_FXC
    TRUTH_SHADER
    TRUTH_REQUIRED_SOURCE
    TRUTH_REQUIRED_SOURCE_2
    TRUTH_REQUIRED_SOURCE_3
    TRUTH_REQUIRED_SOURCE_4
    TRUTH_INCLUDE
    TRUTH_DEFINE
    TRUTH_SECOND_DEFINE
    TRUTH_OUTPUT
    TRUTH_LISTING
    TRUTH_MAXIMUM_INSTRUCTION_SLOTS
    TRUTH_BASELINE_INSTRUCTION_SLOTS
    TRUTH_BASELINE_REVISION
    TRUTH_QUALITY
    TRUTH_REPORT)
  if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
    message(FATAL_ERROR "Missing required variable: ${required_variable}")
  endif()
endforeach()
if(NOT EXISTS "${TRUTH_COMPILE_SCRIPT}")
  message(FATAL_ERROR "Truth shader compile script is absent: ${TRUTH_COMPILE_SCRIPT}")
endif()

# Compile a uniquely named witness inside this gate. The budget must not rely
# on a listing left behind by another CTest or disappear under a concurrent
# configuration/build process.
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DTRUTH_FXC=${TRUTH_FXC}"
    "-DTRUTH_SHADER=${TRUTH_SHADER}"
    "-DTRUTH_REQUIRED_SOURCE=${TRUTH_REQUIRED_SOURCE}"
    "-DTRUTH_REQUIRED_SOURCE_2=${TRUTH_REQUIRED_SOURCE_2}"
    "-DTRUTH_REQUIRED_SOURCE_3=${TRUTH_REQUIRED_SOURCE_3}"
    "-DTRUTH_REQUIRED_SOURCE_4=${TRUTH_REQUIRED_SOURCE_4}"
    "-DTRUTH_INCLUDE=${TRUTH_INCLUDE}"
    "-DTRUTH_DEFINE=${TRUTH_DEFINE}"
    "-DTRUTH_SECOND_DEFINE=${TRUTH_SECOND_DEFINE}"
    "-DTRUTH_OUTPUT=${TRUTH_OUTPUT}"
    "-DTRUTH_LISTING=${TRUTH_LISTING}"
    -P "${TRUTH_COMPILE_SCRIPT}"
  RESULT_VARIABLE compile_result
  OUTPUT_VARIABLE compile_stdout
  ERROR_VARIABLE compile_stderr
)
if(NOT compile_result EQUAL 0)
  message(FATAL_ERROR
    "Truth budget witness compilation failed with ${compile_result}\n"
    "stdout:\n${compile_stdout}\n"
    "stderr:\n${compile_stderr}"
  )
endif()
string(LENGTH "${TRUTH_BASELINE_REVISION}" baseline_revision_length)
if(NOT baseline_revision_length EQUAL 40 OR
   NOT TRUTH_BASELINE_REVISION MATCHES "^[0-9a-f]+$")
  message(FATAL_ERROR "Truth shader baseline revision must be a full lowercase Git hash")
endif()
if(TRUTH_BASELINE_INSTRUCTION_SLOTS LESS TRUTH_MAXIMUM_INSTRUCTION_SLOTS)
  message(FATAL_ERROR "Truth shader baseline must not be below the active budget")
endif()
if(NOT EXISTS "${TRUTH_LISTING}")
  message(FATAL_ERROR "Truth shader listing is absent: ${TRUTH_LISTING}")
endif()

file(READ "${TRUTH_LISTING}" listing)
string(REGEX MATCHALL "Approximately [0-9]+ instruction slots used" matches "${listing}")
if(NOT matches)
  message(FATAL_ERROR "FXC listing does not contain an instruction-slot witness")
endif()
set(instruction_slots 0)
foreach(match IN LISTS matches)
  string(REGEX MATCH "[0-9]+" slot_count "${match}")
  if(slot_count GREATER instruction_slots)
    set(instruction_slots "${slot_count}")
  endif()
endforeach()

cmake_path(GET TRUTH_REPORT PARENT_PATH report_directory)
file(MAKE_DIRECTORY "${report_directory}")
math(EXPR reduction_basis_points
  "(${TRUTH_BASELINE_INSTRUCTION_SLOTS} - ${instruction_slots}) * 10000 / ${TRUTH_BASELINE_INSTRUCTION_SLOTS}")
file(WRITE "${TRUTH_REPORT}"
  "quality,static_instruction_slots,maximum_static_instruction_slots,pre_optimization_static_instruction_slots,baseline_revision,reduction_basis_points,width,height,pixels,static_slot_pixel_estimate\n")
foreach(resolution IN ITEMS "1920;1080" "2560;1440" "3840;2160")
  list(GET resolution 0 width)
  list(GET resolution 1 height)
  math(EXPR pixels "${width} * ${height}")
  math(EXPR instruction_pixels "${instruction_slots} * ${pixels}")
  file(APPEND "${TRUTH_REPORT}"
    "${TRUTH_QUALITY},${instruction_slots},${TRUTH_MAXIMUM_INSTRUCTION_SLOTS},${TRUTH_BASELINE_INSTRUCTION_SLOTS},${TRUTH_BASELINE_REVISION},${reduction_basis_points},${width},${height},${pixels},${instruction_pixels}\n")
endforeach()

message(STATUS
  "Truth shader quality ${TRUTH_QUALITY}: ${instruction_slots} static instruction slots; "
  "budget ${TRUTH_MAXIMUM_INSTRUCTION_SLOTS}; report ${TRUTH_REPORT}")
if(instruction_slots GREATER TRUTH_MAXIMUM_INSTRUCTION_SLOTS)
  message(FATAL_ERROR
    "Truth shader instruction budget exceeded: ${instruction_slots} > "
    "${TRUTH_MAXIMUM_INSTRUCTION_SLOTS}")
endif()
