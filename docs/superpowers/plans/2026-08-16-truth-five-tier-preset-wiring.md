# Truth ENB Five-Tier Preset Wiring Plan

## Goal

Make all five public presets select their real HLSL quality tier and provide
restrained, stage-specific ENB settings for the fixed nine-stage pipeline.

Canonical tiers are Performance `0`, Balanced `1` (default), Quality `2`,
Ultra `3`, and Cinematic `4`.

## Design constraints

- Installed presets cannot rely on an INI value to create a preprocessor
  definition.
- `TruthQuality.fxh` must include a preset-owned override before its guarded
  Balanced fallback. Command-line `/DTRUTH_QUALITY_TIER=N` remains authoritative.
- Each preset overlay owns one `truth/TruthQualityPresetOverride.fxh` containing
  only a guarded tier definition.
- ENB configuration keys use the exact shader `UIName` strings, under the exact
  uppercase stage section (for example `[ENBEFFECT.FX]`). HLSL identifiers are
  not assumed to be serialized keys.
- Every stage INI configures only controls declared by that stage or its direct
  parameter include. Host-only postpass values stay in the postpass INI.
- Performance disables or minimizes costly optics; Balanced is mild and is the
  supported default; higher tiers primarily add sampling quality. Cinematic is
  bounded and is not an all-effects-max preset.
- Disabled or zero-intensity stages retain their existing exact-identity
  contract. No render-order or runtime ABI change is part of this task.

## Implementation

1. Add the default override include and consume it before the quality fallback.
2. Generate a per-host/per-tier override include alongside the ten INI files.
3. Replace copied metadata stage INIs with exact stage-section/UIName settings
   and keep `truth-quality.ini` as human-readable tier metadata.
4. Define conservative values for all five tiers, including the main-effect
   controls and restrained public optical/sky values.
5. Update preset and public-package manifests/checkers/install rules.
6. Add a small semantic compile probe that compiles without `/D`, resolves the
   preset overlay first, and proves the selected tier and representative budgets.
7. Run only the focused preset semantic/package checks and relevant stage compile
   matrix; do not run the long reference renderer or broad suite.

## Acceptance

- All five preset overlays cause tier values `0..4` without `/D`.
- Explicit `/D` continues to override the default file.
- All nine INIs have the correct section and exact UIName keys; no generic
  `[TRUTH STAGE] Name=...` blocks masquerade as effect settings.
- Balanced is mild and complete, Performance is meaningfully cheaper, and
  Cinematic remains bounded.
- The public archive contains the override include for every host/tier and the
  checker rejects a missing or incorrect override.
- Focused checks pass and `git diff --check` is clean.
