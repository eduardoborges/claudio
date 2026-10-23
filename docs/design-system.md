# Design system

The UI follows Anthropic's design system. The spec is in
`assets/design/anthropic.DESIGN.md` and is written for the web, on a light
page. The device has an AMOLED panel, so it uses the dark surface from the
spec's frontmatter (`surface #000000`, `on-surface #FAF9F5`): black pixels stay
off, and cards sit one step above the floor.

The tokens live in `firmware/src/theme.h`. The one place that repeats raw hex
values is the recolor string on the enterprise "Period" line in `ui.cpp`; keep
it in sync when a token changes.

## Colors

| Token | Hex | Spec role | Used for |
|---|---|---|---|
| `THEME_BG` | `#000000` | surface | screen floor |
| `THEME_PANEL` | `#141413` | elevated section (dark) | usage cards |
| `THEME_TEXT` | `#FAF9F5` | on-surface | primary text |
| `THEME_DIM` | `#B0AEA5` | secondary text | reset times, labels, finished sessions |
| `THEME_ACCENT` | `#D97757` | primary | bars, status line, heatmap, sessions that need you |
| `THEME_ALERT` | `#C6613F` | earth accent | a bar at 80% or more, "Over pace" |
| `THEME_SAGE` | `#788C5D` | sage accent | the check on sessions that finished in the last minute, and under the dancing Clawd |
| `THEME_BADGE` | `#E3DACC` | secondary | badge fill ("Current", "Weekly", the plan) |
| `THEME_ON_BADGE` | `#141413` | neutral 0 | badge text |
| `THEME_LINE` | `#343431` | neutral dark at 34% | bar tracks, card borders, dividers, empty heatmap cells |

`THEME_LINE` is `#73726C` at the spec's 34% opacity over the card color,
pre-mixed into one opaque color.

## Type

Everything is set in Anthropic Sans with no letter spacing. The spec pairs it
with Anthropic Serif for headings, but there is no serif font file, so titles
and numbers use the sans Display cut. Sizes from 28px up use Display, smaller
ones use Text, and badges use Text Semibold (the spec's 600 weight). The
status-line spinner glyphs and the check mark come from DejaVu Sans Mono. To regenerate the fonts,
see `docs/fonts.md`.

| Role | Large (480) | Compact (368×448) | Small (240) |
|---|---|---|---|
| Title / clock | 56 | 56 | 34 |
| Usage percentage | 56 | 48 | 24 |
| Reset line | 28 | 28 | 14 |
| Status line | 28 | 28 | 16 |
| Badge | Semibold 20 | Semibold 16 | Semibold 14 |
| Stat value / label | 24 / 16 | 20 / 14 | 14 / 12 |

## Shape and spacing

- Cards have a 16px radius (8px on 240px screens) and a 1px `THEME_LINE`
  border. The spec prefers borders to shadows, and the UI has no shadows.
- Badges and bars use the spec's 4px micro radius. Heatmap cells use 2px.
- On the large and compact layouts spacing stays on the spec's scale: 20px
  screen margin, 16px by 12px card padding, 12px by 4px badge padding. The
  240px layout shrinks all of these.

## Where the device departs from the spec

- There is no serif, as above.
- The alert color is the spec's earth accent. It sits close to the primary
  terracotta, so a bar near its limit barely stands out from a normal one.
- The spec puts dark text on light surfaces. On the device that would light
  every pixel, so only the badges keep dark text on a light fill.
