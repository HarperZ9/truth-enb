# Task brief: wire Truth's five public tiers into installed shaders

Work from `C:\dev\truth-enb-worktrees\public-release-integration` on
`fix/truth-public-release-integration`.

Read `docs/superpowers/plans/2026-08-16-truth-five-tier-preset-wiring.md` and
the current generator/checker/package code before editing. Follow TDD: first
make a focused check demonstrate that a preset overlay does not currently
select its shader tier, then implement the smallest production path.

Owned surface:

- `shaders/truth/TruthQualityPresetOverride.fxh` (new)
- `shaders/truth/TruthQuality.fxh`
- `cmake/GenerateTruthQualityPresets.cmake`
- `cmake/CheckTruthQualityPresets.cmake`
- `cmake/CheckTruthReleasePackage.cmake`
- focused semantic probe/check and only the CMake install/test declarations it
  requires
- parameter default/range files only when a documented restrained public value
  cannot be expressed safely through generated ENB INIs
- this plan/brief and the SDD report/progress artifact

Do not modify media, Nexus copy, runtime ABI, renderer/reference code, or reorder
shader stages. Preserve the integrated commits and concurrent documentation work.

Use exact ENB serialization names. Example observed live output:

```ini
[ENBEFFECT.FX]
[Truth 00] Master | Enabled=true
[Truth 12] Clouds | Density=0.62
```

Do not use `TruthCloudDensity=...` merely because it is the HLSL identifier.
Do not repeat host/tier metadata in every stage INI. Put postpass host values only
in `enbeffectpostpass.fx.ini` using their exact UIName strings.

The base override should be:

```hlsl
#ifndef TRUTH_QUALITY_TIER
#define TRUTH_QUALITY_TIER 1
#endif
```

and each generated preset overlay should use the same guarded shape with its
own tier. `TruthQuality.fxh` must include it before validating and branching on
`TRUTH_QUALITY_TIER`.

Verification is deliberately focused: preset generator/checker, semantic
overlay compile, public package manifest/checker if inexpensive, and relevant
stage compile matrix. Do not run the long WARP reference-renderer workload or
the full suite. Commit as `release: wire Truth quality presets into shaders` and
report RED/GREEN evidence, files, exact tier settings, generated layout, and
limitations.
