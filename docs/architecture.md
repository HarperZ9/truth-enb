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
`shaders/enbeffect.fx` supplies the production ENBSeries 0.504 vertex/pixel
effect. ENB-owned scene, bloom, lens, depth, adaptation, weather, time, and
day/interior inputs are named exactly at this boundary. The Truth-owned
`TRUTHPASSTHROUGH` technique is a safe scene-color fallback. Separately, the
official ENB 0.504 `PS_DrawOriginal` and reserved `ORIGINALPOSTPROCESS`
technique are retained unchanged as the required vanilla fallback and locked
by source hash; Truth rendering never substitutes its own code under that name.

CTest invokes only the exact x64 FXC selected by the project, with target
`fx_5_0`. The compile script rejects missing inputs, a nonzero compiler result,
missing outputs, or empty outputs. Its object and assembly listing are confined
to `build/shaders/<configuration>/`.

## Verification surfaces

- The C++ assertion harness tests ten named behaviors, including exact
  bit-pattern preservation for rejected NaN-containing state.
- The shader test compiles the real effect, not a mock or syntax-only surrogate.
- The D3D11 WARP ABI test executes the production row builder and sky-view
  adapter, reflects the runtime vectors and their exact offsets, and
  compares asymmetric row-major transforms against the CPU reference.
- That gate also executes the exact optimized `TruthEnbPixelMain` against five
  ENB-shaped textures. It covers defaults, manual-exposure endpoints, master
  passthrough, bloom/lens mixing, depth/interior preservation, active sky
  replacement, and non-finite runtime, scene, adaptation, and UI values.
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
looped phase, bounded wind, cloud/weather controls, aurora activity, night
factor, and camera position. `EvaluateSkyFields` commits once after producing
bounded low-frequency cloud body, independent detail erosion, composed density,
curtain mask, and intrinsic aurora radiance. Invalid input preserves the
caller's output.

Phase `1.0` is canonically evaluated as phase `0.0`; the sinusoidal wind orbit
is therefore exactly loopable. Coverage, density, and weather only scale or
raise cloud occupancy, so each control is monotonic for a fixed field sample.
Daylight (`night_factor == 0`) writes exact positive zero for both aurora mask
and intrinsic radiance.

`TruthSkyFields.fxh` mirrors the texture-free field. In the enabled effect,
`TruthSkyViewAdapter.fxh` reconstructs a world-space direction with the
row-major inverse view-projection matrix, rebases the raw engine camera around
an explicit aurora origin, and converts engine units to artist units before the
field is evaluated. The resulting cloud density, detail erosion, and intrinsic
aurora radiance feed cloud lighting before exposure. The canonical
`TRUTH_QUALITY_TIER` contract keeps tiers `0/1` on the analytic path and enables
bounded volume clouds for tiers `2/3/4`; invalid runtime state preserves the
authored scene.

## ENB runtime bridge

The world-space path is an explicit fail-closed protocol rather than an
implicit shader assumption:

```text
Address Library database -> world-root camera relocation
                         -> inverse view-projection + camera
                         -> ENB callback parameter writes
                         -> seven hidden float4 values
                         -> readiness gate -> sky-view adapter
```

Protocol `1.0` carries four row vectors, one camera vector, and one status
vector. Protocol `1.1` adds a normalized celestial vector before status while
remaining compatible with 1.0 camera payloads. Status contains protocol
version, valid flag, generation, and engine-world-units per aurora unit.
Default status is zero, so a missing bridge cannot accidentally enable the
replacement. The effect checks version, validity, scale, camera, celestial,
matrix arithmetic, homogeneous division, direction length, and rebased camera
bounds before it replaces a sky pixel. Interior pixels and non-sky depth retain
the ordinary ENB color path.

The runtime source reads Address Library directly and does not use SKSE or
CommonLib. ENB SDK calls are made only from ENB callbacks. Shader state is
restored around ENB load/save transitions so serialized presets do not retain a
transient runtime camera.

## Procedural aurora curtain

The aurora is an emissive volume integral rather than a screen-space band. Its
source term is factored into a normalized height-dependent deposition profile
and a horizontal electron-flux field. Green and blue share a lower peak; red is
higher, broader, and driven by a lower-frequency persistent flux. The physical
ordering follows the Lawlor–Genetti rendering formulation while all distances
remain explicit Skyrim-scale artist coordinates.

Each view ray intersects the bounded auroral height slab. A three-point coarse
pass rejects rays outside the sheet before integration. Surviving rays evaluate
three two-dimensional noise fields once to establish a shared field context;
fixed samples then use analytic curved-sheet and filament terms, compact-support
deposition windows, and bounded accumulation. View-angle path length is capped;
horizon visibility is softened; phase follows an exact sinusoidal loop; camera
translation moves the world-space sheet without altering direction-space
clouds. CPU and HLSL use fixed `1/2/4/7/10` sample budgets for Performance,
Balanced, Quality, Ultra, and Cinematic. Balanced (`2`) is the authored
default, while tests require cumulative error to decrease toward the
high-tier reference.

The complete Balanced HDR prepass is budgeted, not just the isolated helper.
FXC listing inspection rejects a build above `2,938` static instruction slots;
the IEEE-strict witness is currently `2,917`. Its report records the static
count and a labelled slot-pixel estimate at 1080p, 1440p, and 4K, not a dynamic
executed-instruction upper bound. Activity zero, daylight, below-slab rays, and
coarse-rejected empty rays return exact positive zero with zero integration
samples.

Cloud and fog attenuation remain outside the intrinsic aurora evaluator so the
existing single-composition rule applies them exactly once. The WARP witness
adds a stable direction-space star field behind that same transmittance, making
star preservation and soft cloud occlusion measurable without introducing a
runtime asset or dependency.

## Procedural cloud lighting

`EvaluateCloudLighting` consumes clear-sky radiance, the composed procedural
cloud density/detail field, precomputed aurora radiance, sun/view coordinates,
weather, night state, and fog transmittance. It validates the complete input,
calculates into a local candidate, bounds every intermediate/output, and
commits once. Rejection therefore preserves the caller's output bit-for-bit.

Cloud extinction uses bounded path length and Beer-Lambert transmittance. A
bounded forward phase and detail-aware silver-lining term feed direct sunlight;
self-shadowing controls that light through the body. Powder response raises a
bounded multiple-scattering floor. Day, night, and weather tints then color the
resulting in-scatter. Zero density is handled explicitly as optical depth `0`,
transmittance `1`, and black cloud radiance.

Composition has one energy path:

```text
attenuation = cloud_transmittance * fog_transmittance
aurora = procedural_intrinsic_aurora * attenuation
composite = sky * attenuation + cloud_radiance * fog_transmittance + aurora
```

The procedural aurora already contains its activity, curtain, night, and hue
modulation; cloud lighting does not reconstruct or rescale that color. It only
forces exact black when daylight is complete or the declared night factor is
zero, then applies cloud/fog attenuation once. `TruthCloudLighting.fxh` mirrors
the CPU formulas. The prepass routes
`SkyFieldOutput::aurora_intrinsic_radiance` directly into it. FXC compiles the
nine-stage suite at all five canonical quality tiers as `fx_5_0`.
