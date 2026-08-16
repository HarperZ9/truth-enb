# Nexus mod page content: Truth ENB

Everything needed for the Nexus upload form. The description is BBCode, ready
to paste into the mod-page description field.

## Form fields

- **Name**: Truth ENB
- **Summary** (one line): An original ENB shader suite built on a physical sky:
  single-scattering atmosphere, raymarched cloud volume, aurora curtain, and
  occlusion-aware interior light, in five quality tiers.
- **Category**: Visuals and Graphics
- **Version**: 1.0.0

## Promotional media policy (required)

The images under `media/nexus` are generated promotional brand art, not
gameplay screenshots or evidence of in-game visual quality.

- Use this exact caption on every generated promotional image:
  **Generated promotional brand art — not an in-game screenshot.**
- Apply the Nexus Mods **AI Media** tag whenever any generated promotional
  image is used on the page.
- Do not submit or use these images in the current Nexus Mods
  **25th Anniversary Mod Drive**.
- Support every visual claim—including shader-quality claims, before/after
  comparisons, quality-tier comparisons, and runtime feature
  demonstrations—with clearly labeled, real in-game screenshots or captures.
  Generated promotional art must never be presented as that evidence.

## Requirements (add these on the mod page)

- Skyrim Special Edition or Anniversary Edition
- Choose one shader host:
  - ENBSeries 0.504 for the full currently implemented suite; or
  - Community Shaders with Effects 11 for the partial optical/post
    compatibility path.
- ENBSeries host only: the bundled `TruthENBRuntime.dllplugin`, installed under
  `enbseries`, publishes native camera state and requires the matching Address
  Library database under `Data/SKSE/Plugins` (`version-1-5-97-0.bin` on SE
  1.5.97, or the matching
  `versionlib-<major>-<minor>-<patch>-<build>.bin` on AE 1.6.x).
- SkyrimBridge is optional for the base optical suite, but required by the
  current procedural sky and sun path for its versioned
  `SkyrimBridge_GameState` celestial vector. Truth's native camera path does
  not require it.

## Description (BBCode)

[size=5]Truth ENB[/size]

An ENB shader suite written from scratch, where the sky is computed rather than
tuned. A single-scattering atmosphere model drives the look, and the rest of the
suite is built on top of it: procedural sky fields, an aurora curtain, a
raymarched cloud volume, an occlusion-aware interior light model, and a
screen-space pass with ground-truth ambient occlusion, reflections, and skin
diffusion.

Five quality tiers ship as complete presets, so the same look scales from a
laptop to a machine that can afford to march clouds.

[size=4]The sky is a model, not a table[/size]

Most presets store the sky as tuned colour values. Truth computes it. A
single-scattering model evaluates the atmosphere per frame, which is why the
transitions hold together: sunrise is warm because the air mass is long, not
because a curve said so, and an overcast sky collapses the direct light because
the model says it should.

Around that core:

[list]
[*][b]Procedural sky fields[/b] with a stable, non-jittering sample pattern, so
the sky does not shimmer under a moving camera.
[*][b]An aurora curtain[/b] with quality-scaled sample counts and a phase model
that wraps cleanly, so the endpoints agree instead of popping.
[*][b]A raymarched cloud volume[/b] on the upper three tiers, with interleaved
sampling for stability rather than random temporal jitter.
[*][b]An interior light model[/b] that excludes exterior daylight exactly when a
space is sealed. A basement is dark because the model resolves it to zero, not
because a fog plane was pulled in to hide it.
[/list]

[size=4]Five tiers, and they are actually different[/size]

[list]
[*][b]Performance[/b] and [b]Balanced[/b] are analytic. Volume marching is
compiled out entirely, not stepped down.
[*][b]Quality[/b], [b]Ultra[/b], and [b]Cinematic[/b] march the cloud volume at
8/2, 12/3, and 16/4 step budgets.
[/list]

Each tier is a complete preset tree. An installed-tree compile probe proves the
overlay selects its intended tier without command-line defines, and the strict
shader matrix requires five bytecode-distinct HDR prepasses. Those are compile
and integration facts, not claims about unrecorded in-game appearance.

[size=4]What is verified, and how[/size]

The automated release gate covers quality presets, the sky-view adapter and its
shader contract, scene contracts, optical and composition contracts, the aurora
default-quality contract, the stage compile matrix, the balanced prepass
instruction budget, and runtime reproducibility. Treat those as passed only when
they are recorded against the final posted archive and its SHA-256 sidecar.

The narrower `truth_release_gate` does not include the full rendered-output
screen-space WARP artifact. That evidence comes from `truth_screen_space_warp`
when the final artifact suite is run: unoccluded ambient occlusion must return
the scene unchanged, samples taken across a depth discontinuity must be rejected
instead of counted as occlusion, a reflection ray that misses must return the
scene, and a zero skin mask must return the scene bit for bit.

The package gate requires deterministic output and a SHA-256 sidecar for the
final archive.

[size=4]Scope, stated plainly[/size]

Final in-game visual and gameplay acceptance across SE, AE, and ENB 0.504 is not
recorded yet. Public upload remains blocked until the integrated Performance,
Balanced, Cinematic, and no-runtime/fail-closed matrix is run and recorded. The
automated gates, when recorded for the final artifact, cover shader compilation
budget, tier distinctness, identity contracts, and package reproducibility. They
do not prove it looks good on your monitor in your load order. That judgment is
yours, and feedback with screenshots is the most useful thing you can send.

[size=4]Native runtime and SkyrimBridge compatibility[/size]

Truth's native camera path is its bundled ENB external plugin,
`TruthENBRuntime.dllplugin`. It reads the matching Address Library database from
`Data/SKSE/Plugins` and publishes camera shader parameters without
requiring SKSE, CommonLib, or SkyrimBridge.

SkyrimBridge is optional for the base optical suite and required for the current
procedural sky and sun path. Truth reads only the validated sun vector from the
versioned `SkyrimBridge_GameState` mapping; it does not claim SkyrimBridge
weather, camera, or interior bindings here. If the mapping is absent or invalid,
the procedural sky and sun sprite fail closed while the base optical suite and
native camera path remain independent.

The Effects 11 overlay is currently a partial optical/post compatibility path.
Effects 11 does not expose the ENB SDK host used by Truth's native camera
publisher, so world-space prepass composition remains fail-closed there. This
release does not claim feature parity between the two hosts.

[size=4]Credits[/size]

Truth's shaders are original, and the work still stands on named prior authors:
Boris Vorontsov and ENBSeries; Kitsuune / LonelyKitsuune for interoperability
context, not copied or reverse-engineered implementation; kingeric1992, Adyss,
TreyM, l00ping,
TheSandvichMaker and ReforgedUI, and Marty McFly. Reliance is by technique,
format, or citation, never copied source. The complete attribution record is in
[font=Courier New]CREDITS-AND-PROVENANCE.md[/font] and must stay with any
redistribution.

Maxime Heckel's sky-rendering article is credited as an accessible reference
for atmospheric scattering and ray/sphere reasoning; Truth's implementation is
independently authored.

No ENB or Bethesda shader source is redistributed. The reserved fallback name
uses a Truth-owned scene-color identity path.

[size=4]Install[/size]

[list=1]
[*]Choose one host. Install either ENBSeries 0.504 and its binaries into the
game root for the full currently implemented suite, or install Community
Shaders with Effects 11 for the partial optical/post compatibility path. Do not
install both.
[*]Install this mod with a mod manager, or copy the [font=Courier New]Root[/font]
folder contents into your game root next to the executable.
[*]Pick exactly one host and tier from
[font=Courier New]Presets/<host>/<tier>[/font], then copy that overlay's
[font=Courier New]ROOT[/font] contents over the common [font=Courier New]Root[/font]
tree before deployment. Match the overlay host to the host selected in step 1;
Balanced is the recommended starting tier.
[*]ENBSeries host only: install the exact matching, non-bundled Address Library
database under [font=Courier New]Data/SKSE/Plugins[/font] for the selected
Skyrim runtime. Effects 11 does not use Truth's native runtime bridge.
[*]Launch and confirm the selected shader host is active.
[/list]

[size=4]Source and license[/size]

Truth-authored shaders, configuration, runtime plugin, and documentation in the
public archive are MIT licensed. ENBSeries, Address Library, Bethesda assets,
the separately GPL sky-mesh tool, and generated meshes are not included. Source
and the full build and verification pipeline:
https://github.com/HarperZ9/truth-enb

## Permissions (open, MIT-aligned)

- Users can modify this file: yes
- Users can convert this file to work with other games: yes
- Users can use assets from this file without permission with credit: yes
- Others can use assets in this file with credit, without permission: yes
- Upload to other sites: yes, with credit

State on the page: the Truth-authored public archive is MIT licensed; use it,
modify it, patch it, and build presets on it, with credit. Third-party projects
named for platform, scientific, or interoperability context are not relicensed.
