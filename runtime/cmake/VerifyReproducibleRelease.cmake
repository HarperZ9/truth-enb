cmake_minimum_required(VERSION 3.30)

foreach(required_variable IN ITEMS
    TRUTH_RUNTIME_SOURCE_DIR
    TRUTH_RUNTIME_CORE_ROOT
    TRUTH_RUNTIME_REPRO_ROOT
    TRUTH_RUNTIME_GENERATOR
)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

function(run_checked description)
    execute_process(
        COMMAND ${ARGN}
        RESULT_VARIABLE command_result
        OUTPUT_VARIABLE command_output
        ERROR_VARIABLE command_error
    )
    if(NOT command_result EQUAL 0)
        message(FATAL_ERROR
            "${description} failed (${command_result})\n"
            "stdout:\n${command_output}\n"
            "stderr:\n${command_error}"
        )
    endif()
endfunction()

file(REMOVE_RECURSE "${TRUTH_RUNTIME_REPRO_ROOT}")
file(MAKE_DIRECTORY "${TRUTH_RUNTIME_REPRO_ROOT}")

set(release_artifacts)
foreach(build_name IN ITEMS clean-a clean-b)
    set(build_root "${TRUTH_RUNTIME_REPRO_ROOT}/${build_name}")
    set(configure_command
        "${CMAKE_COMMAND}"
        -S "${TRUTH_RUNTIME_SOURCE_DIR}"
        -B "${build_root}"
        -G "${TRUTH_RUNTIME_GENERATOR}"
        "-DBUILD_TESTING=OFF"
        "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded"
        "-DTRUTH_ENB_RUNTIME_CORE_ROOT=${TRUTH_RUNTIME_CORE_ROOT}"
    )
    if(DEFINED TRUTH_RUNTIME_GENERATOR_PLATFORM
        AND NOT "${TRUTH_RUNTIME_GENERATOR_PLATFORM}" STREQUAL "")
        list(APPEND configure_command
            -A "${TRUTH_RUNTIME_GENERATOR_PLATFORM}"
        )
    endif()
    if(DEFINED TRUTH_RUNTIME_GENERATOR_TOOLSET
        AND NOT "${TRUTH_RUNTIME_GENERATOR_TOOLSET}" STREQUAL "")
        list(APPEND configure_command
            -T "${TRUTH_RUNTIME_GENERATOR_TOOLSET}"
        )
    endif()

    run_checked("${build_name} configure" ${configure_command})
    run_checked(
        "${build_name} Release build"
        "${CMAKE_COMMAND}"
        --build "${build_root}"
        --config Release
        --target truth_enb_runtime_plugin
    )
    list(APPEND release_artifacts
        "${build_root}/Release/TruthENBRuntime.dllplugin"
    )
endforeach()

list(GET release_artifacts 0 artifact_a)
list(GET release_artifacts 1 artifact_b)
foreach(artifact IN LISTS release_artifacts)
    if(NOT EXISTS "${artifact}")
        message(FATAL_ERROR "Release artifact was not produced: ${artifact}")
    endif()
endforeach()

file(SHA256 "${artifact_a}" artifact_a_sha256)
file(SHA256 "${artifact_b}" artifact_b_sha256)
file(SIZE "${artifact_a}" artifact_a_size)
file(SIZE "${artifact_b}" artifact_b_size)
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${artifact_a}" "${artifact_b}"
    RESULT_VARIABLE comparison_result
)
if(NOT comparison_result EQUAL 0)
    message(FATAL_ERROR
        "Independent clean Release builds were not byte-identical:\n"
        "A: ${artifact_a_size} bytes ${artifact_a_sha256}\n"
        "B: ${artifact_b_size} bytes ${artifact_b_sha256}"
    )
endif()

set(verification_scope "clean")
if(DEFINED TRUTH_RUNTIME_REFERENCE_ARTIFACT
    AND NOT "${TRUTH_RUNTIME_REFERENCE_ARTIFACT}" STREQUAL "")
    if(NOT EXISTS "${TRUTH_RUNTIME_REFERENCE_ARTIFACT}")
        message(FATAL_ERROR
            "Shipped Release artifact was not produced: "
            "${TRUTH_RUNTIME_REFERENCE_ARTIFACT}"
        )
    endif()
    file(SHA256 "${TRUTH_RUNTIME_REFERENCE_ARTIFACT}" reference_sha256)
    file(SIZE "${TRUTH_RUNTIME_REFERENCE_ARTIFACT}" reference_size)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E compare_files
            "${artifact_a}" "${TRUTH_RUNTIME_REFERENCE_ARTIFACT}"
        RESULT_VARIABLE reference_comparison_result
    )
    if(NOT reference_comparison_result EQUAL 0)
        message(FATAL_ERROR
            "The shipped Release plugin differs from the clean-build witness:\n"
            "Clean: ${artifact_a_size} bytes ${artifact_a_sha256}\n"
            "Shipped: ${reference_size} bytes ${reference_sha256}"
        )
    endif()
    set(verification_scope "clean/shipped")
endif()

message(STATUS
    "Truth runtime ${verification_scope} Release reproducibility verified: "
    "${artifact_a_size} bytes ${artifact_a_sha256}"
)
