# Truth Unified Atmosphere Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a validated dependency-free C++23 analytic atmosphere and an exactly mirrored, live Truth HLSL path with two FXC macro permutations.

**Architecture:** A stateless POD/free-function CPU boundary validates then commits a complete candidate output. Truth HLSL mirrors the same constants and formulas; the standalone effect adds atmosphere before exposure only in the enabled permutation.

**Tech Stack:** C++23 standard library, CMake 4.2, Visual Studio 18 2026 x64/MSVC static runtime, CTest, exact x64 FXC `fx_5_0`.

## Global Constraints

- Work only in `C:\dev\truth-enb` at base commit `c81307688861f87fb966067e6d869d88576a35f0`.
- Do not read, copy, edit, move, or derive from recovered or peer source.
- Use `apply_patch` for all authored edits.
- Use strict test-first RED/GREEN evidence and record it in `.superpowers/sdd/task-02-report.md`.
- Keep all generated objects, listings, and binaries beneath `build/`.
- Make exactly one new commit, `feat: add Truth atmosphere foundation`; do not amend or push.

---

### Task 1: CPU atmosphere contract and reference

**Files:**
- Create before implementation: `tests/AtmosphereTests.cpp`
- Modify before implementation: `CMakeLists.txt`
- Create after RED: `include/truth/render/Atmosphere.hpp`
- Create after RED: `src/render/Atmosphere.cpp`

**Interfaces:**
- Produces: `RgbRadiance`, `AtmosphereInput`, `AtmosphereOutput`,
  `AtmosphereStatus`, `AtmosphereDiagnostic`, `AtmosphereEvaluation`,
  `RayleighPhase`, `MiePhase`, and `EvaluateAtmosphere` in `truth::render`.
- Consumes: `<algorithm>`, `<cmath>`, and `<cstdint>` only in production.

- [ ] **Step 1: Add the failing real assertion harness**

  Write named tests for fixed status values; dense grid finite/nonnegative and
  transmittance bounds; sun/horizon/anti-solar edges; conservative
  below-horizon fallback and clamp continuity; symmetric Rayleigh and
  forward-asymmetric Mie; monotonic cloud/fog; exact-zero day aurora;
  night-activity monotonicity; cloud/fog attenuation; the exact composite
  equality; invalid-field no-mutation; and two fixed CPU golden samples.

- [ ] **Step 2: Verify CPU RED**

  Run `cmake --preset vs2026-x64`, then
  `cmake --build --preset vs2026-x64-debug`.

  Expected: MSVC C1083 for absent `truth/render/Atmosphere.hpp` while the
  existing master-look target remains valid.

- [ ] **Step 3: Implement the minimum validated CPU reference**

  Add explicit fixed enum values, the exact formulas in the design, ordered
  validation, candidate-only calculation, finite/range output checks, and one
  final assignment. Apply `/W4 /WX /permissive- /EHsc /utf-8` to both new
  MSVC targets.

- [ ] **Step 4: Verify CPU GREEN**

  Run `cmake --build --preset vs2026-x64-debug`, then
  `ctest --preset vs2026-x64-debug --output-on-failure --no-tests=error -R truth_atmosphere_cpp`,
  then run `build/Debug/truth_atmosphere_tests.exe` for named counts.

  Expected: all new cases pass with no compiler warnings.

### Task 2: Truth HLSL mirror and macro permutations

**Files:**
- Modify before shader implementation: `CMakeLists.txt`
- Modify before shader implementation: `cmake/CompileTruthShader.cmake`
- Create after RED: `shaders/truth/TruthAtmosphereCore.fxh`
- Modify after RED: `shaders/enbeffect.fx`

**Interfaces:**
- Consumes: `TRUTH_ENABLE_ATMOSPHERE=1` or `=0` and the exact x64 FXC path.
- Produces: distinct on/off `.fxo` and `.asm` receipts beneath
  `build/shaders/<configuration>/`.

- [ ] **Step 1: Register two compile tests that require the absent atmosphere include**

  Pass a required-source path and one macro definition to the compile script;
  register enabled/disabled CTests plus a dependent object-distinctness test.

- [ ] **Step 2: Verify shader RED**

  Run `cmake --preset vs2026-x64`, then
  `ctest --preset vs2026-x64-debug --output-on-failure --no-tests=error -R "truth_shader_fxc_atmosphere_(enabled|disabled)"`.

  Expected: both compile tests fail because `TruthAtmosphereCore.fxh` is absent.

- [ ] **Step 3: Implement the mirrored HLSL and live effect path**

  Mirror every design constant/formula in `TruthAtmosphereCore.fxh`; add a b1
  parameter block; when enabled, evaluate and add `composite_radiance` to
  sampled scene color before `TruthApplyExposure`; retain the disabled path.

- [ ] **Step 4: Verify shader GREEN and distinct objects**

  Run the two FXC CTests verbosely and run the distinctness CTest.

  Expected: both exact `fx_5_0` compiles pass, objects/listings are nonempty,
  hashes differ, and the dependent distinctness test passes.

### Task 3: Evidence, documentation, and commit

**Files:**
- Create: `.superpowers/sdd/task-02-report.md`
- Modify: `README.md`
- Modify: `docs/architecture.md`

**Interfaces:**
- Consumes: observed RED/GREEN output, C++ counts, and FXC artifact hashes.
- Produces: a reproducible task receipt and updated operator documentation.

- [ ] **Step 1: Record exact RED/GREEN commands and observed diagnostics**

- [ ] **Step 2: Document the input/output contract, coupling equality, macro permutations, and artifact locations**

- [ ] **Step 3: Run the fresh full gate**

  Run `cmake --preset vs2026-x64`,
  `cmake --build --preset vs2026-x64-debug --clean-first`, and
  `ctest --preset vs2026-x64-debug --output-on-failure --no-tests=error`.

  Expected: configure/build exit zero with `/WX` clean and every CTest passes.

- [ ] **Step 4: Inspect and commit only owned changes**

  Run secret scan, `git diff --check`, `git status --short --branch`, inspect the
  staged diff, then commit once with `feat: add Truth atmosphere foundation`.
  Verify clean status and no push.
