# Truth Sky Mesh Generator

This optional standalone tool generates Truth's original Skyrim Special Edition
atmosphere fallback. It does not copy or transform a Bethesda, Picta, or other
third-party mesh. The result is one welded, inward-facing Icosphere L4 with
2,562 vertices, 5,120 triangles, a 500-unit radius, no UV channel, deterministic
elevation weights, and a `BSSkyShaderProperty` configured as sky type 2.

The tool is intentionally isolated from Truth's top-level build. It requires an
external nifly source checkout only while compiling; it never downloads, vendors,
installs, or adds a runtime dependency on nifly. Configuration fails clearly if
the checkout or its Git provenance is unavailable.

## Build, generate, and test

From the repository root, the verified Windows build is:

```powershell
cmake -S tools/sky-mesh -B build/sky-mesh -G "Visual Studio 17 2022" -A x64 `
  -DNIFLY_SOURCE_DIR="$env:NIFLY_SOURCE_DIR" `
  -DTRUTH_SKY_MESH_INSPECTOR="$env:TRUTH_SKY_MESH_INSPECTOR"
cmake --build build/sky-mesh --config Release --target truth_generate_atmosphere --parallel 8
cmake --build build/sky-mesh --config Release --target truth_sky_mesh_contract_tests --parallel 8
ctest --test-dir build/sky-mesh -C Release --output-on-failure
```

`NIFLY_SOURCE_DIR` and `TRUTH_SKY_MESH_INSPECTOR` are caller-supplied cache
locations, not source defaults. The inspector is optional; when supplied, it adds
an independent serialized-NIF contract test.

The product outputs remain under the ignored build tree:

```text
build/sky-mesh/generated/meshes/sky/atmosphere.nif
build/sky-mesh/generated/truth-atmosphere-mesh.manifest.json
```

The manifest records the generated NIF's SHA-256, byte size, topology, format
version, quantization, generator license, full nifly revision, and the fact that
no source template was used. No generated NIF is tracked by Git.

An OBJ is diagnostic-only and opt-in:

```powershell
build/sky-mesh/Release/truth_sky_mesh_generate.exe `
  --output-root build/sky-mesh/manual --emit-obj
```

## Determinism and validation

The contract suite parses the NIF back and verifies its header and three-block
layout, shader class and flags, topology, radius and bounds, welded manifold,
inward winding and packed normals, finite quantized values, elevation-driven
color/alpha ramp, lack of UVs and degenerates, manifest digest, optional OBJ,
invalid-CLI behavior, and byte identity across two clean generations.

The serializer deliberately creates `NiNode`, `BSTriShape`, and
`BSSkyShaderProperty` blocks from new data. It fails instead of falling back to a
copied NIF template if those blocks cannot be serialized and read back faithfully.

## Licensing boundary

Source code in `tools/sky-mesh` is licensed under GPL-3.0-or-later so it can link
to the GPLv3 nifly serializer. nifly remains an external build-time dependency;
its revision and role are recorded in every manifest.

The generated geometry is original numeric output from Truth's deterministic
Icosphere algorithm and contains no copied peer or Bethesda geometry. The
repository has not yet selected an overall asset license, so the manifest states
that fact rather than inventing one. Selecting the generated asset's distribution
terms remains a separate project release decision.
