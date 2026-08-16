# Truth ENB release validation

Truth supports ENBSeries as a peer; it never redistributes ENB binaries. Because
Skyrim SE builds retain the `0.504` version number across silent updates, the
public release candidate records hashes of the exact official upstream bytes
used for development in `enb-upstream.lock`; it redistributes none of them.

On 2026-07-14 America/Los_Angeles, the current archive was downloaded directly
from `http://enbdev.com/enbseries_skyrimse_v0504.zip`. Its SHA-256 was
`f8cc7b824c18736195d461d099cdd791f09789e6da7106962dde4b8a12d06e78`.
The wrapper reports file and product version `0.5.0.4`; exact component hashes
are in the lock file. The public shader uses a Truth-owned identity safety
fallback rather than embedding the upstream vanilla shader.

This pin matters for three current behaviors:

- ENB's 2026-05-07 news records a correction to overly early `OnBeginFrame`
  SDK callbacks. Truth publishes camera data from that callback.
- ENB's 2020-05-14 news explicitly says `UIHidden=1` shader values stay
  accessible through the SDK. The SDK v1002 header still contains an older,
  contradictory failure comment, so live hidden-parameter access remains a
  mandatory release test.
- Horizon Fix 0.2.3 asks ENB users to download an ENB build on or after
  2026-07-12. Truth does not require Horizon Fix or SKSE, but optional
  co-existence testing uses the same current upstream archive.

Authoritative upstream pages:

- `https://enbdev.com/download_mod_tesskyrimse.html`
- `https://www.enbdev.com/news.html`

Truth ENB public page:

- Nexus Mods: TBD until a Truth ENB page is assigned

## Automated gates

The Release build must pass the CPU suites, optimized D3D11 WARP production
pixel test, reserved fallback-interface contract, strict FXC permutations, static shader
budget, runtime plugin binary/ABI tests, and deterministic install/ZIP manifest.
The public package target runs the complete production shader suite plus two clean,
independent static-runtime Release builds before archiving; their plugin bytes
must match each other and the plugin entering the archive exactly.
These are release-artifact requirements, not a standalone public acceptance
claim. Mark them passed only when the final ZIP path, checksum, and package
manifest are recorded for that specific release artifact.

## Live gates before public upload

Run these checks on both Skyrim SE 1.5.97 and the selected AE build. Use the
locked ENB archive for ENBSeries rows; record the exact Community Shaders and
Effects 11 versions for Effects 11 rows, with ENB absent.

The final integrated live matrix is not recorded here yet. Public upload remains
blocked until Performance, Balanced, and Cinematic are run and recorded, along
with the no-runtime/fail-closed path. An isolated main-menu shader compile smoke
is compile-smoke evidence only; without a recorded row and artifact, it is not
visual, gameplay, tier-integration, or upload-acceptance evidence.

| Host | Native runtime | Preset | Host version | SE 1.5.97 DB | SE result / evidence | AE build / DB | AE result / evidence |
|---|---|---|---|---|---|---|---|
| ENBSeries | Enabled | Performance | locked 0.504 | `version-1-5-97-0.bin` | Pending / TBD | build TBD / DB TBD | Pending / TBD |
| ENBSeries | Enabled | Balanced | locked 0.504 | `version-1-5-97-0.bin` | Pending / TBD | build TBD / DB TBD | Pending / TBD |
| ENBSeries | Enabled | Cinematic | locked 0.504 | `version-1-5-97-0.bin` | Pending / TBD | build TBD / DB TBD | Pending / TBD |
| ENBSeries | Removed | Balanced fail-closed | locked 0.504 | N/A | Pending / TBD | build TBD / N/A | Pending / TBD |
| Effects 11 (partial optical/post) | Inactive by design | Performance | version TBD | N/A | Pending / TBD | build TBD / N/A | Pending / TBD |
| Effects 11 (partial optical/post) | Inactive by design | Balanced | version TBD | N/A | Pending / TBD | build TBD / N/A | Pending / TBD |
| Effects 11 (partial optical/post) | Inactive by design | Cinematic | version TBD | N/A | Pending / TBD | build TBD / N/A | Pending / TBD |

1. Confirm the native plugin resolves the host and exact Address Library file.
2. Confirm all seven hidden runtime values can be read and written only inside
   ENB callbacks, with celestial committed before `Status.valid`.
3. Rotate, pitch, translate, change FOV, enter interiors, open menus, and load
   saves; the world-space sky must remain stable and interior/depth masks must
   preserve the original scene.
4. Save configuration, reload the effect repeatedly, reset the display, and
   exit. Authored defaults must never be replaced by runtime matrices, including
   after an injected mid-transaction write/rollback failure.
5. Remove or rename one shader parameter and one Address Library database in
   separate runs. Both must fail closed without a partial valid payload.
6. If Horizon Fix is present in a compatibility load order, verify water skirt,
   ENB LOD shadows, horizon fog, map suppression, save/load, and exterior to
   interior transitions. Truth must remain functional without it.

Public upload stays blocked until these live gates pass. The repository license
is MIT; the optional GPL-3.0-or-later sky-mesh tool is excluded from the
runtime ZIP.
