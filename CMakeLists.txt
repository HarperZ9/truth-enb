cmake_minimum_required(VERSION 3.30)

cmake_policy(SET CMP0091 NEW)
project(TruthENB VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(
  TRUTH_BUILD_RUNTIME
  "Build the pinned standalone Truth ENB camera bridge"
  ON
)

if(WIN32 AND TRUTH_BUILD_RUNTIME)
  add_subdirectory(runtime)
endif()

add_library(truth_master_look STATIC
  src/render/MasterLook.cpp
)
target_compile_features(truth_master_look PUBLIC cxx_std_23)
target_include_directories(truth_master_look PUBLIC include)

if(MSVC)
  target_compile_options(truth_master_look PRIVATE /W4 /permissive- /EHsc /utf-8)
endif()

add_library(truth_interior_light STATIC
  src/render/InteriorLight.cpp
)
target_compile_features(truth_interior_light PUBLIC cxx_std_23)
target_include_directories(truth_interior_light PUBLIC include)

if(MSVC)
  target_compile_options(truth_interior_light PRIVATE /W4 /permissive- /EHsc /utf-8)
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

add_library(truth_cloud_volume STATIC
  src/render/CloudVolume.cpp
)
target_compile_features(truth_cloud_volume PUBLIC cxx_std_23)
target_include_directories(truth_cloud_volume PUBLIC include)
target_link_libraries(truth_cloud_volume PUBLIC truth_sky_fields)

if(MSVC)
  target_compile_options(truth_cloud_volume PRIVATE
    /W4 /WX /permissive- /EHsc /utf-8
  )
endif()

add_library(truth_aurora_curtain STATIC
  src/render/AuroraCurtain.cpp
)
target_compile_features(truth_aurora_curtain PUBLIC cxx_std_23)
target_include_directories(truth_aurora_curtain PUBLIC include)

if(MSVC)
  target_compile_options(truth_aurora_curtain PRIVATE
    /W4 /WX /permissive- /EHsc /utf-8
  )
endif()

target_link_libraries(truth_sky_fields PUBLIC truth_aurora_curtain)

add_library(truth_sky_view_adapter STATIC
  src/render/SkyViewAdapter.cpp
)
target_compile_features(truth_sky_view_adapter PUBLIC cxx_std_23)
target_include_directories(truth_sky_view_adapter PUBLIC include)

if(MSVC)
  target_compile_options(truth_sky_view_adapter PRIVATE
    /W4 /WX /permissive- /EHsc /utf-8
  )
endif()

if(WIN32)
  add_library(truth_reference_renderer_core STATIC
    src/render/ReferenceRenderer.cpp
  )
  target_compile_features(truth_reference_renderer_core PUBLIC cxx_std_23)
  target_include_directories(truth_reference_renderer_core PUBLIC include)
  target_link_libraries(truth_reference_renderer_core PUBLIC
    bcrypt
    d3d11
    d3dcompiler
  )
  set_property(TARGET truth_reference_renderer_core PROPERTY VS_GLOBAL_VcpkgApplocalDeps false)

  add_executable(truth_reference_renderer
    tools/TruthReferenceRenderer.cpp
  )
  target_compile_features(truth_reference_renderer PRIVATE cxx_std_23)
  target_link_libraries(truth_reference_renderer PRIVATE truth_reference_renderer_core)
  set_property(TARGET truth_reference_renderer PROPERTY VS_GLOBAL_VcpkgApplocalDeps false)

  if(MSVC)
    target_compile_options(truth_reference_renderer_core PRIVATE
      /W4 /WX /permissive- /EHsc /utf-8
    )
    target_compile_options(truth_reference_renderer PRIVATE
      /W4 /WX /permissive- /EHsc /utf-8
    )
  endif()
endif()

if(WIN32 AND TRUTH_BUILD_RUNTIME)
  set(TRUTH_RELEASE_COMPONENT TruthPrivateRelease)
  # MO2 Root-Builder layout: everything under Root/ maps to the game root, so
  # ENBSeries finds enbseries\ and the .dllplugin where it reads them. The
  # documentation root stays top-level (MO2 shows it, never deploys it).
  set(TRUTH_RELEASE_SHADER_ROOT "Root/enbseries")
  set(TRUTH_RELEASE_DOCUMENTATION_ROOT "Truth ENB Documentation")
  set(TRUTH_RELEASE_SHADER_INCLUDES
    shaders/truth/TruthAtmosphereCore.fxh
    shaders/truth/TruthAuroraCurtain.fxh
    shaders/truth/TruthCloudLighting.fxh
    shaders/truth/TruthColorCore.fxh
    shaders/truth/TruthEffectParameters.fxh
    shaders/truth/TruthRuntimeParameters.fxh
    shaders/truth/TruthSkyFields.fxh
    shaders/truth/TruthSkyViewAdapter.fxh
  )
  set(TRUTH_RELEASE_ENB_CONTRACT
    shaders/enb/ENBSeries0504VanillaPostProcess.fxh
  )

  install(
    FILES shaders/enbeffect.fx
    DESTINATION "${TRUTH_RELEASE_SHADER_ROOT}"
    COMPONENT "${TRUTH_RELEASE_COMPONENT}"
  )
  install(
    FILES ${TRUTH_RELEASE_SHADER_INCLUDES}
    DESTINATION "${TRUTH_RELEASE_SHADER_ROOT}/truth"
    COMPONENT "${TRUTH_RELEASE_COMPONENT}"
  )
  install(
    FILES ${TRUTH_RELEASE_ENB_CONTRACT}
    DESTINATION "${TRUTH_RELEASE_SHADER_ROOT}/enb"
    COMPONENT "${TRUTH_RELEASE_COMPONENT}"
  )
  install(
    TARGETS truth_enb_runtime_plugin
    RUNTIME DESTINATION "${TRUTH_RELEASE_SHADER_ROOT}"
    COMPONENT "${TRUTH_RELEASE_COMPONENT}"
  )
  install(
    FILES
      README.md
      LICENSE
      runtime/enb-runtime-core.lock
      runtime/enb-upstream.lock
    DESTINATION "${TRUTH_RELEASE_DOCUMENTATION_ROOT}"
    COMPONENT "${TRUTH_RELEASE_COMPONENT}"
  )
  install(
    FILES
      docs/architecture.md
      docs/release-validation.md
    DESTINATION "${TRUTH_RELEASE_DOCUMENTATION_ROOT}/docs"
    COMPONENT "${TRUTH_RELEASE_COMPONENT}"
  )
  install(
    FILES runtime/README.md
    DESTINATION "${TRUTH_RELEASE_DOCUMENTATION_ROOT}"
    RENAME Runtime.md
    COMPONENT "${TRUTH_RELEASE_COMPONENT}"
  )

  set(CPACK_GENERATOR "ZIP")
  set(CPACK_PACKAGE_NAME "Truth ENB Private RC")
  set(CPACK_PACKAGE_VENDOR "Truth ENB")
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Truth ENB original shader system and native camera bridge private RC")
  set(CPACK_PACKAGE_VERSION "0.1.0")
  set(CPACK_PACKAGE_FILE_NAME "Truth-ENB-Private-RC-win64")
  set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
  set(CPACK_COMPONENTS_ALL "${TRUTH_RELEASE_COMPONENT}")
  set(CPACK_THREADS 1)
  set(CPACK_PACKAGE_CHECKSUM SHA256)
  include(CPack)

endif()

if(BUILD_TESTING)
  add_test(
    NAME truth_quality_presets
    COMMAND "${CMAKE_COMMAND}"
      "-DTRUTH_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
      "-DTRUTH_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckTruthQualityPresets.cmake"
  )
  set_tests_properties(truth_quality_presets PROPERTIES
    LABELS "shader;quality;presets;determinism"
  )

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

  add_executable(truth_interior_light_tests
    tests/InteriorLightTests.cpp
  )
  target_compile_features(truth_interior_light_tests PRIVATE cxx_std_23)
  target_link_libraries(truth_interior_light_tests PRIVATE truth_interior_light)

  set_property(TARGET truth_interior_light_tests PROPERTY VS_GLOBAL_VcpkgApplocalDeps false)

  if(MSVC)
    target_compile_options(truth_interior_light_tests PRIVATE /W4 /permissive- /EHsc /utf-8)
  endif()

  add_test(NAME truth_interior_light_cpp COMMAND truth_interior_light_tests)

  if(WIN32)
    add_executable(truth_interior_light_warp_tests
      tests/InteriorLightWarpTests.cpp
    )
    target_compile_features(truth_interior_light_warp_tests PRIVATE cxx_std_23)
    target_link_libraries(truth_interior_light_warp_tests PRIVATE
      truth_interior_light
      d3d11
      d3dcompiler
    )
    set_property(TARGET truth_interior_light_warp_tests
      PROPERTY VS_GLOBAL_VcpkgApplocalDeps false)
    if(MSVC)
      target_compile_options(truth_interior_light_warp_tests PRIVATE
        /W4 /WX /permissive- /EHsc /utf-8
      )
    endif()
    add_test(
      NAME truth_interior_light_warp
      COMMAND truth_interior_light_warp_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/truth/TruthInteriorLightWarpProbe.hlsl"
    )
    set_tests_properties(truth_interior_light_warp PROPERTIES
      LABELS "gpu;warp;shader;interior"
    )
  endif()

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

  add_executable(truth_cloud_volume_tests
    tests/CloudVolumeTests.cpp
  )
  target_compile_features(truth_cloud_volume_tests PRIVATE cxx_std_23)
  target_link_libraries(truth_cloud_volume_tests PRIVATE truth_cloud_volume)
  set_property(TARGET truth_cloud_volume_tests PROPERTY VS_GLOBAL_VcpkgApplocalDeps false)

  if(MSVC)
    target_compile_options(truth_cloud_volume_tests PRIVATE
      /W4 /WX /permissive- /EHsc /utf-8
    )
  endif()

  add_test(NAME truth_cloud_volume_cpp COMMAND truth_cloud_volume_tests)

  add_executable(truth_aurora_curtain_tests
    tests/AuroraCurtainTests.cpp
  )
  target_compile_features(truth_aurora_curtain_tests PRIVATE cxx_std_23)
  target_link_libraries(truth_aurora_curtain_tests PRIVATE truth_aurora_curtain)
  set_property(TARGET truth_aurora_curtain_tests PROPERTY VS_GLOBAL_VcpkgApplocalDeps false)

  if(MSVC)
    target_compile_options(truth_aurora_curtain_tests PRIVATE
      /W4 /WX /permissive- /EHsc /utf-8
    )
  endif()

  add_test(NAME truth_aurora_curtain_cpp COMMAND truth_aurora_curtain_tests)

  add_executable(truth_sky_view_adapter_tests
    tests/SkyViewAdapterTests.cpp
  )
  target_compile_features(truth_sky_view_adapter_tests PRIVATE cxx_std_23)
  target_link_libraries(truth_sky_view_adapter_tests PRIVATE truth_sky_view_adapter)
  set_property(TARGET truth_sky_view_adapter_tests PROPERTY VS_GLOBAL_VcpkgApplocalDeps false)

  if(MSVC)
    target_compile_options(truth_sky_view_adapter_tests PRIVATE
      /W4 /WX /permissive- /EHsc /utf-8
    )
  endif()

  add_test(NAME truth_sky_view_adapter_cpp COMMAND truth_sky_view_adapter_tests)

  if(WIN32)
    add_executable(truth_sky_view_adapter_warp_tests
      tests/SkyViewAdapterWarpTests.cpp
    )
    target_compile_features(truth_sky_view_adapter_warp_tests PRIVATE cxx_std_23)
    target_link_libraries(truth_sky_view_adapter_warp_tests PRIVATE
      truth_master_look
      truth_sky_view_adapter
      d3d11
      d3dcompiler
    )
    set_property(TARGET truth_sky_view_adapter_warp_tests
      PROPERTY VS_GLOBAL_VcpkgApplocalDeps false)

    if(MSVC)
      target_compile_options(truth_sky_view_adapter_warp_tests PRIVATE
        /W4 /WX /permissive- /EHsc /utf-8
      )
    endif()

    add_test(
      NAME truth_sky_view_adapter_warp
      COMMAND truth_sky_view_adapter_warp_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/truth/TruthSkyViewAdapterProbe.hlsl"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/truth/TruthRuntimeParametersProbe.hlsl"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/enbeffect.fx"
    )
    set_tests_properties(truth_sky_view_adapter_warp PROPERTIES
      LABELS "gpu;warp;shader;adapter;abi"
    )
  endif()

  add_test(
    NAME truth_sky_view_adapter_shader_contract
    COMMAND "${CMAKE_COMMAND}"
      "-DTRUTH_EFFECT=${CMAKE_CURRENT_SOURCE_DIR}/shaders/enbeffect.fx"
      "-DTRUTH_ADAPTER=${CMAKE_CURRENT_SOURCE_DIR}/shaders/truth/TruthSkyViewAdapter.fxh"
      "-DTRUTH_RUNTIME=${CMAKE_CURRENT_SOURCE_DIR}/shaders/truth/TruthRuntimeParameters.fxh"
      "-DTRUTH_ENB_VANILLA=${CMAKE_CURRENT_SOURCE_DIR}/shaders/enb/ENBSeries0504VanillaPostProcess.fxh"
      "-DTRUTH_COMPILE_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompileTruthShader.cmake"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckTruthSkyViewAdapterContract.cmake"
  )
  set_tests_properties(truth_sky_view_adapter_shader_contract PROPERTIES
    LABELS "shader;adapter;contract"
  )

  add_test(
    NAME truth_aurora_default_quality_contract
    COMMAND "${CMAKE_COMMAND}"
      "-DTRUTH_AURORA_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/shaders/truth/TruthAuroraCurtain.fxh"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckTruthAuroraDefaultContract.cmake"
  )
  set_tests_properties(truth_aurora_default_quality_contract PROPERTIES
    LABELS "shader;aurora;contract;performance"
  )

  if(WIN32)
    add_executable(truth_reference_renderer_tests
      tests/ReferenceRendererTests.cpp
    )
    target_compile_features(truth_reference_renderer_tests PRIVATE cxx_std_23)
    target_link_libraries(truth_reference_renderer_tests PRIVATE
      truth_reference_renderer_core
      truth_cloud_volume
      truth_sky_fields
    )
    set_property(TARGET truth_reference_renderer_tests PROPERTY VS_GLOBAL_VcpkgApplocalDeps false)

    if(MSVC)
      target_compile_options(truth_reference_renderer_tests PRIVATE
        /W4 /WX /permissive- /EHsc /utf-8
      )
    endif()

    add_test(
      NAME truth_reference_renderer_warp
      COMMAND truth_reference_renderer_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/truth/TruthReferenceSky.hlsl"
        "${CMAKE_CURRENT_BINARY_DIR}/references/$<CONFIG>"
    )
    set_tests_properties(truth_reference_renderer_warp PROPERTIES
      LABELS "gpu;warp;shader"
    )
  endif()

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
        "-DTRUTH_REQUIRED_SOURCE_4=${CMAKE_CURRENT_SOURCE_DIR}/shaders/truth/TruthAuroraCurtain.fxh"
        "-DTRUTH_INCLUDE=${CMAKE_CURRENT_SOURCE_DIR}/shaders/truth"
        "-DTRUTH_DEFINE=TRUTH_ENABLE_ATMOSPHERE=${atmosphere_macro_value}"
        "-DTRUTH_SECOND_DEFINE=TRUTH_ENABLE_PROCEDURAL_SKY=${procedural_sky_macro_value}"
        "-DTRUTH_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/shaders/$<CONFIG>/${artifact_name}.fxo"
        "-DTRUTH_LISTING=${CMAKE_CURRENT_BINARY_DIR}/shaders/$<CONFIG>/${artifact_name}.asm"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompileTruthShader.cmake"
    )
    set_tests_properties("${test_name}" PROPERTIES LABELS "shader;fxc")
  endfunction()

  add_test(
    NAME truth_interior_light_fxc
    COMMAND "${CMAKE_COMMAND}"
      "-DTRUTH_FXC=${TRUTH_FXC_EXECUTABLE}"
      "-DTRUTH_SHADER=${CMAKE_CURRENT_SOURCE_DIR}/shaders/truth/TruthInteriorLightProbe.fx"
      "-DTRUTH_REQUIRED_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/shaders/truth/TruthInteriorLight.fxh"
      "-DTRUTH_INCLUDE=${CMAKE_CURRENT_SOURCE_DIR}/shaders/truth"
      "-DTRUTH_DEFINE=TRUTH_INTERIOR_LIGHT_PROBE=1"
      "-DTRUTH_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/shaders/$<CONFIG>/TruthInteriorLightProbe.fxo"
      "-DTRUTH_LISTING=${CMAKE_CURRENT_BINARY_DIR}/shaders/$<CONFIG>/TruthInteriorLightProbe.asm"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompileTruthShader.cmake"
  )
  set_tests_properties(truth_interior_light_fxc PROPERTIES LABELS "shader;fxc;interior")

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

  function(add_truth_cloud_volume_permutation
      test_name volume_macro_value quality_macro_value artifact_name)
    add_test(
      NAME "${test_name}"
      COMMAND "${CMAKE_COMMAND}"
        "-DTRUTH_FXC=${TRUTH_FXC_EXECUTABLE}"
        "-DTRUTH_SHADER=${CMAKE_CURRENT_SOURCE_DIR}/shaders/truth/TruthReferenceSky.hlsl"
        "-DTRUTH_REQUIRED_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/shaders/truth/TruthCloudVolume.fxh"
        "-DTRUTH_INCLUDE=${CMAKE_CURRENT_SOURCE_DIR}/shaders/truth"
        "-DTRUTH_DEFINE=TRUTH_ENABLE_CLOUD_VOLUME=${volume_macro_value}"
        "-DTRUTH_SECOND_DEFINE=TRUTH_CLOUD_VOLUME_QUALITY=${quality_macro_value}"
        "-DTRUTH_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/shaders/$<CONFIG>/${artifact_name}.cso"
        "-DTRUTH_LISTING=${CMAKE_CURRENT_BINARY_DIR}/shaders/$<CONFIG>/${artifact_name}.asm"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompileTruthReferencePixelShader.cmake"
    )
    set_tests_properties("${test_name}" PROPERTIES LABELS "shader;fxc;cloud-volume")
  endfunction()

  add_truth_cloud_volume_permutation(
    truth_cloud_volume_fxc_fallback 0 2 TruthCloudFallback
  )
  add_truth_cloud_volume_permutation(
    truth_cloud_volume_fxc_quality_1 1 1 TruthCloudQuality1
  )
  add_truth_cloud_volume_permutation(
    truth_cloud_volume_fxc_quality_2 1 2 TruthCloudQuality2
  )
  add_truth_cloud_volume_permutation(
    truth_cloud_volume_fxc_quality_3 1 3 TruthCloudQuality3
  )

  function(add_truth_aurora_permutation
      test_name quality_macro_value artifact_name)
    add_test(
      NAME "${test_name}"
      COMMAND "${CMAKE_COMMAND}"
        "-DTRUTH_FXC=${TRUTH_FXC_EXECUTABLE}"
        "-DTRUTH_SHADER=${CMAKE_CURRENT_SOURCE_DIR}/shaders/truth/TruthReferenceSky.hlsl"
        "-DTRUTH_REQUIRED_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/shaders/truth/TruthAuroraCurtain.fxh"
        "-DTRUTH_INCLUDE=${CMAKE_CURRENT_SOURCE_DIR}/shaders/truth"
        "-DTRUTH_DEFINE=TRUTH_AURORA_QUALITY=${quality_macro_value}"
        "-DTRUTH_SECOND_DEFINE=TRUTH_ENABLE_CLOUD_VOLUME=1"
        "-DTRUTH_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/shaders/$<CONFIG>/${artifact_name}.cso"
        "-DTRUTH_LISTING=${CMAKE_CURRENT_BINARY_DIR}/shaders/$<CONFIG>/${artifact_name}.asm"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompileTruthReferencePixelShader.cmake"
    )
    set_tests_properties("${test_name}" PROPERTIES LABELS "shader;fxc;aurora")
  endfunction()

  add_truth_aurora_permutation(
    truth_aurora_fxc_fallback 0 TruthAuroraFallback
  )
  add_truth_aurora_permutation(
    truth_aurora_fxc_quality_1 1 TruthAuroraQuality1
  )
  add_truth_aurora_permutation(
    truth_aurora_fxc_quality_2 2 TruthAuroraQuality2
  )
  add_truth_aurora_permutation(
    truth_aurora_fxc_quality_3 3 TruthAuroraQuality3
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

  add_test(
    NAME truth_cloud_volume_fallback_is_distinct
    COMMAND "${CMAKE_COMMAND}" -E compare_files
      "${CMAKE_CURRENT_BINARY_DIR}/shaders/$<CONFIG>/TruthCloudFallback.cso"
      "${CMAKE_CURRENT_BINARY_DIR}/shaders/$<CONFIG>/TruthCloudQuality2.cso"
  )
  set_tests_properties(truth_cloud_volume_fallback_is_distinct PROPERTIES
    DEPENDS "truth_cloud_volume_fxc_fallback;truth_cloud_volume_fxc_quality_2"
    LABELS "shader;fxc;cloud-volume"
    WILL_FAIL TRUE
  )

  add_test(
    NAME truth_cloud_volume_quality_tiers_are_distinct
    COMMAND "${CMAKE_COMMAND}" -E compare_files
      "${CMAKE_CURRENT_BINARY_DIR}/shaders/$<CONFIG>/TruthCloudQuality1.cso"
      "${CMAKE_CURRENT_BINARY_DIR}/shaders/$<CONFIG>/TruthCloudQuality3.cso"
  )
  set_tests_properties(truth_cloud_volume_quality_tiers_are_distinct PROPERTIES
    DEPENDS "truth_cloud_volume_fxc_quality_1;truth_cloud_volume_fxc_quality_3"
    LABELS "shader;fxc;cloud-volume"
    WILL_FAIL TRUE
  )

  add_test(
    NAME truth_aurora_fallback_is_distinct
    COMMAND "${CMAKE_COMMAND}" -E compare_files
      "${CMAKE_CURRENT_BINARY_DIR}/shaders/$<CONFIG>/TruthAuroraFallback.cso"
      "${CMAKE_CURRENT_BINARY_DIR}/shaders/$<CONFIG>/TruthAuroraQuality2.cso"
  )
  set_tests_properties(truth_aurora_fallback_is_distinct PROPERTIES
    DEPENDS "truth_aurora_fxc_fallback;truth_aurora_fxc_quality_2"
    LABELS "shader;fxc;aurora"
    WILL_FAIL TRUE
  )

  add_test(
    NAME truth_aurora_quality_tiers_are_distinct
    COMMAND "${CMAKE_COMMAND}" -E compare_files
      "${CMAKE_CURRENT_BINARY_DIR}/shaders/$<CONFIG>/TruthAuroraQuality1.cso"
      "${CMAKE_CURRENT_BINARY_DIR}/shaders/$<CONFIG>/TruthAuroraQuality3.cso"
  )
  set_tests_properties(truth_aurora_quality_tiers_are_distinct PROPERTIES
    DEPENDS "truth_aurora_fxc_quality_1;truth_aurora_fxc_quality_3"
    LABELS "shader;fxc;aurora"
    WILL_FAIL TRUE
  )

  add_test(
    NAME truth_aurora_default_instruction_budget
    COMMAND "${CMAKE_COMMAND}"
      "-DTRUTH_COMPILE_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompileTruthShader.cmake"
      "-DTRUTH_FXC=${TRUTH_FXC_EXECUTABLE}"
      "-DTRUTH_SHADER=${CMAKE_CURRENT_SOURCE_DIR}/shaders/enbeffect.fx"
      "-DTRUTH_REQUIRED_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/shaders/truth/TruthAtmosphereCore.fxh"
      "-DTRUTH_REQUIRED_SOURCE_2=${CMAKE_CURRENT_SOURCE_DIR}/shaders/truth/TruthSkyFields.fxh"
      "-DTRUTH_REQUIRED_SOURCE_3=${CMAKE_CURRENT_SOURCE_DIR}/shaders/truth/TruthCloudLighting.fxh"
      "-DTRUTH_REQUIRED_SOURCE_4=${CMAKE_CURRENT_SOURCE_DIR}/shaders/truth/TruthAuroraCurtain.fxh"
      "-DTRUTH_INCLUDE=${CMAKE_CURRENT_SOURCE_DIR}/shaders/truth"
      "-DTRUTH_DEFINE=TRUTH_ENABLE_ATMOSPHERE=1"
      "-DTRUTH_SECOND_DEFINE=TRUTH_ENABLE_PROCEDURAL_SKY=1"
      "-DTRUTH_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/shaders/$<CONFIG>/TruthBudgetA1P1.fxo"
      "-DTRUTH_LISTING=${CMAKE_CURRENT_BINARY_DIR}/shaders/$<CONFIG>/TruthBudgetA1P1.asm"
      # IEEE-strict compilation reports 2,443 slots for the ENB-bound path at
      # this revision. Keep only 21 slots of headroom so camera/depth/optical
      # correctness cannot hide a material shader-cost regression.
      "-DTRUTH_MAXIMUM_INSTRUCTION_SLOTS=2464"
      "-DTRUTH_BASELINE_INSTRUCTION_SLOTS=6499"
      "-DTRUTH_BASELINE_REVISION=12776780d4ed3d6ff71b09694dd666f3d07b5cf2"
      "-DTRUTH_QUALITY=low-default"
      "-DTRUTH_REPORT=${CMAKE_CURRENT_BINARY_DIR}/reports/$<CONFIG>/truth-aurora-cost-budget.csv"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckTruthShaderInstructionBudget.cmake"
  )
  set_tests_properties(truth_aurora_default_instruction_budget PROPERTIES
    LABELS "shader;fxc;aurora;performance"
  )

  if(WIN32 AND TRUTH_BUILD_RUNTIME)
    add_custom_target(truth_release_gate
      COMMAND "${CMAKE_CTEST_COMMAND}"
        --test-dir "${CMAKE_CURRENT_BINARY_DIR}"
        -C "$<CONFIG>"
        -R "^(truth_sky_view_adapter_warp|truth_sky_view_adapter_shader_contract|truth_shader_fxc_a1_p1|truth_aurora_default_instruction_budget|truth_runtime_reproducible_release)$"
        --output-on-failure
        --no-tests=error
      DEPENDS truth_sky_view_adapter_warp_tests truth_enb_runtime_plugin
      COMMENT "Running the exact production Truth shader and runtime release gates"
      VERBATIM
    )

    add_custom_target(truth_private_release_package
      COMMAND "${CMAKE_COMMAND}" -E make_directory
        "${CMAKE_CURRENT_BINARY_DIR}/packages/$<CONFIG>"
      COMMAND "${CMAKE_COMMAND}" -E env SOURCE_DATE_EPOCH=946684800
        "${CMAKE_CPACK_COMMAND}"
        --config "${CMAKE_CURRENT_BINARY_DIR}/CPackConfig.cmake"
        -C "$<CONFIG>"
        -G ZIP
        -B "${CMAKE_CURRENT_BINARY_DIR}/packages/$<CONFIG>"
      DEPENDS truth_enb_runtime_plugin truth_release_gate
      COMMENT "Packaging the deterministic Truth ENB private release candidate"
      VERBATIM
    )

    add_test(
      NAME truth_private_release_package_manifest
      COMMAND "${CMAKE_COMMAND}"
        "-DTRUTH_BUILD_ROOT=${CMAKE_CURRENT_BINARY_DIR}"
        "-DTRUTH_CONFIGURATION=$<CONFIG>"
        "-DTRUTH_WORK_ROOT=${CMAKE_CURRENT_BINARY_DIR}/package-validation/$<CONFIG>"
        "-DTRUTH_SOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
        "-DTRUTH_RUNTIME_PLUGIN=$<TARGET_FILE:truth_enb_runtime_plugin>"
        "-DTRUTH_CPACK=${CMAKE_CPACK_COMMAND}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckTruthReleasePackage.cmake"
    )
    set_tests_properties(truth_private_release_package_manifest PROPERTIES
      LABELS "install;package;manifest;determinism;release"
    )
  endif()
endif()
