# Disposition: `agent/public-shader-hardening-20260717`

Four commits from 2026-07-17 that never reached `main`. Assessed 2026-08-15.

    2cf3c80  document Truth public RC boundaries, credits, and safe defaults
    615ad0b  harden Truth optical and sky masking paths
    1342dfc  add balanced Truth sky field tier and inactive-path early outs
    b3b762b  tune Truth defaults for restrained public presets

The branch is kept, not merged and not deleted. Merging it verbatim would be
wrong, and deleting it would throw away work `main` still does not have.

## Why it is not a merge

The merge base is `3d73726` (2026-07-16). `main` has moved 23 commits since,
including the ordered-stage rewrite, so the two diverged structurally rather
than textually. Per file:

| file | on `main` since base | disposition |
| --- | --- | --- |
| `shaders/truth/TruthSkyFields.fxh` | **0 commits**, still 170 lines | **port** |
| `shaders/enbeffect.fx` | 4 commits, 235 to 147 lines | **superseded** |
| `shaders/truth/TruthEffectParameters.fxh` | 2 commits, same length | review |
| `README.md` | 1 commit | review |

`enbeffect.fx` is the clear supersede: `main` rewrote it around ordered display
stages and it shrank from 235 lines to 147 as work moved out into stage headers.
The branch's 304-line version predates that architecture entirely.

`TruthSkyFields.fxh` is the opposite. `main` has never touched it since the merge
base, so the branch's work was not rejected or replaced. It simply never landed.

## What is genuinely missing, and what is not

Checked against `main` rather than assumed:

**Already on `main`, do not re-port.** The phase wrap
(`wrapped_phase = input.phase >= 1.0 ? 0.0 : input.phase`) is at line 101 of
`main`'s copy, identical to the branch. It arrived independently.

**Missing: the inactive-path early-out.** `main` evaluates its nine
`TruthSkyValueNoise3D` calls unconditionally. The branch guards the whole body on
`cloud_coverage > 0.0001 && cloud_density > 0.0001`, so a clear sky skips the
noise entirely. That is exactly the scene where it costs most and shows least.

**Missing: sky-field quality tiering.** `main`'s sky field is un-tiered and costs
the same at Performance as at Cinematic. The branch adds three levels: nine noise
calls at the top, roughly six in the middle, two at the bottom.

## The conflict to resolve when porting

The branch introduces its own `TRUTH_SKY_FIELD_QUALITY` (0 to 2). `main` now
ships a unified `TRUTH_QUALITY_TIER` (0 to 4) in `TruthQuality.fxh`, which
governs cloud steps, aurora samples, AO, DOF, bloom, lens and SSR, and which the
rendered tier tests verify by content hash.

Porting the branch verbatim would put a second, competing quality macro beside
the shipped one. The sky field should instead gain an entry in `TruthQuality.fxh`
like every other tiered stage, with the branch's three levels mapped onto the
five tiers.

## Scope of the port

Not a cherry-pick. `TruthSkyFields.fxh` has a CPU mirror in
`src/render/SkyFields.cpp` and a parity test, so an early-out added to the HLSL
alone would break parity. The work is:

1. Add a sky-field noise budget per tier to `TruthQuality.fxh`.
2. Add the early-out to the HLSL and to the CPU mirror together.
3. Gate the noise count by tier in both.
4. Extend the tier tests, which currently assert the five tiers render to
   different hashes; a sky-field budget that varies by tier should strengthen
   that rather than disturb it.

Deferred to the expansion session by decision on 2026-08-15. Until it lands the
branch stays on the remote, because it is the only copy of that work.
