# Credits and provenance

Truth ENB is an independently authored MIT-licensed implementation. Its
runtime ZIP contains Truth source, generated configuration, the Truth runtime
plugin, and a Truth-owned identity fallback under ENB's reserved
`ORIGINALPOSTPROCESS` technique name. It does not redistribute ENB binaries,
ENB/Bethesda shader source, Bethesda assets, or peer preset source.

## Platform and community lineage

- Boris Vorontsov — ENBSeries and the ENB shader/runtime interface.
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

Maxime Heckel's
[sky, sunset, and planet rendering article](https://blog.maximeheckel.com/posts/on-rendering-the-sky-sunsets-and-planets/)
was an accessible design reference for atmospheric scattering, celestial-disc
geometry, and ray/sphere reasoning. Truth's implementation, constants, stage
integration, and fallbacks are independently authored.

The upstream hashes retained as a manual provenance ledger are recorded in
`enb-upstream.lock`; the runtime-core source pin is recorded in
`enb-runtime-core.lock`.
