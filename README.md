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
- `SkyFieldInput` / `SkyFieldOutput`: a deterministic, texture-free procedural
  cloud body/detail erosion field and night-only aurora curtain reference.
- `TruthSkyFields.fxh`: the shader mirror; its generated cloud density, detail,
  and intrinsic aurora radiance feed cloud lighting before exposure, with a
  macro-off direct-control fallback.
- `CloudLightingInput` / `CloudLightingOutput`: bounded cloud optical depth,
  direct and ambient scattering, self-shadow, powder response, weather tint,
  and one explicit sky/cloud/aurora composition path.
- `TruthCloudLighting.fxh`: the original shader mirror that lights procedural
  cloud fields and carries their generated aurora color into the final image.

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
