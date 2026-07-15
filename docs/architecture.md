# Master Look Architecture

## Boundary

The vertical slice has one mutable boundary:

```text
AtmosphereSample + MasterLookState
                 |
                 v
       validate all fields
                 |
                 v
   unified luminance -> target EV
                 |
                 v
 initialize | adapt | discontinuity snap
                 |
                 v
      one candidate-state commit
```

The update function never writes through `state` while validating or
calculating. It copies a fully validated state into a local candidate, performs
the transition there, checks the computed exposure, and assigns the candidate
once. Every rejection path returns before that assignment.

## Public state and outcomes

`AtmosphereSample` is the complete frame input. `MasterLookState` owns all
history and can be serialized without hidden runtime objects. An invalid state
with finite in-range numeric fields means there is no exposure history yet; the
first accepted continuous sample initializes it without advancing the epoch.
A first-sample discontinuity still advances the epoch and reports `snapped`.

`UpdateStatus` is the broad transition result:

| Numeric value | Name | Meaning |
|---:|---|---|
| 0 | `updated` | Continuous history adapted toward target. |
| 1 | `initialized` | No valid history existed; current and target snapped together. |
| 2 | `snapped` | A discontinuity reset current exposure and advanced the epoch. |
| 3 | `rejected` | Validation/calculation failed; state is unchanged. |

`DiagnosticCode` uses explicit numeric assignments. `0` means no diagnostic;
the `100` series covers atmosphere fields, the `140` and `150` series cover
state EV fields, `160` covers validity, `170` prevents epoch overflow, and
`180` guards an unexpected non-finite calculation.

## Meter and adaptation

Exterior metering is `0.75 * scene + 0.25 * sky`. Interior factor linearly
moves that value toward scene luminance alone. The target EV is:

```text
clamp(log2(0.18 / max(unified_luminance, 0.0001)), -16, 16)
```

Continuous adaptation clamps the EV difference to `+3.0 EV/s` when brightening
and `-1.5 EV/s` when darkening. A discontinuity bypasses those rates, snaps
current exposure to target, and increments the epoch only after overflow has
been ruled out.

## Filmic reference

For nonnegative input below the declared linear white point of `4.0`, the CPU
and HLSL use:

```text
x * (1 + x / 16) / (1 + x)
```

Values at or above the declared white point map to `1.0`; nonpositive and NaN
inputs map to black, and positive infinity maps to white. Thus the reference
always returns a finite display value, maps black exactly to black, is monotonic
on its numeric domain, and never reaches white for a finite value below `4.0`.

## Shader mirror and compilation witness

`shaders/truth/TruthColorCore.fxh` mirrors `TruthAtmosphereSample`,
`TruthMasterLookState`, unified luminance, target EV, bounded adaptation,
exposure application, and the filmic curve using original Truth names.
`shaders/enbeffect.fx` supplies a standalone Effects 11 vertex/pixel fixture.

CTest invokes only the exact x64 FXC selected by the project, with target
`fx_5_0`. The compile script rejects missing inputs, a nonzero compiler result,
missing outputs, or empty outputs. Its object and assembly listing are confined
to `build/shaders/<configuration>/`.

## Verification surfaces

- The C++ assertion harness tests ten named behaviors, including exact
  bit-pattern preservation for rejected NaN-containing state.
- The shader test compiles the real effect, not a mock or syntax-only surrogate.
- CMake presets pin the Visual Studio generator, x64 platform, C++23 mode, and
  static MSVC runtime selection.
- `.superpowers/sdd/task-01-report.md` records the observed RED/GREEN sequence.

## Unified atmosphere foundation

`AtmosphereInput` is a stateless ten-field POD: three bounded directional/day
coordinates and seven `[0,1]` weather/cloud/fog/aurora controls.
`EvaluateAtmosphere` validates every field before calculating a candidate
`AtmosphereOutput`; it assigns that candidate once, so invalid input preserves
the caller's output bit-for-bit.

The reference combines a bounded symmetric Rayleigh phase, a clamped
forward-asymmetric Henyey–Greenstein-style Mie phase, conservative horizon air
mass, Beer–Lambert cloud/fog transmittance, and pre-exposed aurora. All
below-horizon view cosines use the same `0.05` horizon fallback.

Coupling is explicit and single-pass:

```text
attenuation = cloud_transmittance * fog_transmittance
aurora_radiance = intrinsic_aurora * attenuation
composite_radiance = sky_radiance * attenuation + aurora_radiance
```

`TruthAtmosphereCore.fxh` mirrors those constants and formulas. The enabled
effect path adds the composite to sampled scene-linear color before exposure;
the disabled macro retains the original scene path. CTest compiles both with
exact x64 FXC `fx_5_0` and asserts their generated objects differ.

`.superpowers/sdd/task-02-report.md` records the CPU and FXC RED/GREEN evidence.

## Procedural sky fields

`SkyFieldInput` is a validated POD containing a normalized view direction,
looped phase, bounded wind, cloud/weather controls, aurora activity, and night
factor. `EvaluateSkyFields` commits once after producing bounded low-frequency
cloud body, independent detail erosion, composed density, curtain mask, and
intrinsic aurora radiance. Invalid input preserves the caller's output.

Phase `1.0` is canonically evaluated as phase `0.0`; the sinusoidal wind orbit
is therefore exactly loopable. Coverage, density, and weather only scale or
raise cloud occupancy, so each control is monotonic for a fixed field sample.
Daylight (`night_factor == 0`) writes exact positive zero for both aurora mask
and intrinsic radiance.

`TruthSkyFields.fxh` mirrors the texture-free field. In the enabled effect it
derives a normalized view ray from screen position, supplies generated cloud
density and aurora mask to `TruthEvaluateAtmosphere`, and adds the resulting
composite before exposure. `TRUTH_ENABLE_PROCEDURAL_SKY=0` retains the direct
cloud-density and aurora-mask inputs as an explicit fallback.
