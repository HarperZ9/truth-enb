# Third-party notices

## ENBSeries

ENBSeries is authored by Boris Vorontsov and is not distributed with Truth
ENB. Users obtain ENB separately from its official distribution site.

`Root/enbseries/enb/ENBSeries0504VanillaPostProcess.fxh` is the exact upstream
fallback source required for ENB 0.504's reserved vanilla post-process
technique. Truth keeps it isolated and hash-locked; Truth-owned rendering does
not substitute code under the reserved technique name.

## enb-runtime-core

Truth links the separately maintained MIT-licensed `enb-runtime-core`. The
admitted source revision is recorded in `enb-runtime-core.lock`.

## Optional sky-mesh generator

`tools/sky-mesh` is a separate GPL-3.0-or-later build tool because it can link
against the external GPLv3 nifly serializer. Neither the tool, nifly, nor a
generated mesh is included in the Truth ENB runtime ZIP.

No ENB DLL, SKSE binary, Address Library database, Bethesda asset, protected
artifact, executable tool, or debugging symbol is included in the runtime ZIP.
