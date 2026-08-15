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

## Requirements (add these on the mod page)

- Skyrim Special Edition or Anniversary Edition
- ENBSeries (built and validated against 0.504)
- Optional: SkyrimBridge, if you want live engine state driving the shaders

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

Each tier is a complete preset tree, and the difference is enforced rather than
assumed: the five tiers are rendered on a software device and compared by content
hash, so a tier that is declared but produces an identical image fails the build.

[size=4]What is verified, and how[/size]

The release gate is ten tests, all passing: quality presets, the sky-view adapter
and its shader contract, scene contracts, optical and composition contracts, the
aurora default-quality contract, the stage compile matrix, the balanced prepass
instruction budget, and runtime reproducibility. The wider suite is 32 tests.

The safety contracts run the real shader code rather than reading it. Unoccluded
ambient occlusion returns the scene unchanged, samples taken across a depth
discontinuity are rejected instead of counted as occlusion, a reflection ray that
misses returns the scene, and a zero skin mask returns the scene bit for bit.
Each of those is asserted against rendered output on a software device.

The package is deterministic and ships a SHA-256 sidecar.

[size=4]Scope, stated plainly[/size]

In-game visual validation across SE, AE, and ENB 0.504 is ahead of this release.
The gates prove the shaders compile within budget, that the tiers differ, that
the identity contracts hold, and that the package is reproducible. They do not
prove it looks good on your monitor in your load order. That judgment is yours,
and feedback with screenshots is the most useful thing you can send.

[size=4]Pairs with SkyrimBridge[/size]

Truth is standalone and needs nothing else. If you also run SkyrimBridge, live
engine state becomes available to the shaders through it: weather, camera,
celestial position, and interior state, published as shader parameters each
frame.

[size=4]Credits[/size]

Truth's shaders are original, and the work still stands on named prior authors:
Boris Vorontsov and ENBSeries, kingeric1992, Adyss, TreyM, l00ping,
TheSandvichMaker and ReforgedUI, and Marty McFly. Reliance is by technique,
format, or citation, never copied source. Every credit is preserved in the
shader headers and in [font=Courier New]CREDITS-AND-PROVENANCE.md[/font], and
must stay there in any redistribution.

The ENB 0.504 vanilla fallback is kept byte-immutable so the original post-process
path is always recoverable.

[size=4]Install[/size]

[list=1]
[*]Install ENBSeries and its binaries into your game root first.
[*]Install this mod with a mod manager, or copy the [font=Courier New]Root[/font]
folder contents into your game root next to the executable.
[*]Pick a tier from [font=Courier New]Presets[/font] and copy its
[font=Courier New]ROOT/enbseries[/font] contents over your enbseries folder.
[*]Launch, and press End in game to confirm ENB is active.
[/list]

[size=4]Source and license[/size]

MIT licensed. Source and the full build and verification pipeline:
https://github.com/HarperZ9/truth-enb

## Permissions (open, MIT-aligned)

- Users can modify this file: yes
- Users can convert this file to work with other games: yes
- Users can use assets from this file without permission with credit: yes
- Others can use assets in this file with credit, without permission: yes
- Upload to other sites: yes, with credit

State on the page: this mod is MIT licensed; use it, modify it, patch it, and
build presets on it, with credit. Prior shader-author attribution in the headers
is not waived by that licence and must be preserved.
