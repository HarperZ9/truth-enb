cmake_minimum_required(VERSION 3.28)

cmake_policy(SET CMP0091 NEW)
project(TruthENB VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_library(truth_master_look STATIC
  src/render/MasterLook.cpp
)
target_compile_features(truth_master_look PUBLIC cxx_std_23)
target_include_directories(truth_master_look PUBLIC include)

if(MSVC)
  target_compile_options(truth_master_look PRIVATE /W4 /permissive- /EHsc /utf-8)
endif()

add_library(truth_atmosphere STATIC
  src/render/Atmosphere.cpp
)
target_compile_features(truth_atmosphere PUBLIC cxx_std_23)
target_include_directories(truth_atmosphere PUBLIC include)

if(MSVC)
  target_compile_options(truth_atmosphere PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
endif()

add_library(truth_sky_fields STATIC
  src/render/SkyFields.cpp
)
target_compile_features(truth_sky_fields PUBLIC cxx_std_23)
target_include_directories(truth_sky_fields PUBLIC include)

if(MSVC)
  target_compile_options(truth_sky_fields PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
endif()

add_library(truth_cloud_lighting STATIC
  src/render/CloudLighting.cpp
)
target_compile_features(truth_cloud_lighting PUBLIC cxx_std_23)
target_include_directories(truth_cloud_lighting PUBLIC include)

if(MSVC)
  target_compile_options(truth_cloud_lighting PRIVATE
    /W4 /WX /permissive- /EHsc /utf-8
  )
endif()

if(BUILD_TESTING)
  add_executable(truth_master_look_tests
    tests/MasterLookTests.cpp
  )
  target_compile_features(truth_master_look_tests PRIVATE cxx_std_23)
  target_link_libraries(truth_master_look_tests PRIVATE truth_master_look)

  # The slice is dependency-free; suppress user-wide vcpkg app-local hooks.
  set_property(TARGET truth_master_look_tests PROPERTY VS_GLOBAL_VcpkgApplocalDeps false)

  if(MSVC)
    target_compile_options(truth_master_look_tests PRIVATE /W4 /permissive- /EHsc /utf-8)
  endif()

  add_test(NAME truth_master_look_cpp COMMAND truth_master_look_tests)

  add_executable(truth_atmosphere_tests
    tests/AtmosphereTests.cpp
  )
  target_compile_features(truth_atmosphere_tests PRIVATE cxx_std_23)
  target_link_libraries(truth_atmosphere_tests PRIVATE truth_atmosphere)
  set_property(TARGET truth_atmosphere_tests PROPERTY VS_GLOBAL_VcpkgApplocalDeps false)

  if(MSVC)
    target_compile_options(truth_atmosphere_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
  endif()

  add_test(NAME truth_atmosphere_cpp COMMAND truth_atmosphere_tests)

  add_executable(truth_sky_fields_tests
    tests/SkyFieldsTests.cpp
  )
  target_compile_features(truth_sky_fields_tests PRIVATE cxx_std_23)
  target_link_libraries(truth_sky_fields_tests PRIVATE truth_sky_fields)
  set_property(TARGET truth_sky_fields_tests PROPERTY VS_GLOBAL_VcpkgApplocalDeps false)

  if(MSVC)
    target_compile_options(truth_sky_fields_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
  endif()

  add_test(NAME truth_sky_fields_cpp COMMAND truth_sky_fields_tests)

  add_executable(truth_cloud_lighting_tests
    tests/CloudLightingTests.cpp
  )
  target_compile_features(truth_cloud_lighting_tests PRIVATE cxx_std_23)
  target_link_libraries(truth_cloud_lighting_tests PRIVATE
    truth_cloud_lighting
    truth_sky_fields
  )
  set_property(TARGET truth_cloud_lighting_tests PROPERTY VS_GLOBAL_VcpkgApplocalDeps false)

  if(MSVC)
    target_compile_options(truth_cloud_lighting_tests PRIVATE
      /W4 /WX /permissive- /EHsc /utf-8
    )
  endif()

  add_test(NAME truth_cloud_lighting_cpp COMMAND truth_cloud_lighting_tests)

  set(TRUTH_FXC_EXECUTABLE
    "C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/fxc.exe"
  )
  function(add_truth_shader_permutation
      test_name atmosphere_macro_value procedural_sky_macro_value artifact_name)
    add_test(
      NAME "${test_name}"
      COMMAND "${CMAKE_COMMAND}"
        "-DTRUTH_FXC=${TRUTH_FXC_EXECUTABLE}"
        "-DTRUTH_SHADER=${CMAKE_CURRENT_SOURCE_DIR}/shaders/enbeffect.fx"
        "-DTRUTH_REQUIRED_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/shaders/truth/TruthAtmosphereCore.fxh"
        "-DTRUTH_REQUIRED_SOURCE_2=${CMAKE_CURRENT_SOURCE_DIR}/shaders/truth/TruthSkyFields.fxh"
        "-DTRUTH_REQUIRED_SOURCE_3=${CMAKE_CURRENT_SOURCE_DIR}/shaders/truth/TruthCloudLighting.fxh"
        "-DTRUTH_INCLUDE=${CMAKE_CURRENT_SOURCE_DIR}/shaders/truth"
        "-DTRUTH_DEFINE=TRUTH_ENABLE_ATMOSPHERE=${atmosphere_macro_value}"
        "-DTRUTH_SECOND_DEFINE=TRUTH_ENABLE_PROCEDURAL_SKY=${procedural_sky_macro_value}"
        "-DTRUTH_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/shaders/$<CONFIG>/${artifact_name}.fxo"
        "-DTRUTH_LISTING=${CMAKE_CURRENT_BINARY_DIR}/shaders/$<CONFIG>/${artifact_name}.asm"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompileTruthShader.cmake"
    )
    set_tests_properties("${test_name}" PROPERTIES LABELS "shader;fxc")
  endfunction()

  add_truth_shader_permutation(
    truth_shader_fxc_a0_p0 0 0 TruthA0P0
  )
  add_truth_shader_permutation(
    truth_shader_fxc_a0_p1 0 1 TruthA0P1
  )
  add_truth_shader_permutation(
    truth_shader_fxc_a1_p0 1 0 TruthA1P0
  )
  add_truth_shader_permutation(
    truth_shader_fxc_a1_p1 1 1 TruthA1P1
  )

  add_test(
    NAME truth_shader_atmosphere_permutations_distinct
    COMMAND "${CMAKE_COMMAND}" -E compare_files
      "${CMAKE_CURRENT_BINARY_DIR}/shaders/$<CONFIG>/TruthA1P1.fxo"
      "${CMAKE_CURRENT_BINARY_DIR}/shaders/$<CONFIG>/TruthA0P0.fxo"
  )
  set_tests_properties(truth_shader_atmosphere_permutations_distinct PROPERTIES
    DEPENDS "truth_shader_fxc_a1_p1;truth_shader_fxc_a0_p0"
    LABELS "shader;fxc"
    WILL_FAIL TRUE
  )

  add_test(
    NAME truth_shader_direct_control_permutations_distinct
    COMMAND "${CMAKE_COMMAND}" -E compare_files
      "${CMAKE_CURRENT_BINARY_DIR}/shaders/$<CONFIG>/TruthA1P1.fxo"
      "${CMAKE_CURRENT_BINARY_DIR}/shaders/$<CONFIG>/TruthA1P0.fxo"
  )
  set_tests_properties(truth_shader_direct_control_permutations_distinct PROPERTIES
    DEPENDS "truth_shader_fxc_a1_p1;truth_shader_fxc_a1_p0"
    LABELS "shader;fxc"
    WILL_FAIL TRUE
  )
endif()
