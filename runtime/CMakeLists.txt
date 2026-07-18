cmake_minimum_required(VERSION 3.30)

project(truth-enb-runtime VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

set(
    TRUTH_ENB_RUNTIME_CORE_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../../enb-runtime-core"
    CACHE PATH
    "Absolute build-time path to the enb-runtime-core sibling"
)

set(
    TRUTH_ENB_RUNTIME_CORE_REVISION
    "e84e42b2053551e677ea00ad1400501117ccfa7c"
)

file(STRINGS
    "${CMAKE_CURRENT_SOURCE_DIR}/enb-runtime-core.lock"
    truth_runtime_core_lock_revision_line
    REGEX "^revision=[0-9a-f]+$"
)
list(LENGTH truth_runtime_core_lock_revision_line truth_runtime_core_lock_count)
if(NOT truth_runtime_core_lock_count EQUAL 1)
    message(FATAL_ERROR "enb-runtime-core.lock must contain one full revision")
endif()
list(GET truth_runtime_core_lock_revision_line 0 truth_runtime_core_lock_revision)
string(LENGTH "${truth_runtime_core_lock_revision}" truth_runtime_core_lock_length)
if(NOT truth_runtime_core_lock_length EQUAL 49)
    message(FATAL_ERROR "enb-runtime-core.lock revision must contain 40 hex digits")
endif()
string(REGEX REPLACE "^revision=" "" truth_runtime_core_lock_revision
    "${truth_runtime_core_lock_revision}")
if(NOT truth_runtime_core_lock_revision STREQUAL TRUTH_ENB_RUNTIME_CORE_REVISION)
    message(FATAL_ERROR
        "enb-runtime-core.lock does not match the admitted runtime revision"
    )
endif()

cmake_path(
    ABSOLUTE_PATH TRUTH_ENB_RUNTIME_CORE_ROOT
    NORMALIZE
    OUTPUT_VARIABLE truth_runtime_core_root_normalized
)
set(TRUTH_ENB_RUNTIME_CORE_ROOT "${truth_runtime_core_root_normalized}")

cmake_path(IS_ABSOLUTE TRUTH_ENB_RUNTIME_CORE_ROOT runtime_core_is_absolute)
if(NOT runtime_core_is_absolute)
    message(FATAL_ERROR "TRUTH_ENB_RUNTIME_CORE_ROOT must be absolute")
endif()
if(NOT EXISTS "${TRUTH_ENB_RUNTIME_CORE_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR "TRUTH_ENB_RUNTIME_CORE_ROOT does not contain enb-runtime-core")
endif()

find_package(Git REQUIRED)
execute_process(
    COMMAND "${GIT_EXECUTABLE}" -C "${TRUTH_ENB_RUNTIME_CORE_ROOT}" rev-parse HEAD
    RESULT_VARIABLE truth_runtime_core_revision_result
    OUTPUT_VARIABLE truth_runtime_core_revision
    ERROR_VARIABLE truth_runtime_core_revision_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT truth_runtime_core_revision_result EQUAL 0)
    message(FATAL_ERROR
        "Could not verify the pinned enb-runtime-core revision: "
        "${truth_runtime_core_revision_error}"
    )
endif()
if(NOT truth_runtime_core_revision STREQUAL TRUTH_ENB_RUNTIME_CORE_REVISION)
    message(FATAL_ERROR
        "enb-runtime-core revision mismatch: expected "
        "${TRUTH_ENB_RUNTIME_CORE_REVISION}, found ${truth_runtime_core_revision}"
    )
endif()
execute_process(
    COMMAND "${GIT_EXECUTABLE}" -C "${TRUTH_ENB_RUNTIME_CORE_ROOT}"
        status --porcelain --untracked-files=no
    RESULT_VARIABLE truth_runtime_core_status_result
    OUTPUT_VARIABLE truth_runtime_core_status
    ERROR_VARIABLE truth_runtime_core_status_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT truth_runtime_core_status_result EQUAL 0)
    message(FATAL_ERROR
        "Could not verify the pinned enb-runtime-core worktree: "
        "${truth_runtime_core_status_error}"
    )
endif()
if(NOT truth_runtime_core_status STREQUAL "")
    message(FATAL_ERROR
        "Pinned enb-runtime-core contains tracked local changes; use a clean "
        "checkout of ${TRUTH_ENB_RUNTIME_CORE_REVISION}"
    )
endif()

set(truth_parent_build_testing "${BUILD_TESTING}")
set(BUILD_TESTING OFF)
add_subdirectory(
    "${TRUTH_ENB_RUNTIME_CORE_ROOT}"
    "${CMAKE_CURRENT_BINARY_DIR}/enb-runtime-core"
    EXCLUDE_FROM_ALL
)
set(BUILD_TESTING "${truth_parent_build_testing}")

if(MSVC)
    # enb_runtime_core object code is linked into the shipped plugin, so its
    # compiler outputs are part of the reproducibility boundary as well.
    target_compile_options(enb_runtime_core PRIVATE /Brepro)
endif()

add_library(truth_enb_runtime_portable STATIC
    src/CameraFrame.cpp
    src/DiagnosticsPublisher.cpp
    src/EnbParameterBridge.cpp
    src/NativeCameraProvider.cpp
    src/PluginRuntime.cpp
    src/RuntimeContract.cpp
    src/RuntimeController.cpp
)
target_compile_features(truth_enb_runtime_portable PUBLIC cxx_std_23)
target_include_directories(
    truth_enb_runtime_portable
    PUBLIC
        include
        "${TRUTH_ENB_RUNTIME_CORE_ROOT}/include"
)
target_link_libraries(truth_enb_runtime_portable PRIVATE enb_runtime_core)

if(MSVC)
    target_compile_options(
        truth_enb_runtime_portable
        PRIVATE /W4 /WX /permissive- /fp:strict /Brepro
    )
endif()

if(WIN32)
    add_library(truth_enb_runtime_plugin SHARED
        src/windows/PluginMain.cpp
        src/windows/WindowsRuntimePlatform.cpp
    )
    target_compile_features(truth_enb_runtime_plugin PRIVATE cxx_std_23)
    target_include_directories(
        truth_enb_runtime_plugin
        PRIVATE
            include
            "${TRUTH_ENB_RUNTIME_CORE_ROOT}/include"
    )
    target_link_libraries(
        truth_enb_runtime_plugin
        PRIVATE
            truth_enb_runtime_portable
            enb_runtime_core
            Psapi
            Version
    )
    set_target_properties(
        truth_enb_runtime_plugin
        PROPERTIES
            OUTPUT_NAME "TruthENBRuntime"
            PREFIX ""
            SUFFIX ".dllplugin"
            WINDOWS_EXPORT_ALL_SYMBOLS OFF
    )
    if(MSVC)
        target_compile_options(
            truth_enb_runtime_plugin
            PRIVATE /W4 /WX /permissive- /fp:strict /guard:cf /Brepro
        )
        target_link_options(
            truth_enb_runtime_plugin
            PRIVATE /DYNAMICBASE /NXCOMPAT /GUARD:CF /CETCOMPAT /Brepro
        )
    endif()
endif()

if(BUILD_TESTING)
    add_executable(truth_runtime_contract_tests
        tests/RuntimeContractTests.cpp
    )
    target_compile_features(truth_runtime_contract_tests PRIVATE cxx_std_23)
    target_include_directories(
        truth_runtime_contract_tests
        PRIVATE
            include
            "${TRUTH_ENB_RUNTIME_CORE_ROOT}/include"
    )
    target_link_libraries(
        truth_runtime_contract_tests
        PRIVATE truth_enb_runtime_portable
    )

    add_executable(truth_runtime_controller_tests
        tests/RuntimeControllerTests.cpp
    )
    target_compile_features(truth_runtime_controller_tests PRIVATE cxx_std_23)
    target_include_directories(
        truth_runtime_controller_tests
        PRIVATE
            include
            "${TRUTH_ENB_RUNTIME_CORE_ROOT}/include"
    )
    target_link_libraries(
        truth_runtime_controller_tests
        PRIVATE truth_enb_runtime_portable
    )

    add_executable(truth_runtime_enb_parameter_tests
        tests/EnbParameterBridgeTests.cpp
    )
    target_compile_features(truth_runtime_enb_parameter_tests PRIVATE cxx_std_23)
    target_include_directories(
        truth_runtime_enb_parameter_tests
        PRIVATE
            include
            "${TRUTH_ENB_RUNTIME_CORE_ROOT}/include"
    )
    target_link_libraries(
        truth_runtime_enb_parameter_tests
        PRIVATE truth_enb_runtime_portable
    )

    add_executable(truth_runtime_native_camera_tests
        tests/NativeCameraProviderTests.cpp
    )
    target_compile_features(truth_runtime_native_camera_tests PRIVATE cxx_std_23)
    target_include_directories(
        truth_runtime_native_camera_tests
        PRIVATE
            include
            "${TRUTH_ENB_RUNTIME_CORE_ROOT}/include"
    )
    target_link_libraries(
        truth_runtime_native_camera_tests
        PRIVATE truth_enb_runtime_portable
    )

    add_executable(truth_runtime_plugin_tests
        tests/PluginRuntimeTests.cpp
    )
    target_compile_features(truth_runtime_plugin_tests PRIVATE cxx_std_23)
    target_include_directories(
        truth_runtime_plugin_tests
        PRIVATE
            include
            "${TRUTH_ENB_RUNTIME_CORE_ROOT}/include"
    )
    target_link_libraries(
        truth_runtime_plugin_tests
        PRIVATE truth_enb_runtime_portable
    )

    add_executable(truth_runtime_diagnostics_publisher_tests
        tests/DiagnosticsPublisherTests.cpp
    )
    target_compile_features(
        truth_runtime_diagnostics_publisher_tests
        PRIVATE cxx_std_23
    )
    target_include_directories(
        truth_runtime_diagnostics_publisher_tests
        PRIVATE
            include
            "${TRUTH_ENB_RUNTIME_CORE_ROOT}/include"
    )
    target_link_libraries(
        truth_runtime_diagnostics_publisher_tests
        PRIVATE truth_enb_runtime_portable
    )

    if(MSVC)
        target_compile_options(
            truth_runtime_contract_tests
            PRIVATE /W4 /WX /permissive- /fp:strict
        )
        target_compile_options(
            truth_runtime_controller_tests
            PRIVATE /W4 /WX /permissive- /fp:strict
        )
        target_compile_options(
            truth_runtime_enb_parameter_tests
            PRIVATE /W4 /WX /permissive- /fp:strict
        )
        target_compile_options(
            truth_runtime_native_camera_tests
            PRIVATE /W4 /WX /permissive- /fp:strict
        )
        target_compile_options(
            truth_runtime_plugin_tests
            PRIVATE /W4 /WX /permissive- /fp:strict
        )
        target_compile_options(
            truth_runtime_diagnostics_publisher_tests
            PRIVATE /W4 /WX /permissive- /fp:strict
        )
    endif()

    add_test(NAME truth_runtime_contract COMMAND truth_runtime_contract_tests)
    add_test(NAME truth_runtime_controller COMMAND truth_runtime_controller_tests)
    add_test(NAME truth_runtime_enb_parameter COMMAND truth_runtime_enb_parameter_tests)
    add_test(NAME truth_runtime_native_camera COMMAND truth_runtime_native_camera_tests)
    add_test(NAME truth_runtime_plugin COMMAND truth_runtime_plugin_tests)
    add_test(
        NAME truth_runtime_diagnostics_publisher
        COMMAND truth_runtime_diagnostics_publisher_tests
    )

    if(MSVC AND WIN32)
        add_test(
            NAME truth_runtime_reproducible_release
            COMMAND
                "${CMAKE_COMMAND}"
                "-DTRUTH_RUNTIME_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
                "-DTRUTH_RUNTIME_CORE_ROOT=${TRUTH_ENB_RUNTIME_CORE_ROOT}"
                "-DTRUTH_RUNTIME_REPRO_ROOT=${CMAKE_CURRENT_BINARY_DIR}/reproducibility/$<CONFIG>"
                "-DTRUTH_RUNTIME_GENERATOR=${CMAKE_GENERATOR}"
                "-DTRUTH_RUNTIME_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
                "-DTRUTH_RUNTIME_GENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
                "-DTRUTH_RUNTIME_REFERENCE_ARTIFACT=$<$<CONFIG:Release>:$<TARGET_FILE:truth_enb_runtime_plugin>>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyReproducibleRelease.cmake"
        )
        set_tests_properties(
            truth_runtime_reproducible_release
            PROPERTIES
                LABELS "release;reproducibility"
                TIMEOUT 300
        )
    endif()

    if(WIN32)
        add_executable(truth_runtime_windows_platform_tests
            tests/WindowsRuntimePlatformTests.cpp
            src/windows/WindowsRuntimePlatform.cpp
        )
        target_compile_features(
            truth_runtime_windows_platform_tests
            PRIVATE cxx_std_23
        )
        target_include_directories(
            truth_runtime_windows_platform_tests
            PRIVATE
                include
                "${TRUTH_ENB_RUNTIME_CORE_ROOT}/include"
        )
        target_link_libraries(
            truth_runtime_windows_platform_tests
            PRIVATE truth_enb_runtime_portable Psapi Version
        )
        if(MSVC)
            target_compile_options(
                truth_runtime_windows_platform_tests
                PRIVATE /W4 /WX /permissive- /fp:strict
            )
        endif()
        add_test(
            NAME truth_runtime_windows_platform
            COMMAND truth_runtime_windows_platform_tests
        )

        add_executable(truth_runtime_plugin_binary_tests
            tests/PluginBinaryTests.cpp
        )
        target_compile_features(
            truth_runtime_plugin_binary_tests
            PRIVATE cxx_std_23
        )
        target_include_directories(
            truth_runtime_plugin_binary_tests
            PRIVATE
                include
                "${TRUTH_ENB_RUNTIME_CORE_ROOT}/include"
        )
        if(MSVC)
            target_compile_options(
                truth_runtime_plugin_binary_tests
                PRIVATE /W4 /WX /permissive-
            )
        endif()
        add_dependencies(
            truth_runtime_plugin_binary_tests
            truth_enb_runtime_plugin
        )
        add_test(
            NAME truth_runtime_plugin_binary
            COMMAND
                truth_runtime_plugin_binary_tests
                "$<TARGET_FILE:truth_enb_runtime_plugin>"
        )
    endif()
endif()

# Keep every generated runtime project hermetic. A workstation-wide vcpkg
# integration must not inject post-build applocal hooks into these binaries.
get_property(truth_runtime_local_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
list(APPEND truth_runtime_local_targets enb_runtime_core)
foreach(truth_runtime_target IN LISTS truth_runtime_local_targets)
    if(TARGET "${truth_runtime_target}")
        set_property(
            TARGET "${truth_runtime_target}"
            PROPERTY VS_GLOBAL_VcpkgApplocalDeps false
        )
    endif()
endforeach()
