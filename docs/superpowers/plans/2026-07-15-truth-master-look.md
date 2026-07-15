# Truth Master Look Vertical Slice Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build and verify a clean-room, dependency-free C++23 master exposure/tone-state slice with an original FXC-compilable Truth HLSL mirror.

**Architecture:** Plain public data structures cross the API boundary; free functions validate into temporaries and commit atomically. CPU and HLSL share named concepts and documented constants while remaining independently compilable.

**Tech Stack:** C++23 standard library, CMake 4.2, Visual Studio 18 2026 x64/MSVC static runtime, CTest, x64 FXC `fx_5_0`.

## Global Constraints

- Create exactly one independent repository at `C:\dev\truth-enb` on `chore/bootstrap-truth`; do not use a worktree.
- Do not read, edit, copy, move, or run Git commands in either prohibited recovered-material location.
- Use only Truth-owned source and the C++ standard library.
- Use `apply_patch` for every authored file creation or edit.
- Generated compiler and shader outputs must remain under `build/`.
- Record observed RED and GREEN commands/results in `.superpowers/sdd/task-01-report.md`.
- Do not configure a remote or push.

---

### Task 1: C++ contract and behavior

**Files:**
- Create: `CMakeLists.txt`
- Create: `CMakePresets.json`
- Create: `tests/MasterLookTests.cpp`
- Create after RED: `include/truth/render/MasterLook.hpp`
- Create after RED: `src/render/MasterLook.cpp`

**Interfaces:**
- Produces: `AtmosphereSample`, `MasterLookState`, `StateValidity`, `UpdateStatus`, `DiagnosticCode`, `UpdateResult`, `Update`, `UnifiedLuminance`, `TargetExposureEv`, and `FilmicToneCurve` in `truth::render`.
- Consumes: C++23 `<algorithm>`, `<cmath>`, `<cstdint>`, and `<limits>` only.

- [x] **Step 1: Write the failing assertion harness**

  Add test cases for explicit enum numbers, first-sample initialization,
  deterministic target exposure, asymmetric continuous adaptation,
  discontinuity snap/epoch, rejected input/state with exact snapshot equality,
  and filmic curve finiteness/monotonicity/black/highlight behavior.

- [x] **Step 2: Run the test build to verify RED**

  Run: `cmake --preset vs2026-x64 && cmake --build --preset vs2026-x64-debug`

  Expected: compilation fails because `truth/render/MasterLook.hpp` does not
  exist; this is the intended missing-behavior failure.

- [x] **Step 3: Add the minimal public contract and implementation**

  Declare the exact POD fields and fixed underlying enum values. Implement
  validation helpers, the documented unified luminance/target functions,
  validate-then-commit update logic, and the extended Reinhard reference curve.

- [x] **Step 4: Run the C++ suite to verify GREEN**

  Run: `cmake --build --preset vs2026-x64-debug && ctest --preset vs2026-x64-debug --output-on-failure --no-tests=error -R truth_master_look_cpp`

  Expected: the C++ assertion harness reports every named case passing and
  CTest reports `100% tests passed`.

### Task 2: Original Truth shader and FXC gate

**Files:**
- Create before RED: `cmake/CompileTruthShader.cmake`
- Modify before RED: `CMakeLists.txt`
- Create after RED: `shaders/truth/TruthColorCore.fxh`
- Create after RED: `shaders/enbeffect.fx`

**Interfaces:**
- Consumes: exact x64 FXC at the requested Windows Kits path.
- Produces: `build/shaders/<config>/TruthMasterLook.fxo` and an assembly listing, plus the `truth_shader_fxc` CTest.

- [x] **Step 1: Add a shader compilation test while the source is absent**

  Register a CTest that invokes `cmake -P cmake/CompileTruthShader.cmake` with
  source, include, output, listing, and exact compiler paths.

- [x] **Step 2: Run the shader test to verify RED**

  Run: `ctest --preset vs2026-x64-debug --output-on-failure --no-tests=error -R truth_shader_fxc`

  Expected: failure states that `shaders/enbeffect.fx` is absent.

- [x] **Step 3: Add the minimal original HLSL fixture**

  Define Truth sample/state structs, unified luminance, target exposure, and
  filmic curve helpers in `TruthColorCore.fxh`; define Effects 11 vertex/pixel
  entry points and one `technique11` in `enbeffect.fx`.

- [x] **Step 4: Run the exact FXC test to verify GREEN**

  Run: `ctest --preset vs2026-x64-debug --output-on-failure --no-tests=error -R truth_shader_fxc`

  Expected: exact x64 FXC exits zero and the script verifies nonempty build-tree
  output files.

### Task 3: Documentation, evidence, and repository handoff

**Files:**
- Create: `README.md`
- Create: `LICENSE`
- Create: `.gitignore`
- Create: `.gitattributes`
- Create: `docs/architecture.md`
- Create: `.superpowers/sdd/task-01-report.md`

**Interfaces:**
- Consumes: observed command output from Tasks 1 and 2.
- Produces: reproducible operator instructions and the RED/GREEN evidence receipt.

- [x] **Step 1: Document the public contract, source boundary, build commands, accepted numeric ranges, and artifact locations**

- [x] **Step 2: Record the actual RED/GREEN commands, exit states, relevant diagnostics, and final named-case counts**

- [x] **Step 3: Run fresh full verification**

  Run: `cmake --preset vs2026-x64 && cmake --build --preset vs2026-x64-debug && ctest --preset vs2026-x64-debug --output-on-failure --no-tests=error`

  Expected: configure/build exit zero and both registered CTests pass.

- [x] **Step 4: Review only owned repository state and commit**

  Run: `git diff --check`, `git status --short --branch`, secret-pattern scan,
  then `git add` the explicit owned paths and `git commit -m "feat: bootstrap Truth master look"`.

  Expected: one root commit on `chore/bootstrap-truth`, no remote, clean status.
