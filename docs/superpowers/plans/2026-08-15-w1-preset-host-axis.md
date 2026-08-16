# W1: Preset Host Axis Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship Truth ENB presets that behave correctly under both ENBSeries and Effects 11 from one shader tree, with the host selected by generated ini rather than a compile-time define.

**Architecture:** Effects 11 injects no preprocessor defines into preset shaders, so the host cannot be detected at compile time. Host selection therefore travels through configuration. A new `config/hosts.csv` adds a second axis to the existing tier generator, producing `Presets/<host>/<tier>/` instead of `Presets/<tier>/`. The shader sources do not fork. One shader change is required first, because Truth's vignette strength is currently welded to the postpass gate and cannot be zeroed without also losing dithering.

**Tech Stack:** CMake 3.30+, HLSL `fx_5_0`, C++23 WARP tests, Visual Studio 18 2026 x64.

## Global Constraints

- Effects 11 passes `nullptr` for `pDefines` at both preset compile sites (`Effect.cpp:301,369`). Never add a task that depends on a compile-time host define.
- `config/quality-tiers.csv` has a hash-locked contract: `cmake/GenerateTruthQualityPresets.cmake` asserts exactly 6 lines, an exact header string, and all five exact row strings. Do not modify that file or those assertions.
- The ENBSeries variant must stay byte-identical to its current rendered baseline. Every task that touches a shader proves this by sha256.
- Repository licence is MIT. No Community Shaders source enters this repository at any point.
- Shipped copy carries no em-dashes, per the workspace voice canon.
- Configure with `cmake --preset vs2026-x64`. Build and test with the `vs2026-x64-debug` preset.

---

### Task 1: Separate vignette strength from the postpass gate

`TruthFinishLdr` early-returns when `TruthPostpassIntensity <= 0.0`, which skips vignette, grain, and `TruthTriangularDither` together. Vignette strength is the hardcoded literal `0.18`. The Effects 11 variant needs vignette and grain off while dithering stays on, so strength needs its own uniform. Default `0.18` preserves current output exactly.

**Correction, 2026-08-15.** The first draft of this task tested through `RenderWarpReference`. That was wrong twice. `RenderWarpReference` compiles the entry point `TruthReferencePixelMain` from `shaders/truth/TruthReferenceSky.hlsl`, which exercises the sky stack. `TruthFinishLdr` is called from `enbeffectpostpass.fx:42` and is never reached on that path, so the test would have passed identically before and after the change. It also asserted against a sha256 baseline, which pins a number without stating a behaviour. The corrected task follows the probe pattern already established by `tests/ScreenSpaceWarpTests.cpp` and `shaders/truth/TruthScreenSpaceWarpProbe.hlsl`, and asserts behaviour.

**Files:**
- Modify: `shaders/truth/TruthStageParameters.fxh:41`
- Modify: `shaders/truth/TruthPostFinish.fxh:28-31`
- Create: `shaders/truth/TruthPostFinishWarpProbe.hlsl`
- Create: `tests/PostFinishWarpTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: nothing from earlier tasks.
- Produces: HLSL uniform `float TruthPostpassVignetteStrength`, range `0.0` to `1.0`, default `0.18`. Task 2 writes this key into generated ini files.

- [x] **Step 1: Write the failing test**

Two files, following the probe pattern established by `shaders/truth/TruthScreenSpaceWarpProbe.hlsl` and `tests/ScreenSpaceWarpTests.cpp`.

`shaders/truth/TruthPostFinishWarpProbe.hlsl` declares the four globals the header reads in an explicit `cbuffer TruthPostFinishProbeParams : register(b0)`, includes `TruthPostFinish.fxh`, and writes two outputs per probe point: `TruthFinishLdr(uv, colour)` and `TruthTriangularDither(uv, colour)`. Emitting both is what lets case 3 assert the stage collapses to dither alone rather than merely changing.

`tests/PostFinishWarpTests.cpp` stands up a WARP device, compiles the probe with `D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS | D3DCOMPILE_IEEE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3`, and dispatches two probe points: the centre at uv `(0.5, 0.5)`, where `dot(centered, centered)` is zero so no vignette applies, and the corner at uv `(0.0, 0.0)`, where it is strongest. Both carry 0.5 grey, which keeps every result clear of the `saturate` clamps.

Three cases, all relations rather than golden values:

1. At the shipped default of `0.18` the corner is darker than the centre by more than the dither floor. This is the regression lock and must hold before and after.
2. At strength `0.0` the corner and centre differ by no more than two 255ths, which is the dither bound.
3. At strength `0.0` and grain `0.0` the stage output equals the dither-only output exactly, at every probe point. This is the contract the Effects 11 variant depends on, and it is what forbids reaching the same result by zeroing `TruthPostpassIntensity`.

Register the target in `CMakeLists.txt` after the `truth_screen_space_warp` block, linking `d3d11` and `d3dcompiler`, with labels `gpu;warp;shader;postpass`.

- [x] **Step 2: Run the test to verify it fails**

```bash
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug --target truth_post_finish_warp_tests
ctest --preset vs2026-x64-debug -R truth_post_finish_warp --output-on-failure
```

Expected, and observed: case 1 passes, cases 2 and 3 fail.

```
at zero strength the corner and centre must differ only by dither; corner=0.473320 centre=0.501561
with strength and grain at zero the stage must equal dither alone at probe point 1; finished=0.473320 dither_only=0.498070
```

The corner sitting at 0.473 against a centre of 0.502 is the hardcoded `0.18` ignoring the uniform, which is exactly the defect.

- [x] **Step 3: Add the uniform and replace the literal**

In `shaders/truth/TruthStageParameters.fxh`, after `TruthPostpassGrainShape` in the `TRUTH_STAGE_PARAMETER_SLOT == 6` block:

```hlsl
float TruthPostpassVignetteStrength <string UIName = "[Truth 70] Postpass | Vignette Strength"; string UIWidget = "Spinner"; float UIMin = 0.0; float UIMax = 1.0; float UIStep = 0.01;> = 0.18;
```

In `shaders/truth/TruthPostFinish.fxh`, replace the literal:

```hlsl
    float3 finished = lerp(
        display_color,
        display_color * vignette,
        TruthPostpassVignetteStrength * saturate(TruthPostpassIntensity));
```

- [x] **Step 4: Run the test to verify it passes**

```bash
ctest --preset vs2026-x64-debug -R truth_post_finish_warp --output-on-failure
```

Observed: `Passed`. A default of `0.18` multiplied by the same intensity reproduces the previous literal exactly.

- [x] **Step 5: Verify by mutation, then revert**

Replace the final `return TruthTriangularDither(uv, finished);` with `return finished;`, which is the failure the design exists to prevent. Observed: case 3 fails at both probe points.

```
with strength and grain at zero the stage must equal dither alone at probe point 0; finished=0.500000 dither_only=0.501561
with strength and grain at zero the stage must equal dither alone at probe point 1; finished=0.500000 dither_only=0.498070
```

Revert and confirm the suite is green.

- [x] **Step 6: Run the full suite, then commit**

```bash
cmake --build --preset vs2026-x64-debug
ctest --preset vs2026-x64-debug
```

Observed: 33 of 33 pass, no compiler or fxc warnings. The production shader now reads a uniform where it read a literal, so the whole suite runs rather than the one new test.

```bash
git add shaders/truth/TruthStageParameters.fxh shaders/truth/TruthPostFinish.fxh shaders/truth/TruthPostFinishWarpProbe.hlsl tests/PostFinishWarpTests.cpp CMakeLists.txt
git commit -m "feat: give postpass vignette its own strength uniform"
```


---

### Task 2: Add the host axis to the preset generator

**Files:**
- Create: `config/hosts.csv`
- Modify: `cmake/GenerateTruthQualityPresets.cmake`
- Modify: `cmake/CheckTruthQualityPresets.cmake:196-212`

**Interfaces:**
- Consumes: `TruthPostpassVignetteStrength` from Task 1.
- Produces: preset output at `Presets/<host_id>/<tier_id>/ROOT/enbseries/truth-quality.ini` for host ids `enbseries` and `effects11`. Task 3 asserts this layout.

- [ ] **Step 1: Write the failing gate assertion**

In `cmake/CheckTruthQualityPresets.cmake`, replace the single-axis glob at line 196 with a host-aware one:

```cmake
set(expected_hosts enbseries effects11)
foreach(host_name IN LISTS expected_hosts)
  foreach(tier_name IN LISTS expected_ids)
    set(host_tier_ini
      "${first_output}/${host_name}/${tier_name}/ROOT/enbseries/truth-quality.ini")
    if(NOT EXISTS "${host_tier_ini}")
      message(FATAL_ERROR
        "Truth preset generation did not produce: ${host_tier_ini}")
    endif()
  endforeach()
endforeach()
```

- [ ] **Step 2: Run the gate to verify it fails**

```bash
cmake --preset vs2026-x64
```

Expected: FATAL_ERROR naming `.../enbseries/performance/ROOT/enbseries/truth-quality.ini` as absent, because the generator still writes the single-axis layout.

- [ ] **Step 3: Create the host manifest**

Create `config/hosts.csv`. Only the keys that differ per host appear here. `postpass_vignette_strength` and `postpass_grain_shape` go to zero under Effects 11 because Community Shaders owns those stages; `postpass_intensity` stays at 1.0 so dithering survives.

```csv
host,id,label,postpass_intensity,postpass_vignette_strength,postpass_grain_shape
0,enbseries,ENBSeries,1.0,0.18,0.0
1,effects11,Effects 11,1.0,0.0,0.0
```

- [ ] **Step 4: Extend the generator**

In `cmake/GenerateTruthQualityPresets.cmake`, after the existing quality-manifest validation block, add host-manifest validation using the same shape:

```cmake
if(NOT DEFINED TRUTH_HOSTS_CSV OR "${TRUTH_HOSTS_CSV}" STREQUAL "")
  set(TRUTH_HOSTS_CSV "${truth_source_dir}/config/hosts.csv")
endif()
if(NOT EXISTS "${TRUTH_HOSTS_CSV}")
  message(FATAL_ERROR "Truth host manifest is absent: ${TRUTH_HOSTS_CSV}")
endif()
file(REAL_PATH "${TRUTH_HOSTS_CSV}" truth_hosts_csv)

file(STRINGS "${truth_hosts_csv}" host_lines ENCODING UTF-8)
list(LENGTH host_lines host_line_count)
if(NOT host_line_count EQUAL 3)
  message(FATAL_ERROR
    "Truth host manifest must contain one header and exactly two hosts; found ${host_line_count} lines")
endif()

list(GET host_lines 0 host_header)
set(expected_host_header
  "host,id,label,postpass_intensity,postpass_vignette_strength,postpass_grain_shape")
if(NOT host_header STREQUAL expected_host_header)
  message(FATAL_ERROR "Truth host manifest header does not match the canonical contract")
endif()

set(expected_host_rows
  "0,enbseries,ENBSeries,1.0,0.18,0.0"
  "1,effects11,Effects 11,1.0,0.0,0.0")
foreach(host_index RANGE 1 2)
  list(GET host_lines ${host_index} host_line)
  math(EXPR expected_row_index "${host_index} - 1")
  list(GET expected_host_rows ${expected_row_index} expected_host_row)
  if(NOT host_line STREQUAL expected_host_row)
    message(FATAL_ERROR
      "Truth host manifest row ${host_index} does not match the canonical contract")
  endif()
endforeach()
```

Then wrap the existing per-tier write loop in a per-host loop, and append the three host keys to each generated ini:

```cmake
foreach(host_index RANGE 1 2)
  list(GET host_lines ${host_index} host_line)
  string(REPLACE "," ";" host_fields "${host_line}")
  list(GET host_fields 1 host_id)
  list(GET host_fields 2 host_label)
  list(GET host_fields 3 host_postpass_intensity)
  list(GET host_fields 4 host_vignette_strength)
  list(GET host_fields 5 host_grain_shape)

  foreach(line_index RANGE 1 5)
    # existing per-tier body, with the output path gaining the host segment:
    #   ${truth_output_dir}/${host_id}/${tier_id}/ROOT/enbseries/truth-quality.ini
    # and the ini gaining these three keys after the tier keys:
    set(host_key_block
      "TruthPostpassIntensity=${host_postpass_intensity}\n"
      "TruthPostpassVignetteStrength=${host_vignette_strength}\n"
      "TruthPostpassGrainShape=${host_grain_shape}\n")
  endforeach()
endforeach()
```

Update the generated header comment in `cmake/CheckTruthQualityPresets.cmake:205` so it names both manifests:

```cmake
"; Generated from config/quality-tiers.csv and config/hosts.csv\n; Product=Truth ENB\n; Host=${host_label}\n; Tier=${tier_name}\n"
```

- [ ] **Step 5: Run the gate to verify it passes**

```bash
cmake --preset vs2026-x64
```

Expected: configure succeeds. Ten preset directories exist, two hosts by five tiers.

- [ ] **Step 6: Verify the ENBSeries variant did not move**

```bash
ctest --preset vs2026-x64-debug -R truth_post_finish_warp --output-on-failure
```

Expected: PASS at the Task 1 baseline hash. The ENBSeries host row carries `0.18`, so its render is unchanged.

- [ ] **Step 7: Commit**

```bash
git add config/hosts.csv cmake/GenerateTruthQualityPresets.cmake cmake/CheckTruthQualityPresets.cmake
git commit -m "feat: generate presets across a host axis"
```

---

### Task 3: Extend the release package gate

**Files:**
- Modify: `cmake/CheckTruthReleasePackage.cmake:73-121`

**Interfaces:**
- Consumes: the `Presets/<host>/<tier>/ROOT/enbseries/*.ini` layout from Task 2.
- Produces: nothing later tasks consume.

- [ ] **Step 1: Write the failing assertion**

At `cmake/CheckTruthReleasePackage.cmake:84`, replace the single-axis expected-file construction:

```cmake
foreach(host IN LISTS expected_hosts)
  foreach(tier IN LISTS expected_tiers)
    foreach(preset IN LISTS expected_presets)
      list(APPEND expected_files
        "Presets/${host}/${tier}/ROOT/enbseries/${preset}")
    endforeach()
  endforeach()
endforeach()
```

And at line 119, widen the classification regex:

```cmake
elseif(relative MATCHES "^Presets/([^/]+)/([^/]+)/ROOT/enbseries/(.+\\.ini)$")
  set(source_candidate
    "${TRUTH_PRESET_ROOT}/${CMAKE_MATCH_1}/${CMAKE_MATCH_2}/ROOT/enbseries/${CMAKE_MATCH_3}")
```

- [ ] **Step 2: Run the package gate to verify it fails**

```bash
cmake --build --preset vs2026-x64-debug --target package
```

Expected: FATAL_ERROR listing the ten expected `Presets/<host>/<tier>/...` paths as absent from the archive, because the install rules still stage the single-axis tree.

- [ ] **Step 3: Update the install rules**

In `CMakeLists.txt`, replace the single-axis preset install block near line 229 with a host-aware pair of loops:

```cmake
  foreach(preset_host IN ITEMS enbseries effects11)
    foreach(preset_tier IN ITEMS performance balanced quality ultra cinematic)
      install(
        DIRECTORY "${TRUTH_PRESET_ROOT}/${preset_host}/${preset_tier}/"
        DESTINATION "Presets/${preset_host}/${preset_tier}"
        FILES_MATCHING PATTERN "*.ini")
    endforeach()
  endforeach()
```

The host and tier lists are duplicated here rather than read from the CSVs because `install()` runs at configure time against values the generator has already validated. If the manifests change, both gates in Task 2 and Task 3 fail loudly rather than silently packaging a stale set.

- [ ] **Step 4: Run the package gate to verify it passes**

```bash
cmake --build --preset vs2026-x64-debug --target package
```

Expected: the archive builds and the gate reports no unexpected and no missing files.

- [ ] **Step 5: Verify determinism**

Build the package twice and compare archive hashes. Expected: identical. The existing package-validation harness already renders `archive-first` and `archive-second` for this purpose.

- [ ] **Step 6: Commit**

```bash
git add cmake/CheckTruthReleasePackage.cmake CMakeLists.txt
git commit -m "feat: package presets across the host axis"
```

---

### Task 4: Audit Elder's real conflict surface

The spec named `sharpenStr`, `vignetteEnable`, and `grainIntensity` in `presets/eote/Helper/EotE_ThemeSystem.fxh` as Elder's conflicting stages. They are declared as `ThemeParams` fields and selected through `ui_EotE_Theme` (0 to 7), but a search found no consumer outside the theme definition itself. They may be inert. `presets/eote/Addons/Effect_ColorGrading.fxh:379-392` computes an unsharp mask that is live and is not covered by any of those names.

This task establishes what Elder actually does before changing anything.

**Files:**
- Read: `elder-enb/presets/eote/Helper/EotE_ThemeSystem.fxh`
- Read: `elder-enb/presets/eote/Addons/Effect_ColorGrading.fxh:379-392`
- Create: `elder-enb/docs/EFFECTS11-CONFLICT-AUDIT.md`

**Interfaces:**
- Consumes: nothing.
- Produces: a written finding that decides whether Elder needs a host axis at all.

- [ ] **Step 1: Establish whether the theme fields are live**

```bash
cd /c/dev/elder-enb && grep -rn "sharpenStr\|grainIntensity\|vignetteEnable\|vignetteStr" presets/eote --include=*.fx --include=*.fxh
```

Expected: hits only inside `Helper/EotE_ThemeSystem.fxh`. If that holds, the fields are declared and unread, and Elder has no theme-driven conflict.

- [ ] **Step 2: Establish whether the unsharp mask is live**

```bash
cd /c/dev/elder-enb && grep -rn "Effect_ColorGrading" presets/eote --include=*.fx --include=*.fxh
```

Read each include site and record whether the `detail = color - blur` path at `Effect_ColorGrading.fxh:392` reaches the output, and what gates it.

- [ ] **Step 3: Write the finding**

Create `elder-enb/docs/EFFECTS11-CONFLICT-AUDIT.md` recording, for each of the four candidate stages, whether it is live, what gates it, and whether Effects 11 duplicates it. State honest nulls where a stage turns out to be inert.

- [ ] **Step 4: Decide and record the consequence**

If no live stage conflicts, write that Elder needs no host axis and why. If the unsharp mask is live, add a follow-on task mirroring Task 1: give it a strength uniform defaulting to its current constant, then add the host axis.

- [ ] **Step 5: Commit**

```bash
cd /c/dev/elder-enb
git checkout -b docs/effects11-conflict-audit
git add docs/EFFECTS11-CONFLICT-AUDIT.md
git commit -m "docs: audit Elder's Effects 11 conflict surface"
```

---

### Task 5: Contribute the settings patches upstream

Effects 11 force-disables competing post effects through `SettingsPatches.json`, matched on exact variable-name strings. Truth's names are absent, so a user installing Truth under Effects 11 gets double processing. Adding the names fixes that for every user without them needing the Effects 11 preset variant.

**Files:**
- Modify (in a Community Shaders fork): `features/Effects11/Shaders/Effects11/SettingsPatches.json`

**Interfaces:**
- Consumes: the uniform name from Task 1.
- Produces: nothing this repository consumes.

- [ ] **Step 1: Fork and branch**

```bash
gh repo fork community-shaders/skyrim-community-shaders --clone --remote
cd skyrim-community-shaders && git checkout -b feat/truth-enb-settings-patches dev
```

- [ ] **Step 2: Add the entries**

In the `enbeffectpostpass.fx` object of `features/Effects11/Shaders/Effects11/SettingsPatches.json`, add to the `patches` array:

```json
{ "variable": "[Truth 70] Postpass | Vignette Strength", "value": "0.0" },
{ "variable": "[Truth 70] Postpass | Grain Shape", "value": "0.0" }
```

- [ ] **Step 3: Verify the file still parses**

```bash
python -c "import json,sys; json.load(open('features/Effects11/Shaders/Effects11/SettingsPatches.json')); print('ok')"
```

Expected: `ok`. A trailing comma is the common failure here.

- [ ] **Step 4: Commit and raise the PR**

`CONTRIBUTING.md` requires conventional commit titles, atomic commits, and no work-in-progress code. This change is data only.

```bash
git add features/Effects11/Shaders/Effects11/SettingsPatches.json
git commit -m "feat: patch Truth ENB postpass settings under Effects 11"
gh pr create --repo community-shaders/skyrim-community-shaders --base dev \
  --title "feat: patch Truth ENB postpass settings under Effects 11"
```

The PR body states which preset the names come from, that the stages duplicate Community Shaders upscaling and TAA, and links the preset's Nexus page. Their `CONTRIBUTING.md` directs contributors to Discord for discussion, so raise it there in parallel rather than waiting on the PR queue.

- [ ] **Step 5: Record the outcome**

Whatever they decide, append the result to `truth-enb/docs/DECISION-hardening-branch.md`'s sibling: create `truth-enb/docs/DECISION-effects11-upstream.md` recording the PR URL, the response, and what it implies for the W3 sky feature. An accepted data PR is evidence the sky PRs are worth writing. A rejected one is worth knowing before spending the effort.

---

### Task 6: Record the enb-runtime-core honest null

`enb-runtime-core` resolves and fingerprints an already-loaded ENB host through the official ENBSeries SDK. Under Effects 11 no such module is loaded, so the library has nothing to resolve. The spec rules out a port. This records that plainly rather than leaving a user to discover it.

**Files:**
- Modify: `enb-runtime-core/README.md`

**Interfaces:**
- Consumes: nothing.
- Produces: nothing.

- [ ] **Step 1: Add the statement**

Add a section immediately after the opening paragraph of `enb-runtime-core/README.md`:

```markdown
## Host requirement

This library requires ENBSeries. It resolves and fingerprints an ENB module that
is already loaded in the process, so it does nothing under shader frameworks that
do not present one. Community Shaders and its Effects 11 feature export only SKSE
entry points and publish no ENB API, so this library is inert there by design
rather than by defect.
```

- [ ] **Step 2: Verify the claim still holds**

```bash
curl -sSL https://raw.githubusercontent.com/community-shaders/skyrim-community-shaders/dev/src/XSEPlugin.cpp | grep -c 'extern "C" DLLEXPORT'
```

Expected: `3`, matching `SKSEPlugin_Load`, `SKSEPlugin_Version`, and `SKSEPlugin_Query`. If this returns a larger number, Community Shaders has added exports and the claim needs rechecking before it ships.

- [ ] **Step 3: Commit**

```bash
cd /c/dev/enb-runtime-core
git checkout -b docs/host-requirement
git add README.md
git commit -m "docs: state the ENBSeries host requirement plainly"
```

---

### Task 7: In-game verification under both hosts

Every gate in this plan is a render or a package check. None of them proves the preset looks correct in a real load order, and the spec's open question about whether Effects 11 supplies every input Truth's `enbeffect.fx` reads is only answerable by running it. This is the human-testing step already planned before any Nexus publish.

**Files:**
- Create: `docs/EFFECTS11-VALIDATION-LOG.md`

**Interfaces:**
- Consumes: the packaged output from Task 3.
- Produces: the evidence that gates the Nexus release.

- [ ] **Step 1: Install and capture under ENBSeries**

Install the `enbseries` variant at tier 2 alongside ENBSeries 0.504. Capture the same six scenes the reference renderer covers: day, dusk, quiet clear night, active clear night, cloudy night aurora, and storm.

- [ ] **Step 2: Install and capture under Effects 11**

Remove ENBSeries, install Community Shaders with Effects 11, install the `effects11` variant at tier 2. Capture the same six scenes from the same save and camera positions.

- [ ] **Step 3: Check for the double-processing defect**

Compare the two capture sets for sharpening halos and film grain. Expected: the Effects 11 captures show neither, because the generated ini zeroes both. If grain is visible, the ini key name does not match the shader uniform name.

- [ ] **Step 4: Check for missing inputs**

Effects 11 supplies ENB's scene, bloom, lens, depth, adaptation, time, weather, and day and interior inputs through its own implementation. Confirm each Truth stage that consumes one produces sane output rather than black, white, or NaN. Record any input that is absent or differs.

- [ ] **Step 5: Write the log**

Create `docs/EFFECTS11-VALIDATION-LOG.md` recording the game version, Community Shaders version, Effects 11 version, ENBSeries version, the six scenes, and the finding for each. Keep honest nulls: an untested configuration is recorded as untested, never as passing.

- [ ] **Step 6: Commit**

```bash
git add docs/EFFECTS11-VALIDATION-LOG.md
git commit -m "docs: log in-game validation under both shader hosts"
```

---

## Notes for the executor

Tasks 1 through 3 are strictly ordered: Task 2 writes the uniform Task 1 creates, and Task 3 packages the layout Task 2 generates. Task 4 is independent and touches `elder-enb`. Task 6 is independent and touches `enb-runtime-core`.

Task 5 is independent of all of them and is the cheapest way to learn whether upstream contribution is viable, so run it early even though it is numbered late. Its outcome gates all of W3.

Task 7 runs last and requires a human at a keyboard with the game installed. Nothing in this plan claims the presets look correct until Task 7 says so.
