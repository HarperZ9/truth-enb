# Upstream contribution to Community Shaders

**Status:** accepted. Merged upstream 2026-08-19.

This records what was offered to Community Shaders, why, and what the answer
implies for the larger sky-feature work in W3. It is the cheap experiment that
runs before three shader PRs get written.

## The PR

<https://github.com/community-shaders/skyrim-community-shaders/pull/2663>

`feat: patch Truth ENB and Elder ENB preset settings`, against `dev`, from
`HarperZ9:feat/truth-and-elder-preset-settings-patches`. Base at the time was
`9215139`.

Data only. Three variable names added to
`features/Effects11/Shaders/Effects11/SettingsPatches.json`, taking it from 115
entries to 118:

| variable | shader object | why |
|---|---|---|
| `[Truth 70] Postpass \| Vignette Strength` | `enbeffectpostpass.fx` | vignette runs by default, duplicates a stage CS owns |
| `[Truth 70] Postpass \| Grain Shape` | `enbeffectpostpass.fx` | film grain runs by default, same |
| `GRADE \| Clarity Enable` | `enbeffect.fx` | unsharp mask, ships off, preventative |

Values were zeroed rather than the stage disabled, because Truth's postpass also
applies triangular dither and nothing else in the chain supplies it. Switching
the stage off removes the dither and bands a graded sky. This is the same reason
`TruthPostpassVignetteStrength` had to become a uniform in the first place.

## What the answer decides

**Accepted.** Evidence that contributions from outside the core team land, and
that the maintainers are willing to carry preset-specific data. W3 Task 1 then
opens the Discord conversation about the sky feature with a merged PR already in
hand, which is a materially better position than a cold approach.

**Rejected, or ignored past a reasonable window.** Evidence that three shader
PRs totalling 1,415 lines are not worth writing. The sky work stays in truth-enb
under MIT, the Effects 11 preset variant carries the fix locally for anyone who
installs Truth, and W3 does not run.

**Accepted with changes.** Whatever they ask for overrides the W3 plan, since
they know their review standards better than the plan does.

## Response

Accepted, without changes. doodlum approved on 2026-08-18 and the PR merged to
`dev` on 2026-08-19, four days after it was raised. The single data-only commit
landed as submitted. Neither the maintainer review nor the repository's
automated review left an actionable comment.

Per the pre-registered interpretation above, this is the accepted outcome. It
is evidence that outside contributions land and that the maintainers will carry
preset-specific data. W3 Task 1 opens the Discord conversation about the sky
feature with this merged PR in hand. Truth and Elder presets now receive
correct default behaviour under Effects 11 from the next Community Shaders
release, independent of Truth's own Effects 11 preset variant.
