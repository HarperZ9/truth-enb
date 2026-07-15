# Task 01 — Truth Master Look TDD Report

Date: 2026-07-15

Repository: `C:\dev\truth-enb`

Branch: `chore/bootstrap-truth`

## Source boundary

This task created an independent repository and original Truth-only C++/HLSL.
No recovered or peer shader material was read, edited, copied, moved, imported,
or used as a Git working directory. The implementation uses the C++ standard
library only and has no source dependency outside this repository.

## Toolchain witness

- CMake: `4.2.0`
- Generator: `Visual Studio 18 2026`
- Platform: `x64`
- Compiler: MSVC `19.50.35721.0`
- Runtime evidence in generated project: `MultiThreadedDebug` for Debug and
  `MultiThreaded` for non-Debug configurations
- Shader compiler:
  `C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\fxc.exe`
- Shader target: `fx_5_0`

## Cycle 1 — C++ master-look behavior

### RED

Authored `tests/MasterLookTests.cpp` and the initial CMake test target before
creating the public header or implementation.

Command:

```powershell
cmake --preset vs2026-x64
```

Observed: exit `0`; CMake selected Windows SDK `10.0.26100.0`, detected MSVC
`19.50.35721.0`, and generated `C:\dev\truth-enb\build`.

Command:

```powershell
cmake --build --preset vs2026-x64-debug
```

Observed: exit `1`, with the intended missing-behavior diagnostic:

```text
MasterLookTests.cpp(1,10): error C1083: Cannot open include file:
'truth/render/MasterLook.hpp': No such file or directory
```

This failure proved that the assertion harness depended on the absent Truth
API rather than passing against scaffolding or a mock.

### GREEN

Added `include/truth/render/MasterLook.hpp` and
`src/render/MasterLook.cpp`, then linked the real library into the same test
executable.

Commands:

```powershell
cmake --build --preset vs2026-x64-debug
ctest --preset vs2026-x64-debug --output-on-failure --no-tests=error -R truth_master_look_cpp
.\build\Debug\truth_master_look_tests.exe
```

Observed: build exit `0`; CTest `1/1` passed; the real assertion harness
reported:

```text
[PASS] stable codes are explicit
[PASS] first sample initializes deterministically
[PASS] brightening uses its own bound
[PASS] darkening uses its own bound
[PASS] discontinuity snaps and advances epoch
[PASS] invalid samples never mutate state
[PASS] invalid state never mutates
[PASS] epoch overflow rejects without mutation
[PASS] filmic curve is finite monotonic and compresses highlights
Truth master-look C++ cases: 9/9; assertions: 12082
```

### Review regression — discontinuity on first accepted sample

The requirements review identified that the first accepted sample followed the
initialization branch before honoring `discontinuity`. A regression case was
added before changing production code.

RED command:

```powershell
ctest --preset vs2026-x64-debug --output-on-failure --no-tests=error -R truth_master_look_cpp
```

Observed: exit `1` with the intended behavioral failure:

```text
[FAIL] discontinuous first sample also advances epoch:
first-sample discontinuity did not report a snap
```

The minimal implementation change made the invalid-history branch honor the
same discontinuity snap/epoch rule. The same command then exited `0`, CTest
reported `1/1`, and the assertion executable reported:

```text
[PASS] discontinuous first sample also advances epoch
Truth master-look C++ cases: 10/10; assertions: 12087
```

## Cycle 2 — exact FXC effect compilation

### RED

Authored `cmake/CompileTruthShader.cmake` and registered the shader CTest before
creating either shader source file.

Command:

```powershell
ctest --preset vs2026-x64-debug --output-on-failure --no-tests=error -R truth_shader_fxc
```

Observed: exit `1`, CTest `0/1`, with the intended failure:

```text
Truth effect source is absent: C:/dev/truth-enb/shaders/enbeffect.fx
```

The script had already verified that the exact requested x64 FXC executable
existed, so this was a source-behavior RED rather than an environment failure.

### GREEN

Added the original Truth shader include and standalone Effects 11 fixture, then
reran the same CTest.

Command:

```powershell
ctest --preset vs2026-x64-debug -V --no-tests=error -R truth_shader_fxc
```

Observed: exit `0`, CTest `1/1`, with these compiler arguments and outputs:

```text
TRUTH_FXC=C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/fxc.exe
TRUTH_SHADER=C:/dev/truth-enb/shaders/enbeffect.fx
TRUTH_OUTPUT=C:/dev/truth-enb/build/shaders/Debug/TruthMasterLook.fxo
TRUTH_LISTING=C:/dev/truth-enb/build/shaders/Debug/TruthMasterLook.asm
FXC target: fx_5_0
```

Artifact witness after the passing run:

| Output | Bytes |
|---|---:|
| `build/shaders/Debug/TruthMasterLook.fxo` | 3,567 |
| `build/shaders/Debug/TruthMasterLook.asm` | 6,773 |

The FXO SHA-256 for this local verification run was
`FA366B267B175A0F496C444C690D298AD3499427098FF5A57B726F75ECC76A9C`.
Generated shader artifacts are ignored and remain beneath `build/`.

## Fresh full gate

Commands:

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug --clean-first
ctest --preset vs2026-x64-debug --output-on-failure --no-tests=error
```

Observed: configure exit `0`, clean build exit `0`, CTest exit `0`:

```text
1/2 Test #1: truth_master_look_cpp ............ Passed
2/2 Test #2: truth_shader_fxc ................. Passed
100% tests passed, 0 tests failed out of 2
```

Final automated count: 10 C++ named cases / 12,087 assertions, plus 1 real FXC
compile test; 2/2 CTests pass.
