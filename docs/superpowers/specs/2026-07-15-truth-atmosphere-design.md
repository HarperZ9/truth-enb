# Truth Unified Atmosphere Design

## Scope and ownership

This slice adds an original Truth-only analytic atmosphere shared by the CPU
reference and Truth HLSL. It does not import, inspect, or derive from recovered
or peer source. C++ remains dependency-free and uses only the standard library.

## API boundary

`AtmosphereInput` is a finite POD with ten explicit controls:

- `view_zenith_cosine`, `view_sun_cosine`, and `sun_elevation`: `[-1, 1]`;
- `weather_density`, `cloud_coverage`, `cloud_density`, `fog_density`,
  `aurora_activity`, `aurora_mask`, and `night_factor`: `[0, 1]`.

`EvaluateAtmosphere(const AtmosphereInput&, AtmosphereOutput&)` returns an
`AtmosphereEvaluation` containing explicitly numbered status and diagnostic
enums. It validates every input and computes into a local candidate. Rejection
returns before assigning the candidate, so the caller's output is bit-for-bit
unchanged.

`AtmosphereOutput` contains RGB `sky_radiance`, scalar cloud and fog
transmittance, RGB `aurora_radiance`, and RGB `composite_radiance`. Every
radiance channel is finite and nonnegative; transmittance is in `[0, 1]`.

## Analytic model

For scattering cosine `mu`:

```text
rayleigh = 0.75 * (1 + mu * mu)
g = 0.65
denominator = max(1 + g*g - 2*g*mu, 0.05)
mie = min((1 - g*g) / (denominator * sqrt(denominator)), 12)
```

Rayleigh is bounded and symmetric. The clamped Henyey–Greenstein-style Mie
term is finite at sun alignment and forward-asymmetric.

View path uses `max(view_zenith_cosine, 0.05)`. The resulting air mass is at
most `20`; every below-horizon sample uses that same conservative horizon
fallback rather than mirroring with `abs(view_zenith_cosine)`.

Daylight is `saturate((sun_elevation + 0.1) / 0.2)`. Weather attenuation is
`1 - 0.55 * weather_density`. RGB sky coefficients are:

```text
R = daylight * (0.18 * rayleigh + 0.035 * mie) * horizon_boost + 0.00225 * night_factor
G = daylight * (0.28 * rayleigh + 0.025 * mie) * horizon_boost + 0.00525 * night_factor
B = daylight * (0.52 * rayleigh + 0.015 * mie) * horizon_boost + 0.01350 * night_factor
```

The three channels are multiplied by weather attenuation.

Cloud optical depth is
`4 * coverage * density * (0.35 + 0.65 * weather_density)`. Fog optical depth
is `3 * fog_density * (1 + 0.15 * (air_mass - 1))`. Their transmittances are
`exp(-optical_depth)`, making both monotonic in their density controls.

Intrinsic aurora strength is
`activity * mask * night_factor * (0.35 + 0.65 * saturate(view_zenith_cosine))`
with RGB coefficients `(0.10, 0.80, 0.55)`. It is exactly zero when
`night_factor` is zero and monotonic in activity at night.

## Coupling invariant

Cloud/fog attenuation is applied exactly once:

```text
attenuation = cloud_transmittance * fog_transmittance
aurora_radiance = intrinsic_aurora * attenuation
composite_radiance = sky_radiance * attenuation + aurora_radiance
```

Tests assert this equality per channel so future work cannot double-attenuate
aurora.

## Shader integration

`TruthAtmosphereCore.fxh` mirrors the CPU constants, structs, and formulas.
`enbeffect.fx` includes the atmosphere core and adds the live composite to scene
linear color before exposure/tone mapping when `TRUTH_ENABLE_ATMOSPHERE=1`.
When the macro is `0`, the original master-look path remains.

CTest compiles both permutations with the exact x64 FXC `fx_5_0` compiler into
distinct build-only objects/listings and verifies that the objects differ.

## Verification

The CPU harness covers a dense bounded grid, horizon fallback/continuity,
Rayleigh symmetry and Mie asymmetry, monotonic cloud/fog response, day/night
aurora rules, single attenuation coupling, invalid-input no-mutation, edge
directions, and fixed golden samples. New MSVC targets use `/WX` once clean.
