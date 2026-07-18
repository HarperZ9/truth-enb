# Truth ENB Ordered Five-Tier Public Shader Suite Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace Truth's legacy-adapted shader surround with a restrained, artifact-resistant, nine-stage Truth-owned ENBSeries 0.504 suite and five complete quality presets.

**Architecture:** One modular HLSL source tree produces five compile-time quality permutations. A canonical CSV drives deterministic preset generation; runtime controls tune bounded intensities without changing loop bounds. ENB's host order is explicit, HDR effects compose once before tone mapping, and LDR finishing is intentionally minimal.

**Tech Stack:** Effects 11 / HLSL Shader Model 5, x64 FXC 10.0.26100, CMake 3.30+, C++23, D3D11 WARP, ENB SDK 1002, enb-runtime-core.

## Global Constraints

- Preserve `shaders/enb/ENBSeries0504VanillaPostProcess.fxh` byte-for-byte.
- Preserve the `SB_Retain` dead-strip retention contract in every SkyrimBridge-consuming stage.
- `TRUTH_QUALITY_TIER` accepts exactly `0..4`; authored default is `1` (`Balanced`).
- Keep all color linear/HDR until `enbeffect.fx`; `enbeffectpostpass.fx` is LDR and dithers last.
- Every public intensity has a documented identity and bounded maximum.
- Do not copy legacy shader source, private reversal material, or protected artifacts.
- Do not redistribute ENB binaries.
- Exclude the separately GPL-3.0-or-later `tools/sky-mesh` tool from the runtime ZIP.
- Public runtime failure is fail-closed: preserve the authored scene when camera/celestial payloads are invalid.

---

### Task 1: Five-tier quality contract and deterministic presets

**Files:**
- Create: `config/quality-tiers.csv`
- Create: `shaders/truth/TruthQuality.fxh`
- Create: `cmake/GenerateTruthQualityPresets.cmake`
- Create: `cmake/CheckTruthQualityPresets.cmake`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: CMake configure variables and the canonical CSV.
- Produces: `TRUTH_QUALITY_TIER`, fixed `TruthQuality*` constants, and generated `presets/{performance,balanced,quality,ultra,cinematic}/ROOT/enbseries/*.ini`.

- [ ] **Step 1: Add a failing manifest/generator contract test**

```cmake
add_test(
  NAME truth_quality_presets
  COMMAND "${CMAKE_COMMAND}"
    "-DTRUTH_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
    "-DTRUTH_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckTruthQualityPresets.cmake")
set_tests_properties(truth_quality_presets PROPERTIES
  LABELS "shader;quality;presets;determinism")
```

- [ ] **Step 2: Run the focused test and confirm it fails**

Run: `ctest --test-dir build -C Debug -R "^truth_quality_presets$" --output-on-failure`

Expected: FAIL because `CheckTruthQualityPresets.cmake` or generated presets do not exist.

- [ ] **Step 3: Add the canonical five rows**

```csv
tier,id,label,cloud_mode,cloud_primary_steps,cloud_light_steps,aurora_samples,ao_directions,ao_steps,dof_rings,bloom_radius,ssr_steps
0,performance,Performance,analytic,0,0,1,4,2,0,2,0
1,balanced,Balanced,analytic,0,0,2,6,3,2,3,0
2,quality,Quality,volume,8,2,4,8,4,3,4,8
3,ultra,Ultra,volume,12,3,7,12,5,4,5,12
4,cinematic,Cinematic,volume,16,4,10,16,6,5,6,16
```

- [ ] **Step 4: Implement the compile-time quality include**

```hlsl
#ifndef TRUTH_QUALITY_TIER
#define TRUTH_QUALITY_TIER 1
#endif
#if TRUTH_QUALITY_TIER < 0 || TRUTH_QUALITY_TIER > 4
#error TRUTH_QUALITY_TIER must be in [0,4]
#endif

static const uint TruthQualityTier = TRUTH_QUALITY_TIER;
#if TRUTH_QUALITY_TIER == 0
static const uint TruthQualityCloudPrimarySteps = 0u;
static const uint TruthQualityCloudLightSteps = 0u;
static const uint TruthQualityAuroraSamples = 1u;
#elif TRUTH_QUALITY_TIER == 1
static const uint TruthQualityCloudPrimarySteps = 0u;
static const uint TruthQualityCloudLightSteps = 0u;
static const uint TruthQualityAuroraSamples = 2u;
#elif TRUTH_QUALITY_TIER == 2
static const uint TruthQualityCloudPrimarySteps = 8u;
static const uint TruthQualityCloudLightSteps = 2u;
static const uint TruthQualityAuroraSamples = 4u;
#elif TRUTH_QUALITY_TIER == 3
static const uint TruthQualityCloudPrimarySteps = 12u;
static const uint TruthQualityCloudLightSteps = 3u;
static const uint TruthQualityAuroraSamples = 7u;
#else
static const uint TruthQualityCloudPrimarySteps = 16u;
static const uint TruthQualityCloudLightSteps = 4u;
static const uint TruthQualityAuroraSamples = 10u;
#endif
```

- [ ] **Step 5: Implement deterministic preset generation and validation**

The generator must reject missing/duplicate tiers, unexpected IDs, invalid integers, and output outside its owned build tree. Each preset writes all nine `.fx.ini` files plus `truth-quality.ini`; every generated file starts with:

```ini
; Generated from config/quality-tiers.csv
; Product=Truth ENB
; Tier=balanced
```

The checker runs generation twice, hashes sorted relative paths and bytes, requires byte-identical trees, and verifies exactly five tier directories and 50 INI files.

- [ ] **Step 6: Re-run the focused test**

Run: `ctest --test-dir build -C Debug -R "^truth_quality_presets$" --output-on-failure`

Expected: PASS with five deterministic complete presets.

- [ ] **Step 7: Commit**

```powershell
git add config/quality-tiers.csv shaders/truth/TruthQuality.fxh cmake/GenerateTruthQualityPresets.cmake cmake/CheckTruthQualityPresets.cmake CMakeLists.txt
git commit -m "feat: add five Truth quality presets"
```

### Task 2: Shared pipeline contract and strict 45-permutation compile matrix

**Files:**
- Create: `shaders/truth/TruthPipelineCommon.fxh`
- Create: `shaders/truth/TruthHostCapabilities.fxh`
- Create: `shaders/truth/TruthStageParameters.fxh`
- Create: `shaders/enbeffectprepass.fx`
- Create: `shaders/enbdepthoffield.fx`
- Create: `shaders/enbbloom.fx`
- Create: `shaders/enbadaptation.fx`
- Create: `shaders/enblens.fx`
- Modify: `shaders/enbeffect.fx`
- Create: `shaders/enbeffectpostpass.fx`
- Create: `shaders/enbsunsprite.fx`
- Create: `shaders/enbunderwater.fx`
- Create: `cmake/CompileTruthStage.cmake`
- Create: `cmake/CheckTruthStageMatrix.cmake`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: ENB-owned textures/uniforms and `TruthQuality.fxh`.
- Produces: finite sanitizers, one depth convention, exact identity functions, an explicit native/Bridge/spatial/identity capability ladder, current-frame scratch ownership, and nine compilable stage files for each of five tiers.

- [ ] **Step 1: Register a failing compile-matrix test**

```cmake
add_test(
  NAME truth_stage_compile_matrix
  COMMAND "${CMAKE_COMMAND}"
    "-DTRUTH_FXC=${TRUTH_FXC_EXECUTABLE}"
    "-DTRUTH_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
    "-DTRUTH_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckTruthStageMatrix.cmake")
set_tests_properties(truth_stage_compile_matrix PROPERTIES
  LABELS "shader;fxc;quality;matrix")
```

- [ ] **Step 2: Run and confirm the missing-stage failure**

Run: `ctest --test-dir build -C Debug -R "^truth_stage_compile_matrix$" --output-on-failure`

Expected: FAIL naming the first absent stage.

- [ ] **Step 3: Add the common contract**

```hlsl
bool TruthFinite1(float value)
{
    return (asuint(value) & 0x7fffffffu) < 0x7f800000u;
}

float3 TruthFiniteOrBlack(float3 value)
{
    return all((asuint(value) & 0x7fffffffu) < 0x7f800000u.xxx)
        ? max(value, 0.0.xxx)
        : 0.0.xxx;
}

float TruthSkyMask(float raw_depth, float threshold, float feather)
{
    float safe_feather = clamp(feather, 0.00001, 0.005);
    return smoothstep(clamp(threshold, 0.99, 1.0),
                      min(clamp(threshold, 0.99, 1.0) + safe_feather, 1.0),
                      raw_depth);
}
```

- [ ] **Step 4: Declare host capabilities and scratch ownership**

`TruthHostCapabilities.fxh` defines four ordered levels:

```hlsl
#define TRUTH_CAPABILITY_IDENTITY 0
#define TRUTH_CAPABILITY_SPATIAL  1
#define TRUTH_CAPABILITY_BRIDGE   2
#define TRUTH_CAPABILITY_NATIVE   3
```

Each stage must define whether it owns color, depth, normal, mask, native
celestial/view data, previous scalar adaptation, and named current-frame
scratch before including the contract. Full-frame history and object motion
vectors are unavailable in the initial public release and must remain `0`.
Reject invalid capability values, undeclared scratch reads, cross-effect alpha
packing, and any code path that treats a current-frame target as persistent
history.

- [ ] **Step 5: Add restrained stage parameters**

Declare one master, one stage enable, one stage intensity, and advanced shape controls only. Use ordered UI prefixes `[Truth 00]` through `[Truth 90]`. Defaults must match the Balanced generated preset and zero intensity must be an exact identity.

- [ ] **Step 6: Add all stage files with correct ENB techniques and identity output**

Each stage initially compiles as a host-correct identity. For example:

```hlsl
float4 TruthPrepassMain(VS_OUTPUT_POST input) : SV_Target
{
    return TextureColor.Sample(Sampler0, input.txcoord0);
}
```

Preserve the existing `ORIGINALPOSTPROCESS` technique only in `enbeffect.fx`.

- [ ] **Step 7: Implement the matrix checker**

Compile the nine stages for tiers `0..4` with `/Ges /WX /O3` and the stage's required entry/technique profile. Write listings below `build/shader-matrix/<tier>/`. Reject warnings and missing `TRUTH_QUALITY_TIER`.

The checker also requires every stage's capability declaration and proves that
native, Bridge-assisted, spatial-fallback, and identity helpers preserve the
same interface. It rejects full-frame temporal claims in this release.

- [ ] **Step 8: Run the matrix**

Run: `ctest --test-dir build -C Debug -R "^truth_stage_compile_matrix$" --output-on-failure`

Expected: PASS, 45 stage/tier compilations.

- [ ] **Step 9: Commit**

```powershell
git add shaders cmake/CompileTruthStage.cmake cmake/CheckTruthStageMatrix.cmake CMakeLists.txt
git commit -m "feat: establish ordered Truth shader stages"
```

### Task 3: HDR prepass environment, cloud tiers, aurora, and interior safety

**Files:**
- Create: `shaders/truth/TruthPrepassCore.fxh`
- Create: `shaders/truth/TruthScreenSpace.fxh`
- Create: `cmake/CheckTruthSceneContracts.cmake`
- Modify: `shaders/enbeffectprepass.fx`
- Modify: `shaders/truth/TruthCloudVolume.fxh`
- Modify: `shaders/truth/TruthAuroraCurtain.fxh`
- Modify: `shaders/truth/TruthInteriorLight.fxh`
- Modify: `shaders/enbeffect.fx`
- Modify: `tests/ReferenceRendererTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: validated view/celestial runtime payload, depth, `TruthQuality*` constants, existing atmosphere/sky/cloud/aurora/interior math.
- Produces: one HDR scene color with environment and interior response composed exactly once.

The atmosphere implementation adapts the optical-depth, Rayleigh/Mie/ozone,
and depth-bounded ray concepts from Hillaire's production atmosphere model and
Maxime Heckel's explanatory implementation. It must not transplant a
full-resolution nested view/light march. Native ENB celestial data is first
choice, the versioned SkyrimBridge celestial payload is second, the stable
authored analytic direction is the last spatial fallback, and invalid
confidence preserves the scene.

- [ ] **Step 1: Add failing reference cases**

Add assertions for analytic tiers `0/1`, volume tiers `2/3/4`, sealed interior identity, invalid runtime identity, phase wrap, and camera-stable world sampling. Required case names:

```cpp
"performance-analytic-day",
"balanced-analytic-night",
"quality-volume-cloud",
"ultra-volume-cloud",
"cinematic-volume-cloud",
"sealed-interior",
"invalid-runtime-preserves-scene",
"ao-flat-surface-is-neutral",
"ao-depth-edge-is-rejected",
"ssr-miss-preserves-scene",
"sss-non-skin-preserves-scene"
```

- [ ] **Step 2: Run the focused CPU/reference test**

Register `truth_scene_contracts`, then run:

`ctest --test-dir build -C Debug -R "truth_(reference_renderer|cloud_volume_cpp|interior_light_cpp|scene_contracts)" --output-on-failure`

Expected: FAIL on missing tier-aware composition.

- [ ] **Step 3: Make cloud/aurora loop bounds consume `TruthQuality.fxh`**

Remove the former three-level cloud and four-level aurora macros. Tier `0/1` must compile out volume marching; tiers `2/3/4` use `8/2`, `12/3`, and `16/4`. Do not add frame-random jitter without a temporal resolver.

- [ ] **Step 4: Implement bounded modern screen-space fallbacks**

`TruthScreenSpace.fxh` independently authors current-frame techniques against
the declared host capabilities:

- GTAO-like visibility and contact response use `TruthQualityAODirections ×
  TruthQualityAOSteps`, native normals when valid, a depth-derived fallback,
  sky rejection, and bilateral depth/normal confidence. Flat unoccluded
  surfaces and zero intensity are exact identities.
- SSR is compiled out when `TruthQualitySSRSteps == 0`; higher tiers use the
  fixed `8/12/16` steps, short view-space marching, binary refinement, edge and
  thickness confidence, and exact miss identity. It cannot claim roughness,
  object motion, or temporal history that the host has not supplied.
- Subsurface diffusion is restricted to a validated skin/material mask, uses a
  small depth/normal-aware current-frame kernel, preserves non-skin pixels
  exactly, and never reuses alpha as hidden history.

Sampling is world/screen stable and deterministic. No frame-random jitter,
luminance-as-motion, unowned scratch, or current-frame-as-history path is
allowed. `CheckTruthSceneContracts.cmake` supplies positive ownership checks
and negative forbidden-resource cases.

- [ ] **Step 5: Implement one prepass compositor**

```hlsl
struct TruthPrepassResult
{
    float3 color;
    float environment_applied;
};

TruthPrepassResult TruthComposePrepass(
    float3 scene,
    float raw_depth,
    float2 texcoord,
    float interior_factor);
```

The function returns the original finite scene when runtime data is invalid, applies exterior sky only through the shared sky mask, applies interior light only inside interiors, and never applies both branches to the same pixel.

Environment is composed first through the sky/exterior mask. Scene-space
visibility, SSR, and mask-aware diffusion then operate only on valid geometry
and select the declared capability ladder. The result remains linear HDR and
finite.

- [ ] **Step 6: Remove duplicate sky/environment composition from `enbeffect.fx`**

`enbeffect.fx` must consume the prepass result as `TextureColor`; its responsibility becomes optical mix, exposure, tone mapping, and color only.

- [ ] **Step 7: Run targeted math, matrix, and reference tests**

Run: `ctest --test-dir build -C Debug -R "truth_(reference_renderer|cloud_volume_cpp|aurora_curtain_cpp|interior_light_cpp|scene_contracts|stage_compile_matrix)" --output-on-failure`

Expected: PASS.

- [ ] **Step 8: Commit**

```powershell
git add shaders cmake/CheckTruthSceneContracts.cmake tests/ReferenceRendererTests.cpp CMakeLists.txt
git commit -m "feat: compose Truth environment in HDR prepass"
```

### Task 4: Restrained DOF, bloom, adaptation, and lens stages

**Files:**
- Create: `shaders/truth/TruthDepthOfField.fxh`
- Create: `shaders/truth/TruthBloom.fxh`
- Create: `shaders/truth/TruthAdaptation.fxh`
- Create: `shaders/truth/TruthLens.fxh`
- Modify: `shaders/enbdepthoffield.fx`
- Modify: `shaders/enbbloom.fx`
- Modify: `shaders/enbadaptation.fx`
- Modify: `shaders/enblens.fx`
- Create: `cmake/CheckTruthOpticalContracts.cmake`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: HDR scene, depth/aperture, bloom source, adaptation history, tier constants.
- Produces: isolated optical stages with exact disabled identities and bounded cost.

- [ ] **Step 1: Add failing optical contract checks**

The checker must require:

```text
Performance: DOF default off, bloom radius 2, lens ghosts 0
Balanced: DOF rings 2, bloom radius 3, lens ghosts <= 1
Quality: DOF rings 3, bloom radius 4, lens ghosts <= 2
Ultra: DOF rings 4, bloom radius 5, lens ghosts <= 2
Cinematic: DOF rings 5, bloom radius 6, lens ghosts <= 3
```

It must also verify zero-strength identity code paths and reject `discard`.

- [ ] **Step 2: Run and confirm failure**

Run: `ctest --test-dir build -C Debug -R "^truth_optical_contracts$" --output-on-failure`

Expected: FAIL before modules are implemented.

- [ ] **Step 3: Implement bounded modules**

Expose exactly:

```hlsl
float3 TruthApplyDepthOfField(float2 uv, float3 scene, float linear_depth);
float3 TruthApplyBloom(float2 uv, float3 hdr_source);
float TruthUpdateAdaptedLuminance(float measured, float history, float delta_seconds);
float3 TruthApplyLens(float2 uv, float3 bloom, float3 scene);
```

Bloom uses soft-knee luminance extraction; adaptation clamps brighten/darken rates to the existing `3.0/1.5 EV/s`; lens consumes bloom rather than the raw scene.

- [ ] **Step 4: Run optical and compile tests**

Run: `ctest --test-dir build -C Debug -R "truth_(optical_contracts|stage_compile_matrix|master_look_cpp)" --output-on-failure`

Expected: PASS.

- [ ] **Step 5: Commit**

```powershell
git add shaders cmake/CheckTruthOpticalContracts.cmake CMakeLists.txt
git commit -m "feat: add restrained Truth optical stages"
```

### Task 5: Main color, LDR finish, sun, and underwater stages

**Files:**
- Create: `shaders/truth/TruthPostFinish.fxh`
- Create: `shaders/truth/TruthSunSprite.fxh`
- Create: `shaders/truth/TruthUnderwater.fxh`
- Modify: `shaders/enbeffect.fx`
- Modify: `shaders/enbeffectpostpass.fx`
- Modify: `shaders/enbsunsprite.fx`
- Modify: `shaders/enbunderwater.fx`
- Create: `cmake/CheckTruthCompositionContracts.cmake`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: one HDR scene, bloom/lens outputs, exposure, engine-aligned sun, underwater mask.
- Produces: display color, minimal LDR finish, and mutually exclusive air/underwater context.

- [ ] **Step 1: Add failing single-application checks**

Require `enbeffect.fx` to call each of exposure, tone curve, and gamut handling once; require postpass to call dithering last; reject bloom/lens/fog functions from postpass; reject air-atmosphere calls from underwater.

- [ ] **Step 2: Run and confirm failure**

Run: `ctest --test-dir build -C Debug -R "^truth_composition_contracts$" --output-on-failure`

Expected: FAIL naming duplicate or missing stage ownership.

- [ ] **Step 3: Implement final-stage functions**

```hlsl
float3 TruthFinishLdr(float2 uv, float3 display_color);
float3 TruthEvaluateSunSprite(float2 uv, float3 sun_direction, float visibility);
float3 TruthEvaluateUnderwater(float2 uv, float3 scene, float linear_depth);
```

`TruthFinishLdr` order is vignette, fine grain, triangular dither. It returns stable alpha `1.0`. Underwater uses one Beer-Lambert absorption/scattering model and disables air fog and lens dirt.

- [ ] **Step 4: Run composition, matrix, and existing master-look tests**

Run: `ctest --test-dir build -C Debug -R "truth_(composition_contracts|stage_compile_matrix|master_look_cpp)" --output-on-failure`

Expected: PASS.

- [ ] **Step 5: Commit**

```powershell
git add shaders cmake/CheckTruthCompositionContracts.cmake CMakeLists.txt
git commit -m "feat: complete ordered Truth display stages"
```

### Task 6: Backward-compatible real celestial runtime payload

**Files:**
- Modify: `runtime/include/truth/runtime/CameraFrame.hpp`
- Modify: `runtime/include/truth/runtime/RuntimeController.hpp`
- Modify: `runtime/src/RuntimeController.cpp`
- Modify: `runtime/include/truth/runtime/NativeCameraProvider.hpp`
- Modify: `runtime/src/NativeCameraProvider.cpp`
- Modify: `runtime/tests/RuntimeControllerTests.cpp`
- Modify: `runtime/tests/NativeCameraProviderTests.cpp`
- Modify: `shaders/truth/TruthRuntimeParameters.fxh`
- Modify: `shaders/enbeffectprepass.fx`

**Interfaces:**
- Consumes: validated engine celestial direction from the runtime provider.
- Produces: optional seventh `float4` key `Truth Runtime | Celestial`; `.xyz` normalized sun direction, `.w` validity. Existing six-vector protocol remains readable.

- [ ] **Step 1: Add failing runtime transaction tests**

Test that the seventh value is captured/restored with the baseline, written before `Status.valid`, normalized, rejected when non-finite or near-zero, and optional for protocol `1.0`.

- [ ] **Step 2: Run focused runtime tests**

Run: `ctest --test-dir build -C Debug -R "truth_runtime_(controller|native_camera)" --output-on-failure`

Expected: FAIL because the celestial key/provider is absent.

- [ ] **Step 3: Extend the payload without breaking protocol 1.0**

```cpp
struct CelestialFrame final {
    Float4 sun_direction_valid{};
};

inline constexpr std::string_view kCelestialParameterKey =
    "Truth Runtime | Celestial";
inline constexpr float kProtocolVersionWithCelestial = 1.1F;
```

Publish the celestial vector before status and restore it before the captured final status. The shader accepts protocol `1.0` camera data but enables engine-aligned sun-dependent rendering only when the celestial `.w > 0.5`.

- [ ] **Step 4: Replace the game-hour proxy**

`TruthResolveProceduralSunDirection` must return the validated runtime direction. If absent, prepass preserves the authored sky for sun-dependent replacement rather than inventing an azimuth.

- [ ] **Step 5: Run runtime and shader tests**

Run: `ctest --test-dir build -C Debug -R "truth_(runtime_.*|stage_compile_matrix|reference_renderer)" --output-on-failure`

Expected: PASS.

- [ ] **Step 6: Commit**

```powershell
git add runtime shaders tests CMakeLists.txt
git commit -m "feat: publish real sun direction to Truth shaders"
```

### Task 7: Public release package, provenance, and focused acceptance

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `cmake/CheckTruthReleasePackage.cmake`
- Modify: `README.md`
- Modify: `docs/architecture.md`
- Modify: `docs/release-validation.md`
- Create: `CREDITS-AND-PROVENANCE.md`
- Create: `THIRD_PARTY_NOTICES.md`
- Modify: `tools/sky-mesh/README.md`

**Interfaces:**
- Consumes: nine stages, five presets, runtime plugin, immutable fallback, license boundaries.
- Produces: deterministic `Truth-ENB-win64.zip`, checksum, exact manifest, and honest public documentation.

- [ ] **Step 1: Make the package test expect the public layout**

Require exactly the nine `.fx` files, Truth includes, five complete preset trees, runtime plugin, README/license/credits/notices, and immutable ENB fallback. Reject `.pdb`, `.exe`, ENB DLLs, `tools/sky-mesh`, `Private`, `RC`, and any path containing `protected`.

- [ ] **Step 2: Run and confirm package failure**

Run: `ctest --test-dir build -C Release -R "^truth_.*release_package_manifest$" --output-on-failure`

Expected: FAIL because the package still has private-RC naming/content.

- [ ] **Step 3: Update install/CPack rules and deterministic archive name**

Set:

```cmake
set(CPACK_PACKAGE_NAME "Truth-ENB")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_FILE_NAME "Truth-ENB-1.0.0-win64")
```

Install only the approved runtime files and generated presets.

- [ ] **Step 4: Correct public documentation**

State MIT at the root; document `tools/sky-mesh` as separately GPL-3.0-or-later and excluded from runtime distribution. Credit Boris Vorontsov, Kitsuune/LonelyKitsuune, kingeric1992, Adyss, TreyM, l00ping, TheSandvichMaker/ReforgedUI, Marty McFly/Pascal Gilcher, and the scientific technique lineage from the design. Describe Kitsuune interoperability as independently authored compatibility; do not label the binary-reversal history clean-room.

- [ ] **Step 5: Run the targeted release gates**

Run:

```powershell
ctest --test-dir build -C Release -R "truth_(quality_presets|stage_compile_matrix|optical_contracts|composition_contracts|master_look_cpp|interior_light_cpp|cloud_volume_cpp|aurora_curtain_cpp|runtime_.*|.*release_package_manifest)" --output-on-failure
cmake --build build --config Release --target truth_public_release_package
```

Expected: all selected tests PASS and a deterministic public ZIP plus SHA-256 sidecar exists.

- [ ] **Step 6: Record live acceptance without overstating it**

Use `docs/release-validation.md` to record actual SE/AE and ENB 0.504 results. Do not mark unrun game cases passed. Test Performance, Balanced, and Cinematic in-game first; only expand to all tiers if those expose a tier-specific fault.

- [ ] **Step 7: Commit**

```powershell
git add CMakeLists.txt cmake README.md docs CREDITS-AND-PROVENANCE.md THIRD_PARTY_NOTICES.md tools/sky-mesh/README.md
git commit -m "chore: prepare Truth ENB public shader release"
```
