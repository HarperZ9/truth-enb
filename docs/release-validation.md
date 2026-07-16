# Truth ENB release validation

Truth supports ENBSeries as a peer; it never redistributes ENB binaries. Because
Skyrim SE builds retain the `0.504` version number across silent updates, the
private release candidate records the exact official upstream bytes used for
development in `enb-upstream.lock`.

On 2026-07-14 America/Los_Angeles, the current archive was downloaded directly
from `http://enbdev.com/enbseries_skyrimse_v0504.zip`. Its SHA-256 was
`f8cc7b824c18736195d461d099cdd791f09789e6da7106962dde4b8a12d06e78`.
All 30 extracted files were byte-identical to the protected current-source
snapshot used by the automated contract tests. The wrapper reports file and
product version `0.5.0.4`; exact component hashes are in the lock file.

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
- `https://www.nexusmods.com/skyrimspecialedition/mods/184607`

## Automated gates

The Release build must pass the CPU suites, optimized D3D11 WARP production
pixel test, exact ENB fallback contract, strict FXC permutations, static shader
budget, runtime plugin binary/ABI tests, and deterministic install/ZIP manifest.
The private package target runs the production shader subset plus two clean,
independent static-runtime Release builds before archiving; their plugin bytes
must match each other and the plugin entering the archive exactly.

## Live gates before public upload

Run these checks on both Skyrim SE 1.5.97 and the supported AE build using the
locked ENB archive:

1. Confirm the native plugin resolves the host and exact Address Library file.
2. Confirm all six hidden runtime values can be read and written only inside
   ENB callbacks, with `Status.valid` committed last.
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

Public upload stays blocked until these live gates pass and a public repository
license is selected.
