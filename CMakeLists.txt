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

  set(TRUTH_FXC_EXECUTABLE
    "C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/fxc.exe"
  )
  add_test(
    NAME truth_shader_fxc
    COMMAND "${CMAKE_COMMAND}"
      "-DTRUTH_FXC=${TRUTH_FXC_EXECUTABLE}"
      "-DTRUTH_SHADER=${CMAKE_CURRENT_SOURCE_DIR}/shaders/enbeffect.fx"
      "-DTRUTH_INCLUDE=${CMAKE_CURRENT_SOURCE_DIR}/shaders/truth"
      "-DTRUTH_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/shaders/$<CONFIG>/TruthMasterLook.fxo"
      "-DTRUTH_LISTING=${CMAKE_CURRENT_BINARY_DIR}/shaders/$<CONFIG>/TruthMasterLook.asm"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompileTruthShader.cmake"
  )
  set_tests_properties(truth_shader_fxc PROPERTIES LABELS "shader;fxc")
endif()
