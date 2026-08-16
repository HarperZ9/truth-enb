# Third-party notices

## ENBSeries

ENBSeries is authored by Boris Vorontsov and is not distributed with Truth
ENB. Users obtain ENB separately from its official distribution site.

Truth does not redistribute ENB binaries or upstream shader source. The
`enb-upstream.lock` file is a source/version evidence ledger for manually
checking silent upstream replacement. Truth's reserved
`ORIGINALPOSTPROCESS` technique uses an independently authored scene-color
identity fallback and is not described as the upstream vanilla post-process.

## enb-runtime-core

Truth links the separately maintained MIT-licensed `enb-runtime-core`. The
admitted source revision is recorded in `enb-runtime-core.lock`.

## Optional sky-mesh generator

`tools/sky-mesh` is a separate GPL-3.0-or-later build tool because it can link
against the external GPLv3 nifly serializer. Neither the tool, nifly, nor a
generated mesh is included in the Truth ENB runtime ZIP.

No ENB DLL, SKSE binary, Address Library database, Bethesda asset, protected
artifact, executable tool, or debugging symbol is included in the runtime ZIP.
