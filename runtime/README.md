# Truth ENB native camera bridge

This subtree builds Truth's original, standalone ENB external plugin. It binds
live Skyrim camera data to the Truth effect without SKSE or CommonLib. The only
installed peers are ENBSeries and the Address Library database already used by
the Skyrim ecosystem.

`enb-runtime-core` is consumed as a configurable static build-time sibling via
`TRUTH_ENB_RUNTIME_CORE_ROOT`. Its clean checkout must match the exact revision
recorded by `enb-runtime-core.lock`; it does not become a second runtime DLL.

## Runtime contract

The plugin:

- resolves an already-loaded ENB SDK v1002-compatible Skyrim SE host;
- accepts only `SkyrimSE.exe` x64, exactly SE 1.5.97 or AE 1.6.x;
- opens only the runtime-specific database under `Data/SKSE/Plugins`: legacy
  SE 1.5.97 uses `version-1-5-97-0.bin`, while AE 1.6.x uses
  `versionlib-<major>-<minor>-<patch>-<build>.bin`;
- requires the database header to name `SkyrimSE.exe` exactly before any
  relocation can be admitted;
- resolves `WorldRootCamera` through Address Library ID 35601 on SE 1.5.97 or
  ID 36609 on AE 1.6.x;
- reads the non-VR `NiCamera` ABI at `0x110` (`worldToCam`) and `0x150`
  (`NiFrustum` plus viewport);
- validates the affine view basis, frustum, viewport, memory ranges, and matrix
  inversions before publishing data;
- transposes the inverse of the row-vector engine `View * Projection` result so
  the published rows are the exact row-major matrix consumed by Truth's
  `mul(inverse_view_projection, clip_column)` shader contract.

The ENB SDK category is `ENBEFFECT.FX`. Every value is SDK `COLOR4`, addressed
by UI name rather than by its HLSL identifier:

1. `Truth Runtime | Inverse VP Row 0`
2. `Truth Runtime | Inverse VP Row 1`
3. `Truth Runtime | Inverse VP Row 2`
4. `Truth Runtime | Inverse VP Row 3`
5. `Truth Runtime | Camera World`
6. `Truth Runtime | Status`

Status is `(protocol, valid, folded_generation, world_scale)`. Protocol is
`1.0`; valid is exactly `0.0` or `1.0`; generation is folded modulo `2^24` so
it remains an exact IEEE-754 integer; and the default scale is `4096.0` engine
units per aurora unit.

All `ENBGetParameter` and `ENBSetParameter` calls are mechanically gated to an
ENB callback scope. Every publication writes invalid Status first, the five
camera payload values next, and the target Status last, so partial camera data
can never remain marked valid. `PostLoad` captures the current effect's shader
defaults; repeated `PostLoad` callbacks recapture and rebind a recreated effect
without restoring defaults from the previous effect. `PreSave`, `PreReset`, and
`OnExit` restore the current baseline before ENB can persist or destroy the
parameters. A live transaction failure disables ordinary frame writes and
starts a best-effort full rollback. If rollback fails, its restore-needed state
is retained and each lifecycle barrier retries it; a final valid Status is not
published over a partial restore.

## Build and verify

From this directory:

```powershell
cmake --preset vs18-x64-static
cmake --build --preset vs18-x64-static-debug
ctest --preset vs18-x64-static-debug --output-on-failure
cmake --build --preset vs18-x64-static-release
ctest --preset vs18-x64-static-release --output-on-failure
```

The test suite includes a Release reproducibility gate. It deletes two owned
build roots, independently configures and builds the static-CRT plugin in each,
then requires the two `TruthENBRuntime.dllplugin` files to be byte-identical.
The compiler and linker inputs that contribute to the shipped plugin use
MSVC's deterministic `/Brepro` mode.

The release artifact is
`out/build/vs18-x64-static/Release/TruthENBRuntime.dllplugin`. Install it under
the Skyrim `enbseries` directory so ENB's external-plugin loader discovers it.
Do not ship the static libraries, test executables, PDBs, or build tree.

## Diagnostics

The plugin exports `TruthEnbRuntimeGetDiagnosticsV1`. It fills the fixed,
128-byte `PluginDiagnosticsV1` ABI and reports host resolution, native camera
admission, Address Library parsing, shader binding, lifecycle state, callback
count, generation, rollback failures, and the selected relocation. Publication
is lock-free and generation-consistent for external diagnostic readers.

## Required in-game release checks

Automated tests cannot prove ENB's live shader registry or the current Skyrim
camera ABI. Before shipping, test both SE 1.5.97 and the supported AE build with
ENB 0.504:

1. Confirm the plugin loads from `enbseries` and reports host resolved.
2. Confirm the exact runtime Address Library filename exists and the native
   camera capability reports ready.
3. Rotate, pitch, translate, change FOV, enter interiors, open menus, and load a
   save while checking that world-space sky motion remains stable.
4. Use Save Configuration, reload the effect, and verify the six authored
   defaults were not replaced by a runtime matrix.
5. Trigger display reset/fullscreen changes and verify `PreReset`/`PostReset`
   recovery.
6. Force one missing/wrong parameter and one missing database run; both must
   show `valid=0` or retain authored defaults without partial live data.

The official SDK v1002 header says hidden shader variables may be rejected by
`ENBGetParameter` and `ENBSetParameter`. Therefore `UIHidden=1` on these six
variables is an explicit in-game compatibility gate for ENB 0.504. If 0.504
follows that documented behavior, keep the parameters SDK-addressable and hide
them through an editor presentation mechanism that does not mark the variables
hidden. The bridge intentionally fails closed if the hidden-variable experiment
is rejected.
