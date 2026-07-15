# Truth ENB

Truth ENB currently contains its first Truth-owned vertical slice: a
dependency-free C++23 master exposure/tone state and an original HLSL effect
that mirrors the same named concepts.

This repository was authored as a clean implementation. It does not import or
depend on recovered or peer shader source.

## What is included

- `AtmosphereSample`: scene luminance, sky luminance, interior factor, frame
  delta, and discontinuity signal.
- `MasterLookState`: current/target exposure EV, history epoch, and validity.
- `Update`: validate-then-commit initialization, bounded adaptation, and
  discontinuity snapping with stable status and diagnostic codes.
- `FilmicToneCurve`: a finite, monotonic CPU reference curve that maps black to
  black and reaches display white only at the declared linear white point.
- `TruthColorCore.fxh`: original shader-side exposure and filmic helpers.
- `enbeffect.fx`: a standalone Effects 11 fixture compiled as `fx_5_0`.
- `AtmosphereInput` / `AtmosphereOutput`: a validated analytic sky, cloud,
  fog, and aurora reference with single-pass cloud/fog coupling.
- `TruthAtmosphereCore.fxh`: the original shader mirror, compiled in live
  atmosphere-enabled and atmosphere-disabled permutations.
- `SkyFieldInput` / `SkyFieldOutput`: a deterministic, seamless 3D
  direction-space cloud field with domain-warped body/detail erosion and a
  night-only world-space aurora-curtain reference.
- `TruthSkyFields.fxh`: the shader mirror; its generated cloud density, detail,
  and intrinsic aurora radiance feed cloud lighting before exposure, with a
  macro-off direct-control fallback.
- `AuroraCurtainInput` / `AuroraCurtainOutput`: an original bounded emission
  integral that factors a normalized height-dependent energy-deposition
  profile from a horizontally varying electron-flux curtain. The lower
  green/blue band and higher, broader red band are deliberately expressed in
  Skyrim-scale coordinates rather than asserted as literal kilometers.
- `TruthAuroraCurtain.fxh`: the CPU-mirrored world-space shader with broad
  warped arcs, fine ray structure, exact looped motion, camera parallax, a
  bounded view-path gain, and fixed `1/4/7/10` fallback/quality sample budgets.
- `CloudLightingInput` / `CloudLightingOutput`: bounded cloud optical depth,
  direct and ambient scattering, self-shadow, powder response, weather tint,
  and one explicit sky/cloud/aurora composition path.
- `TruthCloudLighting.fxh`: the original shader mirror that lights procedural
  cloud fields and carries their generated aurora color into the fallback
  image path.
- `CloudVolumeInput` / `CloudVolumeOutput`: an analytic height-slab raymarch
  with world-space 3D fBm, low-frequency weather/type fields, cellular
  erosion, stratus/cumulus/anvil profiles, Beer-Lambert extinction, bounded
  Henyey-Greenstein phase, sun self-shadowing, powder/silver response, and
  weather/night scattering tints.
- `TruthCloudVolume.fxh`: the CPU-mirrored `ps_5_0` implementation with
  deterministic interleaved jitter, early transmittance termination,
  distance-aware night detail LOD, and fixed performance budgets.

## Toolchain

- CMake 3.28 or newer (verified with 4.2.0)
- Visual Studio 18 2026, x64
- C++23 with the static MSVC runtime (`/MT` or `/MTd` by configuration)
- x64 FXC at:
  `C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\fxc.exe`

## Build and test

From the repository root:

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug
ctest --preset vs2026-x64-debug --output-on-failure --no-tests=error
```

The C++ assertion executable and the FXC object/listing are generated only
beneath `build/`. The shader CTest fails if the exact compiler is missing, the
effect does not compile as `fx_5_0`, or either expected output is empty.

After building, generate the four deterministic Direct3D 11 WARP reference
captures with one command:

```powershell
.\build\Debug\truth_reference_renderer.exe .\shaders\truth\TruthReferenceSky.hlsl .\build\references\Debug
```

This runs the original atmosphere, true cloud volume, procedural aurora, and
tone curve as `vs_5_0` / `ps_5_0`, reads an offscreen RGBA8 target back from
WARP, and writes `quiet-clear-night`, `active-clear-night`,
`cloudy-night-aurora`, and `storm` as binary PPM files. It reports the cold
shader-compile time separately from cached per-frame render/readback time. The
WARP suite also checks panorama topology, camera-translation parallax,
darker-core/lit-edge structure, deterministic readback, bounded CPU/HLSL
parity, aurora luminance/color budgets, star preservation, and cloud
extinction. Captures, shader objects, and executables remain under ignored
build paths.

`TRUTH_CLOUD_VOLUME_QUALITY` selects fixed `primary/light` sample budgets of
`16/4`, `20/5`, or `24/6`. All three tiers are compiled in CI-style CTests.
`TRUTH_ENABLE_CLOUD_VOLUME=0` retains the bounded procedural cloud-lighting
fallback and is compiled as a separate, bytecode-distinct permutation.

`TRUTH_AURORA_QUALITY` selects fixed curtain integration budgets of `1`, `4`,
`7`, or `10` samples. The one-sample path is the explicit fallback; all four
paths compile with strict FXC warnings-as-errors and their fallback/quality
objects are asserted distinct.

The enabled effect adds unified atmosphere composite radiance to scene-linear
color before exposure. The sky and precomputed procedural aurora are attenuated
once, while fog attenuates cloud in-scatter once:

```text
attenuation = cloud_transmittance * fog_transmittance
aurora = procedural_intrinsic_aurora * attenuation
composite = sky * attenuation + cloud_radiance * fog_transmittance + aurora
```

When procedural sky is disabled, the same lighting path consumes the direct
cloud-density and aurora controls. Zero cloud density is an exact identity:
cloud optical depth is `0`, cloud transmittance is `1`, and cloud radiance is
exactly black.

Procedural sky animation uses normalized phase `[0,1]`; both endpoints map to
the same exact field state. The field has no cloud or aurora texture inputs.
The aurora follows the emissive factorization described by Lawlor and Genetti's
[primary GPU-rendering paper](https://lawlor.cs.uaf.edu/~olawlor/papers/2010/aurora/lawlor_aurora_2010.pdf),
but its code, fields, coefficients, tests, and assets are original Truth work.

## Input contract

`Update` accepts only the following finite values:

| Input | Accepted range |
|---|---:|
| Scene luminance | `0.0` to `1,000,000.0` |
| Sky luminance | `0.0` to `1,000,000.0` |
| Interior factor | `0.0` to `1.0` |
| Delta seconds | greater than `0.0` to `1.0` |
| Current/target exposure | `-16.0` to `16.0` EV |

Invalid, non-finite, or out-of-range input returns `rejected` with a stable
diagnostic and leaves the complete state unchanged. A continuous valid update
brightens at no more than `3.0 EV/s` and darkens at no more than `1.5 EV/s`.
A discontinuity snaps to target and increments `history_epoch` exactly once.

See [the architecture note](docs/architecture.md) and
[the TDD receipt](.superpowers/sdd/task-01-report.md) for the full contract and
verification evidence.

## License

No license has been selected. `LICENSE` is a placeholder notice and grants no
rights.
