#!/usr/bin/env bash
# Regenerates firmware/src/font_sans_*.c from assets/fonts/. Needs lv_font_conv
# (npm i -g lv_font_conv) and python3. See docs/fonts.md.
set -euo pipefail
cd "$(dirname "$0")/.."

FONTS=assets/fonts/anthropic-sans
OUT=firmware/src
ASCII=0x20-0x7E,0x2026
# Spinner glyphs for the status line, from DejaVu Sans Mono so every frame has
# the same width and the text next to it doesn't shift. The check mark for a
# finished task comes from there too.
SPIN=0xB7,0x2714,0x2722,0x2733,0x2736,0x273B,0x273D

# name  size  cut  weight  spinner
FONT_LIST="
sans_56          56 Display Regular  0
sans_48          48 Display Regular  0
sans_34          34 Display Regular  0
sans_28          28 Display Regular  1
sans_24          24 Text    Regular  0
sans_20          20 Text    Regular  0
sans_16          16 Text    Regular  1
sans_14          14 Text    Regular  0
sans_12          12 Text    Regular  0
sans_semibold_20 20 Text    Semibold 0
sans_semibold_16 16 Text    Semibold 0
sans_semibold_14 14 Text    Semibold 0
"

echo "$FONT_LIST" | while read -r name size cut weight spin; do
  [ -z "$name" ] && continue
  extra=()
  [ "$spin" = 1 ] && extra=(--font assets/fonts/DejaVuSansMono.ttf -r "$SPIN")
  out="$OUT/font_$name.c"
  lv_font_conv --font "$FONTS/AnthropicSans-$cut-$weight-Static.otf" -r "$ASCII" \
    ${extra[@]+"${extra[@]}"} --size "$size" --format lvgl --bpp 4 --no-compress \
    -o "$out" --lv-include lvgl.h
  # lv_font_conv emits LVGL 8 version guards; rewrite the descriptors for LVGL 9.
  python3 - "$out" <<'EOF'
import re, sys
p = sys.argv[1]
s = open(p).read()
s = re.sub(r'#if LVGL_VERSION_MAJOR == 8\n/\*Store all the custom data of the font\*/\nstatic\s+lv_font_fmt_txt_glyph_cache_t cache;\n#endif\n',
           '', s)
s = re.sub(r'#if LVGL_VERSION_MAJOR >= 8\n(static const lv_font_fmt_txt_dsc_t font_dsc = \{)\n#else\n.*?\n#endif\n',
           r'\1\n', s)
s = re.sub(r',\n#if LVGL_VERSION_MAJOR == 8\n    \.cache = &cache\n#endif\n', '\n', s)
s = re.sub(r'#if LVGL_VERSION_MAJOR >= 8\n(const lv_font_t \w+ = \{)\n#else\n.*?\n#endif\n', r'\1\n', s)
s = re.sub(r'#if [^\n]*\n(    \.(?:subpx|underline_position|underline_thickness|fallback)[^\n]*\n)', r'\1', s)
s = re.sub(r'(    \.(?:subpx|underline_thickness|fallback)[^\n]*\n)#endif\n', r'\1', s)
s = s.replace('    .line_height', '    .release_glyph = NULL,\n    .line_height')
s = s.replace('    .dsc = &font_dsc,', '    .kerning = 0,\n    .static_bitmap = 0,\n    .dsc = &font_dsc,')
assert 'LVGL_VERSION' not in s.split('ALL CUSTOM DATA')[1].replace('LV_LVGL_H_INCLUDE_SIMPLE', ''), p
open(p, 'w').write(s)
EOF
  echo "$out"
done
