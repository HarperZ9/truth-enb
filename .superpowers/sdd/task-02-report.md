# Task 02 — Truth Unified Atmosphere TDD Report

Date: 2026-07-15

Repository: `C:\dev\truth-enb`

Branch: `chore/bootstrap-truth`

Base commit: `c81307688861f87fb966067e6d869d88576a35f0`

## Source boundary

All C++ and HLSL in this task is original Truth-owned work. No recovered or
peer shader source was read, copied, edited, moved, imported, or used as a
derivation source. Production C++ uses only the standard library.

## CPU cycle

### RED

`tests/AtmosphereTests.cpp` and its CMake test target were created before the
public header or implementation.

Commands:

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug
```

Observed: configure exit `0`; build exit `1` with the intended missing API:

```text
AtmosphereTests.cpp(1,10): error C1083: Cannot open include file:
'truth/render/Atmosphere.hpp': No such file or directory
```

The existing `truth_master_look` library and test executable still built in
that RED run.

### GREEN

Added `Atmosphere.hpp`, `Atmosphere.cpp`, the real static library link, and
`/W4 /WX /permissive- /EHsc /utf-8` on both new MSVC targets.

Commands:

```powershell
cmake --build --preset vs2026-x64-debug
ctest --preset vs2026-x64-debug --output-on-failure --no-tests=error -R truth_atmosphere_cpp
.\build\Debug\truth_atmosphere_tests.exe
```

Observed: build exit `0` with no warnings; CTest `1/1` passed; the assertion
harness reported 12/12 cases and 408,859 assertions:

```text
[PASS] stable codes are explicit
[PASS] dense grid stays finite and bounded
[PASS] horizon fallback is conservative and continuous
[PASS] phase functions have expected symmetry and asymmetry
[PASS] cloud opacity is monotonic
[PASS] fog transmittance is monotonic
[PASS] day aurora is exactly zero
[PASS] night aurora increases with activity
[PASS] aurora is attenuated by cloud and fog
[PASS] composite uses single attenuation
[PASS] invalid inputs never mutate output
[PASS] CPU golden samples stay stable
Truth atmosphere C++ cases: 12/12; assertions: 408859
```

The dense grid evaluates 15,129 bounded atmosphere inputs and checks finite,
nonnegative radiance, transmittance bounds, and the exact single-attenuation
composite equation.

## Shader cycle

### RED

The compile script and CMake registered both macro permutations before
`TruthAtmosphereCore.fxh` existed.

Command:

```powershell
ctest --preset vs2026-x64-debug --output-on-failure --no-tests=error -R "truth_shader_fxc_atmosphere_(enabled|disabled)"
```

Observed: exit `1`; both tests failed for the intended missing source:

```text
Required Truth shader source is absent:
C:/dev/truth-enb/shaders/truth/TruthAtmosphereCore.fxh
```

### GREEN

Added the mirrored atmosphere include and connected enabled composite radiance
to live pixel output before exposure.

Command:

```powershell
ctest --preset vs2026-x64-debug -V --no-tests=error -R "truth_shader_fxc_atmosphere_(enabled|disabled)"
ctest --preset vs2026-x64-debug --output-on-failure --no-tests=error -R truth_shader_permutations_distinct
```

Observed: both exact x64 FXC `fx_5_0` compiles passed and the distinctness test
passed.

| Permutation | Define | FXO bytes | ASM bytes | FXO SHA-256 |
|---|---|---:|---:|---|
| Enabled | `TRUTH_ENABLE_ATMOSPHERE=1` | 6,205 | 11,097 | `9460E92F1C3952A5C85B5A3CBA6CC0C7F4A49F69966DAC46E1455855DF8669DE` |
| Disabled | `TRUTH_ENABLE_ATMOSPHERE=0` | 4,133 | 7,590 | `4D8A50716423A59AEA6ADE91FE59851130AD058A7E3EAC47303863476DF1C0B7` |

Compiler:
`C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\fxc.exe`

All four generated shader artifacts remain under `build/shaders/Debug/`.

## Fresh full gate

Commands:

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug --clean-first
ctest --preset vs2026-x64-debug --output-on-failure --no-tests=error
```

Observed: configure exit `0`, clean `/WX` build exit `0`, and all `5/5` CTests
passed: two C++ executables, two real FXC permutations, and the shader object
distinctness check.
