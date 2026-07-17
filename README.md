# Truth ENB

Truth ENB is an original ENBSeries 0.504 rendering stack for Skyrim Special
Edition and Anniversary Edition. It contains a production `enbeffect.fx`, a
small native camera bridge, and C++23 reference implementations for the color,
atmosphere, cloud, aurora, sky-view, and interior-light models.

## Release status

The repository is suitable for public source review under the MIT license. The
binary/mod package remains a **release candidate**, not a stable end-user
release, until the live Skyrim SE/AE + ENB 0.504 acceptance matrix in
[`docs/release-validation.md`](docs/release-validation.md) has been completed.
Offline FXC, CPU, D3D11 WARP, ABI, determinism, and package checks do not replace
that game-bound validation.

The package target retains the historical name
`Truth-ENB-Private-RC-win64.zip` for build-script compatibility. Do not present
that archive as a stable public release merely because it builds.

## Public-safe rendering policy

The live pass is deliberately more restrained than the reference renderer:

- ENB lens contribution is disabled by default; bloom remains available.
- Procedural sky replacement starts at 75 percent rather than replacing every
  depth-identified sky pixel at full strength.
- Weather, cloud, fog, wind, and radiance defaults are reduced.
- Aurora activity is opt-in instead of always active at night.
- The production sky field uses the balanced quality tier: five 3D-noise
  evaluations instead of the eight-noise reference tier.
- Zero cloud or aurora activity now exits before their procedural work.
- The sky mask keeps distinct smoothstep edges, rejects non-finite depth, and
  suppresses depth-discontinuity quads to reduce halos over trees, hair,
  mountains, and other silhouettes.
- Scene, lens, bloom, adaptation, exposure, and procedural radiance paths fail
  to finite bounded values instead of allowing NaN/Inf pixels to propagate.

The full quality-2 sky field remains the default for reference and parity
shaders. Authors can still select stronger settings in ENB's editor after
checking the result in representative weather, interiors, snow, water, and
night scenes.

## What is live

`shaders/enbeffect.fx` consumes ENB's scene, bloom, lens, depth, adaptation,
time, weather, day/night, and interior inputs. It replaces only
far-depth exterior sky pixels when the runtime camera protocol is valid, then
applies exposure and a bounded filmic curve. A Truth-named passthrough and ENB
0.504's required hash-locked vanilla fallback remain available.

The live shader includes:

- `TruthColorCore.fxh`
- `TruthAtmosphereCore.fxh`
- `TruthSkyFields.fxh`
- `TruthAuroraCurtain.fxh`
- `TruthCloudLighting.fxh`
- `TruthSkyViewAdapter.fxh`
- `TruthRuntimeParameters.fxh`
- `TruthEffectParameters.fxh`

The native bridge publishes a row-major inverse view-projection matrix, camera
position, protocol/generation state, and world-unit scale through six hidden
`float4` parameters. Missing, stale, non-finite, or incompatible runtime state
fails closed: the world-space sky path stays disabled while the ordinary color
path remains usable.

## Experimental components not in the live pass

`TruthCloudVolume.fxh` is a high-cost raymarched reference. It includes nested
primary/light sampling and cellular detail and is not shipped in the live ENB
include list. It should not be wired into `enbeffect.fx` until temporal
stability, horizon behavior, GPU timing, and in-game weather transitions have
been validated.

`TruthInteriorLight.fxh` and its CPU/WARP references model aperture- and
occlusion-aware exterior daylight. They are not yet connected to the live ENB
pass because the required per-cell aperture/occlusion inputs are not published
by the current runtime protocol.

The live sun direction is still a bounded game-hour proxy. Replacing it with
the engine sun vector requires an explicit runtime ABI revision and new CPU,
WARP, SE, and AE acceptance evidence; it must not be silently inferred from an
unverified parameter.

## Runtime boundary

The intended runtime peers are ENBSeries and Address Library. Truth does not
require SKSE, CommonLib, ENB Helper, a peer shader package, or a preset-overlay
tool. The bridge reads the Address Library database directly and writes only
ENB shader parameters during ENB callbacks.

`runtime/enb-upstream.lock` records the exact ENB 0.504 archive, wrapper,
compiler, shader, SDK archive, and SDK-header hashes used by the release
candidate. Truth does not redistribute ENB binaries.

## Build and test

Requirements:

- CMake 3.30 or newer
- Visual Studio 18 2026, x64
- C++23 with the static MSVC runtime
- x64 FXC at the path configured by the project presets

From the repository root:

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug
ctest --preset vs2026-x64-debug --output-on-failure --no-tests=error
cmake --build --preset vs2026-x64-release
ctest --preset vs2026-x64-release --output-on-failure --no-tests=error
cmake --build build --config Release --target truth_private_release_package
```

The test boundary covers CPU invariants, strict optimized FXC compilation,
D3D11 WARP execution of the exact production pixel entry, runtime ABI
reflection, sky-view matrix orientation, non-finite fallbacks, package
manifests, deterministic clean builds, and byte-identical fixed-epoch ZIPs.
The production compile also has a static instruction-slot ceiling; that metric
is a compile witness, not a claim about dynamic executed instructions on a
particular GPU.

Generate deterministic WARP reference captures with:

```powershell
.\build\Debug\truth_reference_renderer.exe `
  .\shaders\truth\TruthReferenceSky.hlsl `
  .\build\references\Debug
```

The reference renderer writes quiet clear night, active clear night,
cloudy-night aurora, and storm captures beneath the ignored build tree.

## Stable-release acceptance gates

Before a stable public binary upload, record results for at least:

1. Skyrim SE and AE with ENB 0.504, including startup, save load, exterior to
   interior transitions, fast travel, weather transitions, menus, alt-tab, and
   shutdown.
2. Day, dusk, night, storm, fog, snow, water, mountains, trees, hair, and
   particle-heavy scenes, with explicit checks for sky halos, flicker, NaNs,
   black frames, exposure pumping, banding, and aurora/cloud crawling.
3. Runtime absent, runtime stale, runtime invalid, Address Library mismatch,
   and safe-passthrough behavior.
4. GPU timing at 1080p, 1440p, and 4K on representative low-, mid-, and
   high-tier hardware.
5. Package installation through the intended MO2 Root-Builder layout and a
   clean uninstall/rollback check.

Unpassed gates must remain visible release-candidate limitations rather than
being converted into implied compatibility claims.

## Input contract

The CPU master-look update accepts finite scene and sky luminance from 0 to
1,000,000, an interior factor from 0 to 1, delta time above 0 and at most 1
second, and current/target exposure from -16 to +16 EV. Invalid input is
rejected without mutating state. Continuous adaptation is bounded to 3 EV/s
when brightening and 1.5 EV/s when darkening; discontinuities snap explicitly
and increment the history epoch once.

## License and scoped third-party material

The repository root is licensed under the MIT License; see [`LICENSE`](LICENSE).
The separate `tools/sky-mesh` sub-tool carries its own GPL-3.0 license files.
That sub-tool is not included in the Truth ENB runtime package. Its scoped
license must remain with the tool and must not be described as relicensing the
MIT runtime or shader files.

Truth's shipped implementation is clean project-owned code and does not import
recovered or peer shader source. The project nevertheless preserves the ENB
shader lineage and community credits named in the release handoff:

- Boris Vorontsov
- kingeric1992
- Adyss
- TreyM
- l00ping
- TheSandvichMaker / ReforgedUI
- Marty McFly

Those names acknowledge prior author lineage and technical influence; they do
not assert that every listed author's source is present verbatim. Existing
per-file copyright and attribution notices remain authoritative and must not be
removed during packaging or refactoring.
