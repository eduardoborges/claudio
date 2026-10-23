# Recompiling fonts

The `firmware/src/font_sans_*.c` files are LVGL bitmap fonts made from
Anthropic Sans, the typeface of Anthropic's design system (see
`docs/design-system.md`). The source .otf files live in
`assets/fonts/anthropic-sans/`. To regenerate:

```bash
npm install -g lv_font_conv
tools/gen_fonts.sh
```

The script holds the list of sizes and cuts. The Display cut covers 28px and
up, the Text cut everything below, and badges use Text Semibold. `font_sans_28`
and `font_sans_16` also carry the status-line spinner glyphs and the check mark
for a finished task, taken from `assets/fonts/DejaVuSansMono.ttf` so every
spinner frame has the same width.

`lv_font_conv` 1.5.3 writes LVGL 8 descriptors, so the script rewrites each
file for LVGL 9: it drops the version guards and the `.cache` field, and adds
`.release_glyph`, `.kerning` and `.static_bitmap`. Without that patch the fonts
compile but render invisible.
