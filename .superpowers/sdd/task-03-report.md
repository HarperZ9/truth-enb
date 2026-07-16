# Task 03 — Occlusion-aware interior light (Helios-class seed) TDD Report

Date: 2026-07-15

Repository: `C:\dev\truth-enb`

Branch: `chore/bootstrap-truth`

Base commit: `bb44ffe4464110429c3a3270349e59bc79150fdd`

## Source boundary

All C++ in this task is original Truth-owned work. No recovered or peer shader
or mod source was read, copied, edited, imported, or used as a derivation
source. Helios was studied only for its documented behavior; none of its code
was consulted. Production C++ uses only the standard library.

## Purpose

The design's frontier target for exterior-to-interior continuity must beat the
whole-cell ambient approximation whose documented flaw is that it "also
illuminates basements and other occluded spaces." This task lands the CPU
reference for an occlusion-aware model: an interior cell is lit by the exterior
sky only through its window/portal apertures and only when it is not occluded.
A windowless or sealed cell receives exactly zero exterior daylight and keeps
only its own ambient floor.

## Model

```text
effective_aperture = clamp( sum_i(sky_visibility_i * transmittance_i), 0, 1 )
open_factor        = effective_aperture * (1 - occlusion)
exterior_daylight  = exterior_sky_luminance * open_factor
interior_light     = clamp( ambient_floor + exterior_daylight, 0, 1e6 )
exterior_excluded  = (open_factor == 0)     # basement / sealed cell
```

`EvaluateInteriorLight` validates every field, computes a bounded candidate,
and assigns the output exactly once; any rejection preserves the caller's
output bit-for-bit, matching the other Truth vertical slices.

## CPU cycle

### RED
`tests/InteriorLightTests.cpp` written first; built against the header alone.
Link failed with the expected missing symbol:

```
InteriorLightTests.obj : error LNK2019: unresolved external symbol
  "...EvaluateInteriorLight(InteriorLightOutput&, InteriorLightInput const&)"
truth_interior_light_tests.exe : fatal error LNK1120: 1 unresolved externals
```

### GREEN
`src/render/InteriorLight.cpp` implemented to the model above. The new target
built clean under `/W4 /permissive-` and the suite passed:

```
Truth interior-light C++ cases: 10/10; assertions: 37
```

Cases: stable enum codes; open room receives clamped daylight; **basement
receives no exterior daylight**; **full occlusion seals a windowed cell**;
aperture sum clamps to unity; partial aperture and occlusion compose; interior
light clamps at the ceiling; invalid input preserves output bit-for-bit;
out-of-range occlusion rejects; too many apertures rejects.

### Regression
Full Debug suite re-run after integration: **41/41 tests passed, 0 failed**
(`truth_interior_light_cpp` added as test #2; all 40 prior cases unchanged).

## Shader mirror (delivered in the follow-up commit)

`shaders/truth/TruthInteriorLight.fxh` mirrors the CPU arithmetic (aperture sum
clamp, open factor, exact-zero basement exclusion, bounded interior light);
`TruthInteriorLightProbe.fx` forces full consumption of the model and compiles
strict through the project gate. CTest `truth_interior_light_fxc` passed
(`fx_5_0 /WX /Ges /Gis /O3`, exact pinned x64 FXC). Full suite after
integration: **42/42**.

## Not yet in this task (next)

- WARP-executed CPU/HLSL numeric parity for the interior model (the compile
  witness proves the mirror builds; parity execution follows the sky-view
  adapter's WARP pattern).
- Portal/window-anchor geometry binding and lightning propagation, which the
  full Helios-class replacement will layer on this reference.
