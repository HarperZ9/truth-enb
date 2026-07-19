# Credits and provenance

Truth ENB is an independently authored MIT-licensed implementation. Its
runtime ZIP contains Truth source, generated configuration, the Truth runtime
plugin, and the exact ENB 0.504 vanilla fallback source required by ENB's
reserved `ORIGINALPOSTPROCESS` technique. It does not redistribute ENB
binaries, Bethesda assets, or peer preset source.

## Platform and community lineage

- Boris Vorontsov — ENBSeries, the ENB shader/runtime interface, and the
  upstream vanilla fallback retained byte-for-byte.
- Kitsuune / LonelyKitsuune — interoperability context for multi-stage ENB
  shader layouts. Truth compatibility is independently authored.
- kingeric1992, Adyss, TreyM, l00ping, TheSandvichMaker / ReforgedUI, and
  Marty McFly / Pascal Gilcher — prior ENB and real-time rendering work that
  informed the problem space and quality bar. No source from those projects is
  included in Truth's implementation.

## Scientific technique lineage

Truth's atmosphere design is informed by Sébastien Hillaire's production
atmosphere work. The aurora factorization follows the physical organization
described by Orion Lawlor and John Genetti. Common real-time techniques such
as Beer–Lambert extinction, Henyey–Greenstein phase functions, filmic tone
mapping, circle-of-confusion depth of field, bloom, and screen-space
confidence rejection are independently implemented and bounded for ENB's
fixed render stages.

The immutable upstream hashes used for verification are recorded in
`enb-upstream.lock`; the runtime-core revision is recorded in
`enb-runtime-core.lock`.
