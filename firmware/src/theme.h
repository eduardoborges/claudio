#pragma once
#include <lvgl.h>

// Design tokens from Anthropic's design system (assets/design/anthropic.DESIGN.md),
// on its dark surface: a true black floor, which leaves AMOLED pixels off.
#define THEME_BG       lv_color_hex(0x000000)   // surface
#define THEME_PANEL    lv_color_hex(0x141413)   // elevated section (dark): cards
#define THEME_TEXT     lv_color_hex(0xfaf9f5)   // on-surface
#define THEME_DIM      lv_color_hex(0xb0aea5)   // secondary text
#define THEME_ACCENT   lv_color_hex(0xd97757)   // primary: terracotta
#define THEME_ALERT    lv_color_hex(0xc6613f)   // earth accent: a limit close to running out
#define THEME_SAGE     lv_color_hex(0x788c5d)   // sage accent: a finished task's check
#define THEME_BADGE    lv_color_hex(0xe3dacc)   // secondary: badge fill
#define THEME_ON_BADGE lv_color_hex(0x141413)   // text on a badge
// Neutral dark (#73726c) at the system's 34% opacity over the panel, pre-mixed:
// bar tracks, card borders, dividers.
#define THEME_LINE     lv_color_hex(0x343431)
