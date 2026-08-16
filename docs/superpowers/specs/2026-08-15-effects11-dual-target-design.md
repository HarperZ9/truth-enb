# Effects 11 Dual-Target and Community Shaders Sky Feature

## Purpose

On 3 August 2026 doodlum published Effects 11, a Community Shaders feature that
reimplements ENBSeries on FX11 and cannot coexist with it. It auto-patches most
ENB presets, ships replacements for ENB Helper and ENB Extender, and removes the
`d3d11.dll` proxy requirement. A second shader host now exists for the same
audience, and it is gaining users.

This spec covers three workstreams. Truth ENB and Elder ENB gain an Effects 11
install path alongside ENBSeries. SkyrimBridge gains a host-agnostic exported
state ABI. Truth's atmosphere, sky-field, and aurora work is offered upstream to
Community Shaders as a user-facing feature, with SkyrimBridge as an optional
input rather than the product.

ENBSeries remains the primary target. Nothing here reduces the ENBSeries
release, and no work in this spec blocks the Nexus publish sequence.

The three workstreams live in three repositories and are separately plannable.
W1 and W2 have no dependency on each other. W3 depends on W2 only for its
optional input path, so it can begin before W2 completes. Each workstream takes
its own implementation plan rather than sharing one.

## Established from source

Every claim below was read from the Community Shaders `dev` branch or from this
workspace. Nothing is inferred from the Nexus description.

- Community Shaders exports only `SKSEPlugin_Load`, `SKSEPlugin_Version`, and
  `SKSEPlugin_Query` (`src/XSEPlugin.cpp:49,61,70`). It publishes no ENB C API.
- It includes `include/ENB/ENBSeriesAPI.h` as a consumer, resolving symbols with
  `GetProcAddress` against an ENB module when one is loaded.
- `ShaderPatches.json` contains one `enbeffect.fx` patch, rewriting
  `lerp( TOD(a), a##_Interior, EInteriorFactor )` to a ternary. Neither Truth nor
  Elder writes that form, so the patcher is a no-op on both presets.
- `SettingsPatches.json` is a blocklist matched on exact variable-name strings.
  It force-disables sharpening, blur, AA, vignette, and letterboxing harvested
  from popular presets.
- `Feature::GetFeatureList()` returns a static compiled-in vector
  (`src/Feature.cpp:219`). There is no runtime feature registration.
- `CONTRIBUTING.md` states that backend systems for other features are not
  considered features, caps reviewable PRs at 4,000 lines, and asks for CPU and
  GPU numbers.
- Both preset compile sites pass `nullptr` for `pDefines`
  (`src/Features/Effects11/Effects/Effect.cpp:301,369`). Effects 11 injects no
  preprocessor defines into preset shaders.
- `Effects11::GetShaderDefineName()` returns `EFFECTS11`, which reaches Community
  Shaders' own game shaders and not preset FX11 compilation.

## Consequences

A preset cannot detect its host at compile time. Host selection must therefore
travel through configuration, which Truth already generates per tier.

SkyrimBridge's per-frame publish path depends on `ENBSetParameter`. Under
Effects 11 that symbol does not exist, so the ENB half goes dark. The remaining
SkyrimBridge surface, EngineReflect's 827 fields, collision generation, and
asset import, is unaffected by which shader host is installed.

`enb-runtime-core` resolves and fingerprints an already-loaded ENB host. Under
Effects 11 there is no such host.

## W1: dual-target the presets

Truth and Elder shaders compile unmodified under Effects 11. The defect is
silent double-processing: their post chains stay enabled because the Effects 11
blocklist does not know the variable names `sharpenStr`, `vignetteEnable`,
`TruthPostpassGrainShape`, or `[Truth 70] Postpass | Grain Shape`. Under
Effects 11 those stack on Community Shaders upscaling and TAA.

Host selection extends the existing five-tier generator with a host axis rather
than duplicating the shader tree. One source tree continues to produce the
ENBSeries presets and additionally produces Effects 11 variants whose ini
disables the stages Community Shaders already owns: sharpening, film grain,
vignette, anti-aliasing, and letterboxing. The shader sources do not fork.

The upstream half is a data-file contribution adding Truth and Elder variable
names to `SettingsPatches.json`, so a user installing either preset under
Effects 11 gets correct behaviour without the variant. This is small, obviously
useful, and carries no architectural risk. It is the first contact with the
project and it precedes the sky PRs.

## W2: SkyrimBridge exported state ABI

SkyrimBridge already exports a versioned C interface
(`src/d3d11_proxy/ProxyAPI.h:147`, `src/core/EditorIDCache.cpp:277`). This
workstream generalises that precedent into `SB_GetBridgeInterface()`, returning a
versioned struct of function pointers over a per-frame state block.

The ABI is host-agnostic. Under ENBSeries it feeds the existing
`ENBInterface` publish path unchanged. Under Community Shaders it is what the
sky feature reads. It is useful independent of whether any Community Shaders
work is accepted, because it decouples state production from the host that
consumes it.

The ABI stays MIT. No GPL code enters SkyrimBridge.

## W3: the Community Shaders sky feature

A feature carrying SkyrimBridge state into shaders would be a backend system and
would be rejected by `CONTRIBUTING.md`. The feature offered is therefore the sky
work itself, which is user-facing, original, physically grounded, and aimed at a
gap Community Shaders acknowledges.

The port covers 1,415 lines of HLSL:

| header | lines |
|---|---|
| `TruthAtmosphereCore.fxh` | 94 |
| `TruthSkyViewAdapter.fxh` | 105 |
| `TruthCloudLighting.fxh` | 148 |
| `TruthSkyFields.fxh` | 170 |
| `TruthAuroraCurtain.fxh` | 383 |
| `TruthCloudVolume.fxh` | 444 |
| `TruthQuality.fxh` | 71 |

The C++ side is a `Feature` subclass following `docs/new-feature-template`: name,
category, settings, constant buffer, UI, and shader define. SkyrimBridge
detection is optional and happens at runtime through `GetModuleHandle` plus
`GetProcAddress("SB_GetBridgeInterface")`. When the module is absent the feature
runs on Community Shaders' own weather and time data, so it never hard-depends on
a second mod.

The 4,044 lines of CPU mirror in `src/render` and `include/truth` do not port.
They remain in this repository as the parity oracle and as the evidence behind
any performance or correctness claim made in a PR.

## Licence boundary

Truth ENB, Elder ENB, Elder Weathers, SkyrimBridge, and ENB Runtime Core are MIT
and stay MIT. Community Shaders is GPL-3.0-or-later with a modding exception.

MIT source may be contributed into a GPL project. GPL source may not be brought
back. Flow is one-way and the boundary is a module boundary: the GPL feature
consumes SkyrimBridge's MIT header through `GetProcAddress`, which is the same
shape Community Shaders already uses against `ENBSeriesAPI.h`. No Community
Shaders code enters this workspace at any point.

## PR sequence

| order | content | size |
|---|---|---|
| 1 | `SettingsPatches.json` entries for Truth and Elder | data only |
| 2 | atmosphere core, sky-view adapter, tier selection, feature scaffold | 270 HLSL |
| 3 | sky fields, cloud volume, cloud lighting | 762 HLSL |
| 4 | aurora curtain | 383 HLSL |

`TruthQuality.fxh` ships in PR 2 because tier selection gates every later stage.
The three shader PRs sum to the full 1,415 lines.

Each arrives polished, carries CPU and GPU numbers, and uses the Alpha flag until
the set lands. `CONTRIBUTING.md` directs contributors to Discord to discuss
potential contributions, and that conversation happens before PR 2.

## enb-runtime-core: honest null

No port. The library resolves an ENBSeries host, and under Effects 11 there is
none to resolve. Its README gains a plain statement that it is ENBSeries-only.

A host-abstraction layer is deferred, not rejected. It becomes worth building
when a second host exposes an API surface to abstract over, and Effects 11 does
not currently expose one.

## Verification

Preset dual-targeting uses the existing package-validation harness, which already
emits rendered evidence, extended to produce evidence under both hosts. Success
is the Effects 11 variant showing no sharpening or grain contribution above the
measurement floor while the ENBSeries variant is unchanged from its current
rendered baseline.

The ABI needs version-negotiation cases and an absent-host case asserting that a
missing module degrades to defaults rather than failing.

The sky feature tests as parity against the existing CPU mirrors. Tier
selection stays hash-verified by the current tier tests.

## Open questions

Whether Community Shaders will accept the sky feature is unknown and outside our
control. PR 1 is deliberately sequenced first so that the answer arrives cheaply.

Whether Effects 11 supplies every input Truth's `enbeffect.fx` reads is
unverified. It compiles, which is necessary and not sufficient. This is settled
by running it, and it belongs to the human testing phase already planned before
any Nexus publish.
