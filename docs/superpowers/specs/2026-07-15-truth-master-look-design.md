# Truth Master Look Vertical Slice Design

## Scope and provenance

This repository is a clean, Truth-owned implementation. It does not import,
copy, inspect, or derive from recovered or peer shader material. The slice is
dependency-free C++23 plus original HLSL compiled by the explicitly selected
x64 FXC executable.

## Architecture

The public API uses plain data structures and free functions in
`truth::render`. `AtmosphereSample` is the complete per-frame input.
`MasterLookState` is the only persistent history. `Update` validates
the entire sample and existing state into local temporaries, computes one
unified luminance and target exposure, and commits a candidate state only after
all calculations are valid. Rejection therefore leaves every state field
bit-for-bit unchanged.

Stable, explicitly numbered status and diagnostic enums separate the broad
outcome (`updated`, `initialized`, `snapped`, `rejected`) from the precise
reason. The accepted numeric domains are part of the public contract:

- luminance: finite, `0.0` through `1,000,000.0` inclusive;
- interior factor: finite, `0.0` through `1.0` inclusive;
- delta time: finite, greater than `0.0` through `1.0` second inclusive;
- exposure state: finite, `-16.0` through `16.0` EV inclusive;
- validity: exactly `invalid` or `valid`;
- a discontinuity is rejected if incrementing `history_epoch` would overflow.

An invalid-but-well-formed state is initialization history, not an error. Its
numeric fields must still be finite and in range. The first accepted sample
snaps both exposure values to the target and marks the state valid. It leaves
the epoch unchanged for a continuous sample; a discontinuity still increments
the epoch and reports `snapped`.

## Exposure transform

Exterior luminance is `0.75 * scene + 0.25 * sky`. Interior factor linearly
selects between that exterior value and scene luminance alone. The target is
`clamp(log2(0.18 / max(unified, 0.0001)), -16, 16)`.

Continuous history moves toward the target by at most `3.0 EV/s` when exposure
must brighten and `1.5 EV/s` when it must darken. A discontinuity snaps to the
new target and increments `history_epoch` exactly once.

## Tone curve and shader mirror

The CPU reference curve and HLSL helper use the same Truth names and the same
extended Reinhard transform: `x * (1 + x / 16) / (1 + x)`, clamped only at the
declared linear white point of `4.0`. It maps black to black, remains finite and
monotonic on nonnegative finite input, compresses highlights, reaches display
white at the declared white point, and does not clip below it.

`TruthColorCore.fxh` owns the shader-side sample/state vocabulary and math.
`enbeffect.fx` is a standalone Effects 11 fixture using `fx_5_0`, with simple
Truth vertex/pixel entry points. No external shader include is permitted.

## Verification

One dependency-free C++ assertion executable covers initialization,
deterministic target calculation, separate adaptation bounds, discontinuity
epoch behavior, every validation class, unchanged-state rejection, stable enum
values, and tone-curve invariants. A separate CTest invokes the exact x64 FXC
path and writes its `.fxo` and listing beneath the active CMake build tree.
