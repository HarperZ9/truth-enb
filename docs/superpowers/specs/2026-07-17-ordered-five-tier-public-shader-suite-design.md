# Truth ENB Ordered Five-Tier Public Shader Suite

## Purpose

Truth ENB will ship as a restrained, coherent, product-owned ENBSeries 0.504
shader suite rather than a native `enbeffect.fx` surrounded by unrelated legacy
passes. The release look is professional, seamless, timeless, and cinematic.
Quality scaling changes cost and refinement without changing the underlying
grade or enabling every available effect.

The rewrite covers the complete shipped shader chain and its configuration:

1. `enbeffectprepass.fx`
2. `enbdepthoffield.fx`
3. `enbbloom.fx`
4. `enbadaptation.fx`
5. `enblens.fx`
6. `enbeffect.fx`
7. `enbeffectpostpass.fx`
8. `enbsunsprite.fx`
9. `enbunderwater.fx`

Existing Truth CPU references, WARP parity tests, runtime contracts, and
attribution remain foundations. The official ENB fallback stays
provenance-separated and byte-locked; it is not rewritten.

## Selected architecture

Truth uses one modular source tree with five compile-time permutations and five
generated complete presets. It does not duplicate five shader trees and does
not put every effect behind dynamic branches in one monolithic pixel shader.

Product modules have one responsibility and expose bounded input/output
contracts:

- `contract`: host inputs, finite-value sanitizers, depth convention, runtime
  status, and `SB_Retain` compatibility;
- `scene`: depth reconstruction, stable masks, and HDR scene augmentation;
- `environment`: atmosphere, sky, clouds, aurora, fog, and underwater media;
- `optics`: depth of field, bloom, lens response, and sun sprite;
- `color`: exposure, tone mapping, gamut handling, and restrained finishing;
- `quality`: tier constants, feature availability, and instruction budgets.

All stages use linear HDR values until the main effect applies exposure and tone
mapping. LDR post-processing cannot re-expose, re-grade, or re-bloom the image.

## Modern-technique compatibility renderer

Truth treats ENB's fixed stages, ordered sub-techniques, and named render
targets as a small compatibility framegraph. Modern effects are backported
where the game does not expose their ideal engine pipeline; they are not
silently dropped merely because motion vectors, compute dispatch, or arbitrary
history buffers are unavailable.

The verified installed host surface includes HDR color and depth, engine
normals and material mask in prepass, celestial/view data, a multiresolution
bloom chain, one-pixel adaptation history, and current-frame scratch targets.
Those surfaces are useful but not interchangeable. Each stage declares its
available inputs, scratch ownership, lifetime, resolution, and fallback level.
A target is current-frame-only unless live validation proves persistence.
Alpha or unused color channels cannot be reused across effects without an
explicit packing contract and a round-trip test.

Every modern technique follows one public capability ladder:

1. use a valid native ENB input when present;
2. use a versioned SkyrimBridge value or reconstruction when available;
3. use a stable bounded spatial approximation;
4. return the exact authored identity when confidence is insufficient.

The shader implementation may therefore use:

- depth/normal horizon visibility for GTAO-like ambient occlusion and contact
  shadowing;
- short confidence-weighted SSR with edge rejection and binary refinement;
- depth-, normal-, and mask-aware separable subsurface diffusion;
- Rayleigh/Mie/ozone transmittance, depth-bounded aerial perspective, and
  shipped low-dimensional atmosphere data inspired by Hillaire's production
  model;
- Beer-Lambert cloud extinction, stable world-space sampling, early exits, and
  spatial bilateral reconstruction;
- signed circle-of-confusion, multiresolution bloom, robust adaptation, and
  luminance-preserving tone/gamut mapping.

It may not label camera-only reprojection as object motion, a luminance delta as
a motion vector, or a current-frame target as persistent history. Temporal
upscaling, temporal SSGI/SSR denoising, frame generation, and reservoir reuse
remain future Bridge-assisted capabilities. Until their required resources
exist, the release uses explicit stable spatial fallbacks instead of
frame-random sampling.

Technical references:

- Sébastien Hillaire, *A Scalable and Production Ready Sky and Atmosphere
  Rendering Technique*: https://sebh.github.io/publications/egsr2020.pdf
- Maxime Heckel, *On Rendering the Sky, Sunsets, and Planets*:
  https://blog.maximeheckel.com/posts/on-rendering-the-sky-sunsets-and-planets/

## ENB render order and ownership

The fixed host order is treated as an API:

1. **Prepass, HDR:** reconstruct depth once; create depth-safe sky/interior
   masks; compose atmosphere, analytic or volumetric clouds, aurora, room light,
   restrained AO/SSR/SSS/snow, and fog exactly once. The result must be suitable
   for downstream depth of field and bloom.
2. **Depth of field, HDR:** apply only lens focus. It cannot add grading,
   vignette, sharpening, or atmosphere.
3. **Bloom, HDR:** extract and filter scene radiance. It cannot perform lens
   dirt, tone mapping, or general blur.
4. **Adaptation, HDR:** meter a robust luminance statistic and produce bounded
   exposure history. Sky and UI outliers cannot dominate metering.
5. **Lens, HDR:** respond to the bloom signal with restrained ghosts, veiling
   glare, and optional dirt. Zero intensity is an exact identity.
6. **Main effect, HDR to display:** combine scene, bloom, and lens once; apply
   exposure, the Truth tone curve, color response, and gamut compression.
7. **Postpass, LDR:** optional fine grain and vignette, then final triangular
   dithering. Sharpening is absent by default and never follows dithering.
8. **Sun sprite:** use the engine-aligned celestial direction and a bounded
   optical response without duplicating bloom or atmosphere.
9. **Underwater:** replace incompatible air effects with one underwater medium
   model; do not stack a second fog, grade, lens dirt, or god-ray system.

## Five quality tiers

`TRUTH_QUALITY_TIER` is an integer from 0 through 4. Tier-dependent loop bounds
and feature availability are compile-time constants. User intensity controls
remain runtime values inside safe ranges.

| Tier | Name | Baseline |
|---:|---|---|
| 0 | Performance | Analytic clouds, essential atmosphere, low-sample bloom, DOF disabled by default, no SSR |
| 1 | Balanced | Analytic clouds, restrained DOF, low-cost AO, simple lens response |
| 2 | Quality | Entry volumetric clouds, standard DOF/bloom, stable SSR, moderate aurora integration |
| 3 | Ultra | Higher volumetric and optical sampling, improved self-shadowing and depth refinement |
| 4 | Cinematic | Highest bounded sampling and temporal stability; photographic secondary effects remain restrained |

Each tier has a complete preset containing shader feature flags, intensities,
sample budgets, ENB configuration values, and product metadata. A tier is not a
bag of sample-count overrides. All five presets target the same exposure,
neutral balance, black point, highlight behavior, and atmosphere density.

The authored default is **Balanced**. Advanced users can change granular
controls after selecting a tier. Resetting a tier restores its complete known
baseline.

## Artifact prevention

- All depth comparisons use one declared convention and a validated sky-mask
  feather; no stage invents its own threshold.
- Procedural fields are world-stable. Screen-space or frame-random jitter is
  not used without a valid temporal resolve.
- Missing history or velocity selects the documented spatial fallback; it does
  not enable guessed reprojection or reuse an unverified persistent target.
- Scratch-target and packed-channel ownership is statically checked so one
  modern effect cannot consume another effect's transient data.
- Cloud, fog, aurora, bloom, lens, and underwater attenuation each compose
  exactly once.
- Real engine sun direction is preferred through a backward-compatible runtime
  celestial payload. Missing or invalid payloads fail closed to the authored
  scene rather than lighting clouds with a contradictory game-hour proxy.
- Every public intensity has an exact identity value and a bounded maximum.
- NaN and infinity are sanitized at stage boundaries, not hidden after final
  tone mapping.
- Postpass writes stable alpha and applies dithering last to prevent LDR
  banding.

## Configuration and packaging

A canonical, reviewable tier manifest is the source of truth. A generator emits
the five preset directories and their `.ini` values deterministically. Generated
files carry the tier manifest hash and are verified against the shader symbols
they configure.

The public package contains only Truth-authored shaders and configuration,
required documentation, the Truth runtime plugin, and the separately identified
official ENB fallback source permitted by the existing host contract. It does
not redistribute ENB binaries.

`tools/sky-mesh` remains a separately licensed GPL-3.0-or-later tool and is
excluded from the ENB runtime archive. Its license and boundary are explicit in
the source repository.

## Provenance and interoperability

Prior authors and technique lineage remain credited. Compatibility with
Kitsuune/LonelyKitsuune workflows uses independently authored schemas and
adapters. Public source and packages contain no proprietary plugin binaries,
private reverse-engineering specifications, recovered implementation, or
permission-dependent replacement component. Compatibility claims describe the
accepted public formats and observable interface behavior, not authorship.

## Verification

Implementation is accepted when:

- all nine stages compile in all five tiers with strict FXC settings;
- static instruction and loop/sample budgets are recorded per stage and tier;
- existing CPU/WARP parity remains green for Truth math;
- reference captures cover clear day, clear night, cloudy aurora, storm,
  interior, high-contrast edge, underwater, and missing-runtime fallback;
- identity controls are exact and repeated effect application is absent;
- native, Bridge-assisted, spatial-fallback, and identity capability paths are
  exercised without changing stage order;
- scratch lifetimes and channel ownership match the declared compatibility
  framegraph;
- both deterministic package runs produce byte-identical archives and hashes;
- the public archive contains no private/protected input or ENB binary;
- live ENB 0.504 validation confirms stable depth, camera, FOV, weather,
  interior, menu, save/load, and preset-reload behavior.

Offline gates prove compilation, math, budgets, determinism, and package
contents. They do not substitute for the final live-game acceptance matrix.
