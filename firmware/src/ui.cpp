#include "ui.h"
#include "splash.h"
#include <lvgl.h>
#include <time.h>
#include "logo.h"
#include "clawd_still.h"
#include "icons.h"
#include "hal/board_caps.h"

// Anthropic Sans: Display cut from 28px up, Text below. Regenerate with
// tools/gen_fonts.sh.
LV_FONT_DECLARE(font_sans_56);
LV_FONT_DECLARE(font_sans_48);
LV_FONT_DECLARE(font_sans_34);
LV_FONT_DECLARE(font_sans_28);   // + spinner glyphs
LV_FONT_DECLARE(font_sans_24);
LV_FONT_DECLARE(font_sans_20);
LV_FONT_DECLARE(font_sans_16);   // + spinner glyphs
LV_FONT_DECLARE(font_sans_14);
LV_FONT_DECLARE(font_sans_12);
LV_FONT_DECLARE(font_sans_semibold_20);   // badges
LV_FONT_DECLARE(font_sans_semibold_16);
LV_FONT_DECLARE(font_sans_semibold_14);

// Layout values computed from the active board's geometry. Populated once
// in ui_init() and treated as const for the rest of the program. Adding a
// new display size means extending compute_layout() with another
// breakpoint — never editing the screen-builder functions below.
struct Layout {
    int16_t scr_w, scr_h;
    int16_t margin;
    int16_t title_y;
    int16_t content_y;
    int16_t content_w;

    // Usage screen
    int16_t usage_panel_h;
    int16_t usage_panel_gap;
    int16_t usage_bar_y;
    int16_t usage_reset_y;
    int16_t bar_h;
    int16_t panel_pad_x, panel_pad_y;
    int16_t pill_pad_x, pill_pad_y;
    int16_t radius;                  // card corners
    const lv_font_t* title_font;     // screen title / clock
    const lv_font_t* pct_font;       // big percentage number
    const lv_font_t* ent_pct_font;   // enterprise spending number
    const lv_font_t* pill_font;      // "Current" / "Weekly" pill
    const lv_font_t* reset_font;     // "Resets in ..." line
    const lv_font_t* pace_font;      // enterprise "Under/On/Over pace" line
    const lv_font_t* anim_font;      // animated status line
    int16_t anim_y;                  // status line offset from bottom
    bool    small_icons;             // 40px logo + 24px battery (vs 80/48) on small screens
    int16_t title_nudge;             // title x-shift balancing the corner logo
    int16_t logo_y;                  // logo top edge
    int16_t batt_y;                  // battery icon top edge
    int16_t batt_w;                  // battery icon width, for position math

    // Pairing hint / idle screen
    int16_t pair_y1, pair_y2, pair_y3;
    int16_t idle_px;                 // sleeping-creature size on the idle screen

    // Bluetooth screen
    int16_t bt_info_panel_h;
    int16_t bt_reset_zone_h;
    const lv_font_t* bt_title_font;
    const lv_font_t* bt_status_font;
    const lv_font_t* bt_device_font;
    const lv_font_t* bt_credit_1_font;
    const lv_font_t* bt_credit_2_font;

    // Stats workspace
    const lv_font_t* stat_label_font;   // heatmap axis, legend and stat names
    const lv_font_t* stat_value_font;   // stat numbers
    const lv_font_t* use_font;          // Sessions usage strip
    uint8_t stat_rows;                  // rows of the two-column stat grid
};
static Layout L = {};

// Pick layout values from the active board's pixel dimensions. The two
// existing boards happen to land on the two breakpoints below; new ports
// inherit the closer one — visually OK, may need a polish pass for
// pixel-perfect alignment but never blocks the port from booting.
static void compute_layout(const BoardCaps& c) {
    L.scr_w = c.width;
    L.scr_h = c.height;
    L.margin = 20;
    L.title_y = 30;

    // Values shared by the two original breakpoints; the small branch below
    // overrides them wholesale.
    L.bar_h = 24;
    L.panel_pad_x = 16;
    L.panel_pad_y = 12;
    L.pill_pad_x = 12;
    L.pill_pad_y = 4;
    L.radius = 16;
    L.title_font   = &font_sans_34;
    L.pct_font     = &font_sans_56;
    L.ent_pct_font = &font_sans_56;
    L.pill_font    = &font_sans_semibold_20;
    L.reset_font   = &font_sans_28;
    L.pace_font    = &font_sans_16;
    L.anim_font    = &font_sans_28;
    L.anim_y = -15;
    L.small_icons = false;
    L.title_nudge = 16;
    L.logo_y = L.title_y - 10;
    L.batt_y = L.title_y;
    L.batt_w = ICON_BATTERY_W;
    L.pair_y1 = 40;
    L.pair_y2 = 120;
    L.pair_y3 = 160;
    L.idle_px = 160;
    L.stat_label_font = &font_sans_16;
    L.stat_value_font = &font_sans_24;
    L.use_font = &font_sans_20;
    L.stat_rows = 3;

    if (c.height >= 460) {
        // Large layout — tuned for 480x480 (AMOLED-2.16).
        L.content_y = 100;
        L.usage_panel_h = 150;
        L.usage_panel_gap = 16;
        L.usage_bar_y = 56;
        L.usage_reset_y = 94;
        L.bt_info_panel_h = 160;
        L.bt_reset_zone_h = 110;
        L.bt_title_font    = &font_sans_56;
        L.bt_status_font   = &font_sans_48;
        L.bt_device_font   = &font_sans_28;
        L.bt_credit_1_font = &font_sans_24;
        L.bt_credit_2_font = &font_sans_20;
    } else if (c.height >= 300) {
        // Compact layout — tuned for 368x448 (AMOLED-1.8).
        L.content_y = 85;
        L.usage_panel_h = 130;
        L.usage_panel_gap = 12;
        L.usage_bar_y = 48;
        L.usage_reset_y = 78;
        L.bt_info_panel_h = 140;
        L.bt_reset_zone_h = 90;
        L.bt_title_font    = &font_sans_34;
        L.bt_status_font   = &font_sans_28;
        L.bt_device_font   = &font_sans_20;
        L.bt_credit_1_font = &font_sans_16;
        L.bt_credit_2_font = &font_sans_14;
        L.stat_label_font = &font_sans_14;
        L.stat_value_font = &font_sans_20;
        L.use_font = &font_sans_16;
        L.pct_font  = &font_sans_48;
        L.pill_font = &font_sans_semibold_16;
    } else {
        // Small layout — tuned for 240x240 (LCD-1.54 and similar square TFTs).
        // Everything shrinks: fonts two steps down, panels ~half height, and
        // the corner logo/battery switch to the 40px/24px small assets.
        L.margin = 8;
        L.title_y = 4;
        L.content_y = 44;
        L.usage_panel_h = 74;
        L.usage_panel_gap = 6;
        L.usage_bar_y = 30;
        L.usage_reset_y = 46;
        L.bar_h = 12;
        L.panel_pad_x = 10;
        L.panel_pad_y = 6;
        L.pill_pad_x = 8;
        L.pill_pad_y = 2;
        L.radius = 8;
        L.title_font   = &font_sans_20;
        L.pct_font     = &font_sans_24;
        L.ent_pct_font = &font_sans_34;
        L.pill_font    = &font_sans_semibold_14;
        L.reset_font   = &font_sans_14;
        L.pace_font    = &font_sans_12;
        L.anim_font    = &font_sans_16;
        // Center the status line in the strip below the weekly panel; flush
        // against the bottom edge it reads as unevenly spaced.
        L.anim_y = -10;
        L.small_icons = true;
        L.title_nudge = 8;
        L.logo_y = 2;
        L.batt_y = 10;
        L.batt_w = ICON_BATTERY_SMALL_W;
        L.pair_y1 = 12;
        L.pair_y2 = 56;
        L.pair_y3 = 80;
        L.idle_px = 96;
        L.bt_info_panel_h = 90;
        L.bt_reset_zone_h = 60;
        L.bt_title_font    = &font_sans_34;
        L.bt_status_font   = &font_sans_20;
        L.bt_device_font   = &font_sans_14;
        L.bt_credit_1_font = &font_sans_12;
        L.bt_credit_2_font = &font_sans_12;
        L.stat_label_font = &font_sans_12;
        L.stat_value_font = &font_sans_14;
        L.use_font = &font_sans_14;
        L.stat_rows = 2;
    }

    L.content_w = L.scr_w - 2 * L.margin;
}

// Anthropic design system palette — design tokens live in theme.h
#include "theme.h"
#define COL_BG        THEME_BG
#define COL_PANEL     THEME_PANEL
#define COL_TEXT      THEME_TEXT
#define COL_DIM       THEME_DIM
#define COL_ACCENT    THEME_ACCENT
#define COL_ALERT     THEME_ALERT
#define COL_LINE      THEME_LINE
#define COL_DONE      THEME_SAGE

// ---- Usage screen widgets (single non-splash view) ----
static lv_obj_t* usage_container;
static lv_obj_t* lbl_title;
// Clock fed by the daemon: base epoch (local wall-clock seconds) + the lv_tick at
// which it landed, so the title ticks forward locally between 60s payloads.
static long     clock_base_epoch = 0;
static uint32_t clock_base_ms = 0;
static int      clock_fmt = 24;   // 12 or 24, set from the daemon payload
static int      clock_last_min = -1;   // last rendered minute; avoids redrawing the title every tick
static lv_obj_t* usage_group;   // the two usage panels — shown when connected
static lv_obj_t* pair_group;    // pairing hint — shown when disconnected
static lv_obj_t* bar_session;
static lv_obj_t* lbl_session_pct;
static lv_obj_t* lbl_session_label;
static lv_obj_t* lbl_session_reset;
static lv_obj_t* bar_weekly;
static lv_obj_t* lbl_weekly_pct;
static lv_obj_t* lbl_weekly_label;
static lv_obj_t* lbl_weekly_reset;
static lv_obj_t* lbl_plan;   // plan pill, left of the Weekly pill
static lv_obj_t* panel_session = nullptr;
static lv_obj_t* panel_weekly = nullptr;
// Enterprise-only widgets inside panel_session
static lv_obj_t* lbl_session_pct_sym = nullptr;  // "%" in smaller font
static lv_obj_t* lbl_spending_desc = nullptr;     // "of your monthly budget"
static lv_obj_t* lbl_spending_status = nullptr;   // "Under pace" / "On pace" / "Over pace"
static lv_obj_t* lbl_anim;      // status line: connection state + whimsical idle
static lv_obj_t* done_box;      // celebration: check + finished task, over the status line
static lv_obj_t* lbl_done_task;
static int32_t   done_task_max_w;

// ---- Battery indicator (shared, on top) ----
static lv_obj_t* battery_img;
static lv_obj_t* logo_img;
static lv_image_dsc_t battery_dscs[6];  // empty, low, medium, full, charging, plug-zap (no battery)

// ---- Live-data freshness → which usage sub-view to show ----
// usage panels when data is flowing, the pairing hint when nothing is bonded,
// and a waiting screen with a creature for everything in between (connecting,
// loading, no data within DATA_FRESH_MS). Re-evaluated every loop in ui_tick_anim().
static lv_obj_t* idle_group;            // the waiting screen
static uint32_t  last_data_ms = 0;      // lv_tick when the last valid usage update landed
static bool      data_received = false; // any valid update since boot
static bool      data_ok = true;        // last payload's ok flag; a {"ok":false} beat = "no fresh data"
static int       view_state = -1;       // -1 unknown / 0 pair / 1 waiting / 2 usage
static bool      beat_since_connect = false;  // any payload, ok or not, since BLE connected
static uint32_t  disconnected_at_ms = 0;      // boot counts as a disconnect
static const uint32_t CONNECT_GRACE_MS = 30000;  // advertising this long with no host = not connected

// What the device is waiting on. The status line names it and the creature on
// the waiting screen jumps while something is on its way, sleeps when not.
enum Link { LINK_UNPAIRED, LINK_CONNECTING, LINK_OFFLINE, LINK_LOADING, LINK_NO_DATA, LINK_LIVE };
static const uint32_t DATA_FRESH_MS = 90000;  // usage counts as "live" within this window (daemon sends ~60s)

// ---- Shared ----
static lv_image_dsc_t logo_dsc;
static screen_t current_screen = SCREEN_USAGE;
static bool     s_ble_connected = false;   // cached BLE connection state
// Claude Code is working. True until the daemon's first sessions message, so a
// host without the hooks always shows the busy verbs.
static bool s_working = true;
static uint32_t connected_at_ms = 0;       // when we last entered CONNECTED ("Connected" dwell)

// Animation state
static uint32_t anim_last_ms = 0;
static uint8_t anim_spinner_idx = 0;
static uint8_t anim_phase = 0;
static uint8_t anim_msg_idx = 0;
static uint32_t anim_msg_start = 0;
#define ANIM_MSG_MS     4000

static const char* const spinner_frames[] = {
    "\xC2\xB7", "\xE2\x9C\xBB", "\xE2\x9C\xBD",
    "\xE2\x9C\xB6", "\xE2\x9C\xB3", "\xE2\x9C\xA2",
};
static const char* const CHECK = "\xE2\x9C\x94";   // a finished task, in the spinner's fonts
#define SPINNER_COUNT 6
#define SPINNER_PHASES (2 * (SPINNER_COUNT - 1))  // 10: ping-pong 0..5..0

static const uint16_t spinner_ms[SPINNER_COUNT] = {
    260, 130, 130, 130, 130, 260,
};

static const char* const anim_messages[] = {
    "Accomplishing", "Elucidating", "Perusing",
    "Actioning", "Enchanting", "Philosophising",
    "Actualizing", "Envisioning", "Pondering",
    "Baking", "Finagling", "Pontificating",
    "Booping", "Flibbertigibbeting", "Processing",
    "Brewing", "Forging", "Puttering",
    "Calculating", "Forming", "Puzzling",
    "Cerebrating", "Frolicking", "Reticulating",
    "Channelling", "Generating", "Ruminating",
    "Churning", "Germinating", "Scheming",
    "Clauding", "Hatching", "Schlepping",
    "Coalescing", "Herding", "Shimmying",
    "Cogitating", "Honking", "Shucking",
    "Combobulating", "Hustling", "Simmering",
    "Computing", "Ideating", "Smooshing",
    "Concocting", "Imagining", "Spelunking",
    "Conjuring", "Incubating", "Spinning",
    "Considering", "Inferring", "Stewing",
    "Contemplating", "Jiving", "Sussing",
    "Cooking", "Manifesting", "Synthesizing",
    "Crafting", "Marinating", "Thinking",
    "Creating", "Meandering", "Tinkering",
    "Crunching", "Moseying", "Transmuting",
    "Deciphering", "Mulling", "Unfurling",
    "Deliberating", "Mustering", "Unravelling",
    "Determining", "Musing", "Vibing",
    "Discombobulating", "Noodling", "Wandering",
    "Divining", "Percolating", "Whirring",
    "Doing", "Wibbling",
    "Effecting", "Wizarding",
    "Working", "Wrangling",
};
#define ANIM_MSG_COUNT (sizeof(anim_messages) / sizeof(anim_messages[0]))

static lv_color_t pct_color(float pct) {
    return pct >= 80.0f ? COL_ALERT : COL_ACCENT;
}

// Wall-clock time `secs_ahead` from now, in the daemon's hour format. Only
// meaningful once the daemon has sent the clock (clock_base_epoch > 0).
static void format_clock(long secs_ahead, char* buf, size_t len) {
    time_t t = (time_t)(clock_base_epoch + (lv_tick_get() - clock_base_ms) / 1000 + secs_ahead);
    struct tm tmv;
    gmtime_r(&t, &tmv);   // epoch is already local wall-clock → gmtime keeps it as-is
    if (clock_fmt == 12) {
        int h12 = tmv.tm_hour % 12;
        if (h12 == 0) h12 = 12;
        snprintf(buf, len, "%d:%02d %s", h12, tmv.tm_min, tmv.tm_hour < 12 ? "AM" : "PM");
    } else {
        snprintf(buf, len, "%02d:%02d", tmv.tm_hour, tmv.tm_min);
    }
}

// At the limit the reset is when Claude works again, so the line says when,
// as a clock time if the daemon sends one.
static void format_reset_time(int mins, bool at_limit, char* buf, size_t len) {
    const char* verb = at_limit ? "Back in" : "Resets in";
    if (mins < 0) {
        snprintf(buf, len, "---");
    } else if (at_limit && clock_base_epoch > 0 && mins < 1440) {
        char t[12];
        format_clock((long)mins * 60, t, sizeof(t));
        snprintf(buf, len, "Back at %s", t);
    } else if (mins < 60) {
        snprintf(buf, len, "%s %dm", verb, mins);
    } else if (mins < 1440) {
        snprintf(buf, len, "%s %dh %dm", verb, mins / 60, mins % 60);
    } else {
        snprintf(buf, len, "%s %dd %dh", verb, mins / 1440, (mins % 1440) / 60);
    }
}

// Forward decls — callbacks defined near ui_show_screen below
static void global_click_cb(lv_event_t* e);

static lv_obj_t* make_panel(lv_obj_t* parent, int x, int y, int w, int h) {
    lv_obj_t* panel = lv_obj_create(parent);
    lv_obj_set_pos(panel, x, y);
    lv_obj_set_size(panel, w, h);
    lv_obj_set_style_bg_color(panel, COL_PANEL, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(panel, L.radius, 0);
    lv_obj_set_style_border_color(panel, COL_LINE, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_pad_left(panel, L.panel_pad_x, 0);
    lv_obj_set_style_pad_right(panel, L.panel_pad_x, 0);
    lv_obj_set_style_pad_top(panel, L.panel_pad_y, 0);
    lv_obj_set_style_pad_bottom(panel, L.panel_pad_y, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_EVENT_BUBBLE);
    return panel;
}

static lv_obj_t* make_bar(lv_obj_t* parent, int x, int y, int w, int h) {
    lv_obj_t* bar = lv_bar_create(parent);
    lv_obj_set_pos(bar, x, y);
    lv_obj_set_size(bar, w, h);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar, COL_LINE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, COL_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 4, LV_PART_INDICATOR);
    return bar;
}

static void init_icon_dsc_rgb565a8(lv_image_dsc_t* dsc, int w, int h, const uint8_t* data) {
    dsc->header.w = w;
    dsc->header.h = h;
    dsc->header.cf = LV_COLOR_FORMAT_RGB565A8;
    dsc->header.stride = w * 2;
    dsc->data = data;
    dsc->data_size = w * h * 3;
}

static lv_obj_t* make_pill(lv_obj_t* parent, const char* text) {
    lv_obj_t* lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, L.pill_font, 0);
    lv_obj_set_style_text_color(lbl, THEME_ON_BADGE, 0);
    lv_obj_set_style_bg_color(lbl, THEME_BADGE, 0);
    lv_obj_set_style_bg_opa(lbl, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(lbl, 4, 0);
    lv_obj_set_style_pad_left(lbl, L.pill_pad_x, 0);
    lv_obj_set_style_pad_right(lbl, L.pill_pad_x, 0);
    lv_obj_set_style_pad_top(lbl, L.pill_pad_y, 0);
    lv_obj_set_style_pad_bottom(lbl, L.pill_pad_y, 0);
    return lbl;
}

static void init_battery_icons(void) {
    if (L.small_icons) {
        init_icon_dsc_rgb565a8(&battery_dscs[0], ICON_BATTERY_SMALL_W, ICON_BATTERY_SMALL_H, icon_battery_small_data);
        init_icon_dsc_rgb565a8(&battery_dscs[1], ICON_BATTERY_LOW_SMALL_W, ICON_BATTERY_LOW_SMALL_H, icon_battery_low_small_data);
        init_icon_dsc_rgb565a8(&battery_dscs[2], ICON_BATTERY_MEDIUM_SMALL_W, ICON_BATTERY_MEDIUM_SMALL_H, icon_battery_medium_small_data);
        init_icon_dsc_rgb565a8(&battery_dscs[3], ICON_BATTERY_FULL_SMALL_W, ICON_BATTERY_FULL_SMALL_H, icon_battery_full_small_data);
        init_icon_dsc_rgb565a8(&battery_dscs[4], ICON_BATTERY_CHARGING_SMALL_W, ICON_BATTERY_CHARGING_SMALL_H, icon_battery_charging_small_data);
        init_icon_dsc_rgb565a8(&battery_dscs[5], ICON_PLUG_ZAP_SMALL_W, ICON_PLUG_ZAP_SMALL_H, icon_plug_zap_small_data);
        return;
    }
    init_icon_dsc_rgb565a8(&battery_dscs[0], ICON_BATTERY_W, ICON_BATTERY_H, icon_battery_data);
    init_icon_dsc_rgb565a8(&battery_dscs[1], ICON_BATTERY_LOW_W, ICON_BATTERY_LOW_H, icon_battery_low_data);
    init_icon_dsc_rgb565a8(&battery_dscs[2], ICON_BATTERY_MEDIUM_W, ICON_BATTERY_MEDIUM_H, icon_battery_medium_data);
    init_icon_dsc_rgb565a8(&battery_dscs[3], ICON_BATTERY_FULL_W, ICON_BATTERY_FULL_H, icon_battery_full_data);
    init_icon_dsc_rgb565a8(&battery_dscs[4], ICON_BATTERY_CHARGING_W, ICON_BATTERY_CHARGING_H, icon_battery_charging_data);
    init_icon_dsc_rgb565a8(&battery_dscs[5], ICON_PLUG_ZAP_W, ICON_PLUG_ZAP_H, icon_plug_zap_data);
}

// ======== Usage Screen ========

static lv_obj_t* make_usage_panel(lv_obj_t* parent, int y, const char* pill_text,
                                  lv_obj_t** out_pct, lv_obj_t** out_pill,
                                  lv_obj_t** out_bar, lv_obj_t** out_reset) {
    lv_obj_t* panel = make_panel(parent, L.margin, y, L.content_w, L.usage_panel_h);

    *out_pct = lv_label_create(panel);
    lv_label_set_text(*out_pct, "---%");
    lv_obj_set_style_text_font(*out_pct, L.pct_font, 0);
    lv_obj_set_style_text_color(*out_pct, COL_TEXT, 0);
    lv_obj_set_pos(*out_pct, 0, 0);

    *out_pill = make_pill(panel, pill_text);
    lv_obj_align(*out_pill, LV_ALIGN_TOP_RIGHT, 0, 1);

    *out_bar = make_bar(panel, 0, L.usage_bar_y,
                        L.content_w - 2 * L.panel_pad_x, L.bar_h);

    *out_reset = lv_label_create(panel);
    lv_label_set_text(*out_reset, "---");
    lv_obj_set_style_text_font(*out_reset, L.reset_font, 0);
    lv_obj_set_style_text_color(*out_reset, COL_DIM, 0);
    lv_obj_set_pos(*out_reset, 0, L.usage_reset_y);

    return panel;
}

// Pairing hint — shown when disconnected so the screen isn't empty and the
// user knows how to (re)pair. Wording matches the 3-second release gesture.
static void build_pair_group(lv_obj_t* parent) {
    pair_group = lv_obj_create(parent);
    lv_obj_set_size(pair_group, L.scr_w, L.scr_h - L.content_y);
    lv_obj_set_pos(pair_group, 0, L.content_y);
    lv_obj_set_style_bg_opa(pair_group, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(pair_group, 0, 0);
    lv_obj_set_style_pad_all(pair_group, 0, 0);
    lv_obj_clear_flag(pair_group, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(pair_group, LV_OBJ_FLAG_EVENT_BUBBLE);

    lv_obj_t* l1 = lv_label_create(pair_group);
    lv_label_set_text(l1, "To pair");
    lv_obj_set_style_text_font(l1, L.bt_status_font, 0);
    lv_obj_set_style_text_color(l1, COL_TEXT, 0);
    lv_obj_align(l1, LV_ALIGN_TOP_MID, 0, L.pair_y1);

    lv_obj_t* l2 = lv_label_create(pair_group);
    lv_label_set_text(l2, "hold the power button");
    lv_obj_set_style_text_font(l2, L.bt_device_font, 0);
    lv_obj_set_style_text_color(l2, COL_DIM, 0);
    lv_obj_align(l2, LV_ALIGN_TOP_MID, 0, L.pair_y2);

    lv_obj_t* l3 = lv_label_create(pair_group);
    lv_label_set_text(l3, "for 3 seconds, then release");
    lv_obj_set_style_text_font(l3, L.bt_device_font, 0);
    lv_obj_set_style_text_color(l3, COL_DIM, 0);
    lv_obj_align(l3, LV_ALIGN_TOP_MID, 0, L.pair_y3);

    lv_obj_add_flag(pair_group, LV_OBJ_FLAG_HIDDEN);  // ui_update_ble_status decides
}

// Waiting screen: shown while connecting or loading, and when no usage update
// has landed recently (token expired, daemon down, host asleep…). Full-screen,
// like the pairing hint, so we never render hours-old numbers as if they were live.
static void build_idle_group(lv_obj_t* parent) {
    idle_group = lv_obj_create(parent);
    lv_obj_set_size(idle_group, L.scr_w, L.scr_h - L.content_y);
    lv_obj_set_pos(idle_group, 0, L.content_y);
    lv_obj_set_style_bg_opa(idle_group, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(idle_group, 0, 0);
    lv_obj_set_style_pad_all(idle_group, 0, 0);
    lv_obj_clear_flag(idle_group, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(idle_group, LV_OBJ_FLAG_EVENT_BUBBLE);

    // A shrunk-down creature sits between the header and the status line:
    // jumping while a connection or data is on its way, riding a cloud when
    // nothing is coming. The status line carries the words.
    lv_obj_t* creature = splash_mini_create(idle_group, "cloud", L.idle_px);
    if (creature) lv_obj_align(creature, LV_ALIGN_CENTER, 0, -20);

    lv_obj_add_flag(idle_group, LV_OBJ_FLAG_HIDDEN);  // update_view_state decides
}

// ---- Stats workspace: messages per day as a heatmap, plus a grid of numbers,
// both from the daemon's stats messages.
static const StatsData* stats_data = nullptr;
static lv_obj_t* stats_empty;
static lv_obj_t* heat_obj;
// Two columns of stats, each a dim name over its value. Shared by the Stats
// and Today workspaces.
struct StatGrid { lv_obj_t* name[6]; lv_obj_t* value[6]; };
static StatGrid stats_grid;
static int32_t  stat_grid_y;   // top of the grid, under the heatmap
static int32_t   heat_label_w, heat_pitch, heat_cell, heat_text_h;

static int32_t text_width(const char* txt, const lv_font_t* font) {
    lv_point_t size;
    lv_text_get_size(&size, txt, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    return size.x;
}

static lv_color_t heat_color(uint8_t level) {
    return level ? lv_color_mix(COL_ACCENT, COL_LINE, level * 255 / 4) : COL_LINE;
}

// Text must outlive the draw call: literals or StatsData strings only.
static void heat_text(lv_layer_t* layer, lv_draw_label_dsc_t* dsc, const char* txt,
                      int32_t x, int32_t y) {
    dsc->text = txt;
    const lv_area_t a = { x, y, x + text_width(txt, dsc->font) + 2, y + heat_text_h };
    lv_draw_label(layer, dsc, &a);
}

// One object draws the whole heatmap: an lv_obj per cell would not fit LVGL's heap.
static void heat_draw_cb(lv_event_t* e) {
    if (!stats_data || !stats_data->days) return;
    lv_layer_t* layer = lv_event_get_layer(e);
    lv_area_t a;
    lv_obj_get_coords(lv_event_get_target_obj(e), &a);
    const int32_t gx = a.x1 + heat_label_w;          // grid origin
    const int32_t gy = a.y1 + heat_text_h + 4;

    lv_draw_label_dsc_t t;
    lv_draw_label_dsc_init(&t);
    t.font = L.stat_label_font;
    t.color = COL_DIM;
    for (uint8_t i = 0; i < stats_data->months; i++) {
        const char* name = stats_data->month[i].name;
        const int32_t x = gx + stats_data->month[i].col * heat_pitch;
        heat_text(layer, &t, name, LV_MIN(x, a.x2 - text_width(name, t.font)), a.y1);
    }
    static const char* const DAY[] = { "Mon", "Wed", "Fri" };
    for (int i = 0; i < 3; i++)
        heat_text(layer, &t, DAY[i], a.x1, gy + (1 + 2 * i) * heat_pitch + (heat_cell - heat_text_h) / 2);

    lv_draw_rect_dsc_t r;
    lv_draw_rect_dsc_init(&r);
    r.radius = 2;
    for (uint8_t i = 0; i < stats_data->days; i++) {
        const int32_t x = gx + (i / 7) * heat_pitch, y = gy + (i % 7) * heat_pitch;
        const lv_area_t cell = { x, y, x + heat_cell - 1, y + heat_cell - 1 };
        r.bg_color = heat_color(stats_data->level[i]);
        lv_draw_rect(layer, &r, &cell);
    }

    // Legend under the grid, right-aligned: Less, the five levels, More.
    const int32_t ly = gy + 7 * heat_pitch + 4;
    const int32_t cy = ly + (heat_text_h - heat_cell) / 2;
    int32_t x = a.x2 - text_width("More", t.font);
    heat_text(layer, &t, "More", x, ly);
    x -= 6 + 5 * heat_pitch;
    for (uint8_t lvl = 0; lvl <= 4; lvl++) {
        const lv_area_t cell = { x + lvl * heat_pitch, cy, x + lvl * heat_pitch + heat_cell - 1, cy + heat_cell - 1 };
        r.bg_color = heat_color(lvl);
        lv_draw_rect(layer, &r, &cell);
    }
    heat_text(layer, &t, "Less", x - 6 - text_width("Less", t.font), ly);
}

static void build_stat_grid(lv_obj_t* tile, StatGrid* g) {
    const int32_t row_h = heat_text_h + lv_font_get_line_height(L.stat_value_font) + 4;
    for (int i = 0; i < 6; i++) {
        const int32_t x = L.margin + (i % 2) * (L.content_w / 2);
        const int32_t y = stat_grid_y + (i / 2) * row_h;
        g->name[i] = lv_label_create(tile);
        lv_obj_set_style_text_font(g->name[i], L.stat_label_font, 0);
        lv_obj_set_style_text_color(g->name[i], COL_DIM, 0);
        lv_obj_set_pos(g->name[i], x, y);
        g->value[i] = lv_label_create(tile);
        lv_obj_set_style_text_font(g->value[i], L.stat_value_font, 0);
        lv_obj_set_style_text_color(g->value[i], COL_TEXT, 0);
        lv_obj_set_pos(g->value[i], x, y + heat_text_h);
        lv_obj_add_flag(g->name[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g->value[i], LV_OBJ_FLAG_HIDDEN);
    }
}

static void set_stat_grid(StatGrid* g, const StatPair* stat, uint8_t n) {
    for (int i = 0; i < 6; i++) {
        const bool shown = i < n && i / 2 < L.stat_rows;
        if (shown) {
            lv_label_set_text(g->name[i], stat[i].label);
            lv_label_set_text(g->value[i], stat[i].value);
        }
        lv_obj_set_flag(g->name[i], LV_OBJ_FLAG_HIDDEN, !shown);
        lv_obj_set_flag(g->value[i], LV_OBJ_FLAG_HIDDEN, !shown);
    }
}

static void build_stats_workspace(lv_obj_t* tile) {
    heat_text_h  = lv_font_get_line_height(L.stat_label_font);
    heat_label_w = text_width("Mon", L.stat_label_font) + 6;
    heat_pitch   = (L.content_w - heat_label_w) / STATS_WEEKS;
    heat_cell    = heat_pitch - LV_MAX(1, heat_pitch / 7);
    const int32_t heat_h = heat_text_h + 4 + 7 * heat_pitch + 4 + heat_text_h;

    heat_obj = lv_obj_create(tile);
    lv_obj_remove_style_all(heat_obj);
    lv_obj_clear_flag(heat_obj, LV_OBJ_FLAG_CLICKABLE);   // taps fall through to the tile
    lv_obj_set_size(heat_obj, L.content_w, heat_h);
    lv_obj_set_pos(heat_obj, L.margin, L.content_y);
    lv_obj_add_event_cb(heat_obj, heat_draw_cb, LV_EVENT_DRAW_MAIN, NULL);

    stat_grid_y = L.content_y + heat_h + 10;
    build_stat_grid(tile, &stats_grid);

    stats_empty = lv_label_create(tile);
    lv_label_set_text(stats_empty, "No stats yet");
    lv_obj_set_style_text_font(stats_empty, L.stat_value_font, 0);
    lv_obj_set_style_text_color(stats_empty, COL_DIM, 0);
    lv_obj_center(stats_empty);
}

// ---- Today workspace: messages per hour as 24 bars, plus the stat grid, from
// the daemon's "td" message.
static const TodayData* today_data = nullptr;
static lv_obj_t* today_empty;
static lv_obj_t* hours_obj;
static StatGrid  today_grid;

static void hours_draw_cb(lv_event_t* e) {
    if (!today_data) return;
    lv_layer_t* layer = lv_event_get_layer(e);
    lv_area_t a;
    lv_obj_get_coords(lv_event_get_target_obj(e), &a);
    const int32_t pitch = lv_area_get_width(&a) / 24;
    const int32_t bar_w = pitch - LV_MAX(2, pitch / 4);
    const int32_t base = a.y2 - heat_text_h - 4;   // bars stand on this line
    const int32_t span = base - a.y1;

    lv_draw_rect_dsc_t r;
    lv_draw_rect_dsc_init(&r);
    r.radius = 2;
    for (int h = 0; h < 24; h++) {
        const uint8_t lvl = today_data->hour[h];
        const int32_t x = a.x1 + h * pitch;
        const int32_t top = base - (lvl ? LV_MAX(4, span * lvl / 8) : 2);   // 2px stub for idle hours
        const lv_area_t bar = { x, top, x + bar_w - 1, base - 1 };
        r.bg_color = lvl ? COL_ACCENT : COL_LINE;
        lv_draw_rect(layer, &r, &bar);
    }

    lv_draw_label_dsc_t t;
    lv_draw_label_dsc_init(&t);
    t.font = L.stat_label_font;
    t.color = COL_DIM;
    static const char* const HOUR[] = { "0", "6", "12", "18" };
    for (int i = 0; i < 4; i++)
        heat_text(layer, &t, HOUR[i], a.x1 + i * 6 * pitch, base + 4);
}

static void build_today_workspace(lv_obj_t* tile) {
    // Same footprint as the heatmap, so both grids sit at the same height.
    hours_obj = lv_obj_create(tile);
    lv_obj_remove_style_all(hours_obj);
    lv_obj_clear_flag(hours_obj, LV_OBJ_FLAG_CLICKABLE);   // taps fall through to the tile
    lv_obj_set_size(hours_obj, L.content_w, stat_grid_y - 10 - L.content_y);
    lv_obj_set_pos(hours_obj, L.margin, L.content_y);
    lv_obj_add_event_cb(hours_obj, hours_draw_cb, LV_EVENT_DRAW_MAIN, NULL);
    build_stat_grid(tile, &today_grid);

    today_empty = lv_label_create(tile);
    lv_label_set_text(today_empty, "Nothing yet today");
    lv_obj_set_style_text_font(today_empty, L.stat_value_font, 0);
    lv_obj_set_style_text_color(today_empty, COL_DIM, 0);
    lv_obj_center(today_empty);
}

// ---- Sessions workspace: one row per open Claude Code session. A mark on the
// left shows the state (a turning spinner while it works), then the project
// and how long, and on a second line the git branch and what it is doing. Rows
// are laid out in ui_update_sessions, since a row with neither takes one line.
struct SessRow { lv_obj_t *mark, *name, *time, *branch, *doing, *rule; };
static SessRow   sess[SESSION_ROWS];
static char      sess_state[SESSION_ROWS];   // what each row shows, for sess_tick
static lv_obj_t* sess_empty;
static lv_obj_t* sess_more;                  // "+3 more" when rows run out
static int32_t   sess_mark_w;                // mark column, gap included

static lv_obj_t* sess_label(lv_obj_t* tile, const lv_font_t* font) {
    lv_obj_t* l = lv_label_create(tile);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_add_flag(l, LV_OBJ_FLAG_HIDDEN);
    return l;
}

// Session and weekly usage as one line of thin bars above the rows. Hidden
// until the first usage data arrives.
struct MiniBar { lv_obj_t *name, *bar, *pct; };
static MiniBar sess_use[2];
static int32_t sess_use_h;                   // the strip and its gap to the rows

static void set_mini_bar(int i, const char* name, int pct, lv_color_t color) {
    const MiniBar& m = sess_use[i];
    const int32_t gap  = L.margin / 2;
    const int32_t half = (L.content_w - L.margin) / 2;
    const int32_t x    = L.margin + i * (half + L.margin);
    const int32_t name_w = text_width(name, L.use_font);
    const int32_t pct_w  = text_width("100%", L.use_font);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", pct);
    lv_label_set_text(m.name, name);
    lv_label_set_text(m.pct, buf);
    lv_obj_set_pos(m.name, x, L.content_y);
    lv_obj_set_pos(m.pct, x + half - text_width(buf, L.use_font), L.content_y);
    const int32_t bar_h = lv_obj_get_height(m.bar);
    lv_obj_set_pos(m.bar, x + name_w + gap,
                   L.content_y + (lv_font_get_line_height(L.use_font) - bar_h) / 2);
    lv_obj_set_width(m.bar, half - name_w - pct_w - 2 * gap);
    lv_bar_set_value(m.bar, pct, LV_ANIM_ON);
    lv_obj_set_style_bg_color(m.bar, color, LV_PART_INDICATOR);
    lv_obj_clear_flag(m.name, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(m.bar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(m.pct, LV_OBJ_FLAG_HIDDEN);
}

static void build_sessions_workspace(lv_obj_t* tile) {
    sess_mark_w = text_width(spinner_frames[1], L.anim_font) + L.margin / 2;
    for (MiniBar& m : sess_use) {
        m.name = sess_label(tile, L.use_font);
        lv_obj_set_style_text_color(m.name, COL_DIM, 0);
        m.pct = sess_label(tile, L.use_font);
        lv_obj_set_style_text_color(m.pct, COL_TEXT, 0);
        m.bar = make_bar(tile, 0, 0, 0, LV_MAX(L.bar_h * 3 / 10, 4));
        lv_obj_set_style_radius(m.bar, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_radius(m.bar, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
        lv_obj_add_flag(m.bar, LV_OBJ_FLAG_HIDDEN);
    }
    sess_use_h = lv_font_get_line_height(L.use_font)
               + lv_font_get_line_height(L.stat_value_font) / 2;
    for (int i = 0; i < SESSION_ROWS; i++) {
        SessRow& r = sess[i];
        r.mark  = sess_label(tile, L.anim_font);
        r.name  = sess_label(tile, L.stat_value_font);
        lv_label_set_long_mode(r.name, LV_LABEL_LONG_DOT);
        r.time  = sess_label(tile, L.stat_label_font);
        lv_obj_set_style_text_color(r.time, COL_DIM, 0);
        r.branch = sess_label(tile, L.stat_label_font);
        lv_label_set_long_mode(r.branch, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_color(r.branch, COL_DIM, 0);
        r.doing = sess_label(tile, L.stat_label_font);
        r.rule  = lv_obj_create(tile);
        lv_obj_remove_style_all(r.rule);
        lv_obj_set_style_bg_color(r.rule, COL_LINE, 0);
        lv_obj_set_style_bg_opa(r.rule, LV_OPA_COVER, 0);
        lv_obj_set_size(r.rule, L.content_w, 1);
        lv_obj_add_flag(r.rule, LV_OBJ_FLAG_HIDDEN);
    }

    sess_more = sess_label(tile, L.stat_label_font);
    lv_obj_set_style_text_color(sess_more, COL_DIM, 0);

    sess_empty = lv_label_create(tile);
    lv_label_set_text(sess_empty, "No sessions");
    lv_obj_set_style_text_font(sess_empty, L.stat_value_font, 0);
    lv_obj_set_style_text_color(sess_empty, COL_DIM, 0);
    lv_obj_center(sess_empty);
}

// Workspaces: full-screen pages stacked in place, one visible at a time. A
// horizontal swipe slides to the next one while the header, status line and
// mascot stay put. The page doesn't follow the finger, because each frame of
// motion redraws the whole screen over QSPI; see WS_SLIDE_MS. Taps bubble up to
// usage_container, where a tap toggles the splash.
enum { WS_SESSIONS, WS_USAGE, WS_TODAY, WS_STATS, WS_COUNT };
static const char* const ws_title[WS_COUNT] = {"Sessions", "Usage", "Today", "Stats"};
static lv_obj_t* ws[WS_COUNT];
static uint8_t   ws_cur;

// A short time-based slide: LVGL drops frames instead of stretching it, so it
// stays quick even though each frame is a full-screen redraw.
static const uint32_t WS_SLIDE_MS = 120;

static void ws_x_cb(void* obj, int32_t x) { lv_obj_set_x((lv_obj_t*)obj, x); }

static void ws_slide_done(lv_anim_t* a) {
    if (a->var != ws[ws_cur]) lv_obj_add_flag((lv_obj_t*)a->var, LV_OBJ_FLAG_HIDDEN);
}

static void ws_slide(lv_obj_t* obj, int32_t from, int32_t to) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, ws_x_cb);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_duration(&a, WS_SLIDE_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_completed_cb(&a, ws_slide_done);
    lv_anim_start(&a);
}

static void show_workspace(uint8_t i) {
    const int32_t dx = i > ws_cur ? L.scr_w : -L.scr_w;
    lv_obj_t* out = ws[ws_cur];
    ws_cur = i;
    // A swipe during a slide restarts from wherever the pages are.
    for (uint8_t k = 0; k < WS_COUNT; k++) {
        lv_anim_delete(ws[k], ws_x_cb);
        if (ws[k] != out && k != i) lv_obj_add_flag(ws[k], LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_clear_flag(ws[i], LV_OBJ_FLAG_HIDDEN);
    ws_slide(out, lv_obj_get_x(out), -dx);
    ws_slide(ws[i], dx, 0);
    // The header names the workspace, unless the clock owns it.
    if (clock_base_epoch == 0) lv_label_set_text(lbl_title, ws_title[i]);
}

static void ws_gesture_cb(lv_event_t* e) {
    (void)e;
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
    if (dir == LV_DIR_LEFT && ws_cur + 1 < WS_COUNT) show_workspace(ws_cur + 1);
    else if (dir == LV_DIR_RIGHT && ws_cur > 0)      show_workspace(ws_cur - 1);
}

static lv_obj_t* add_workspace(lv_obj_t* parent) {
    lv_obj_t* ws = lv_obj_create(parent);
    lv_obj_set_size(ws, L.scr_w, L.scr_h);
    lv_obj_set_pos(ws, 0, 0);
    lv_obj_set_style_bg_opa(ws, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ws, 0, 0);
    lv_obj_set_style_pad_all(ws, 0, 0);
    lv_obj_clear_flag(ws, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ws, LV_OBJ_FLAG_EVENT_BUBBLE);
    return ws;
}

static void init_usage_screen(lv_obj_t* scr) {
    usage_container = lv_obj_create(scr);
    lv_obj_set_size(usage_container, L.scr_w, L.scr_h);
    lv_obj_set_pos(usage_container, 0, 0);
    lv_obj_set_style_bg_opa(usage_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(usage_container, 0, 0);
    lv_obj_set_style_pad_all(usage_container, 0, 0);
    lv_obj_clear_flag(usage_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(usage_container, global_click_cb, LV_EVENT_CLICKED, NULL);

    // Created before the title and status line so those draw on top.
    for (uint8_t i = 0; i < WS_COUNT; i++) {
        ws[i] = add_workspace(usage_container);
        if (i != ws_cur) lv_obj_add_flag(ws[i], LV_OBJ_FLAG_HIDDEN);
    }
    // Gestures bubble to the screen by default; stop them here. The screen must
    // not scroll either (the mascot walks past its edge), or LVGL drops them.
    lv_obj_clear_flag(usage_container, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(usage_container, ws_gesture_cb, LV_EVENT_GESTURE, NULL);
    build_stats_workspace(ws[WS_STATS]);   // first: it sets the grid position Today reuses
    build_today_workspace(ws[WS_TODAY]);
    build_sessions_workspace(ws[WS_SESSIONS]);

    lbl_title = lv_label_create(usage_container);
    lv_label_set_text(lbl_title, ws_title[ws_cur]);
    lv_obj_set_style_text_font(lbl_title, L.title_font, 0);
    lv_obj_set_style_text_color(lbl_title, COL_TEXT, 0);
    // The nudge balances the corner logo on the left; smaller on small
    // screens where the logo is 40px and the battery icon sits closer. The
    // title is centered on the battery icon's height.
    const int32_t batt_h = L.small_icons ? ICON_BATTERY_SMALL_H : ICON_BATTERY_H;
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, L.title_nudge,
                 L.batt_y + (batt_h - lv_font_get_line_height(L.title_font)) / 2);

    // Usage panels (shown when connected) live in a transparent full-size group
    // so they can be toggled against the pairing hint as one unit.
    usage_group = lv_obj_create(ws[WS_USAGE]);
    lv_obj_set_size(usage_group, L.scr_w, L.scr_h);
    lv_obj_set_pos(usage_group, 0, 0);
    lv_obj_set_style_bg_opa(usage_group, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(usage_group, 0, 0);
    lv_obj_set_style_pad_all(usage_group, 0, 0);
    lv_obj_clear_flag(usage_group, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(usage_group, LV_OBJ_FLAG_EVENT_BUBBLE);

    panel_session = make_usage_panel(usage_group, L.content_y, "Current",
                     &lbl_session_pct, &lbl_session_label,
                     &bar_session, &lbl_session_reset);

    // Enterprise-only overlays inside panel_session — hidden until enterprise data arrives
    lbl_session_pct_sym = lv_label_create(panel_session);
    lv_label_set_text(lbl_session_pct_sym, "%");
    lv_obj_set_style_text_font(lbl_session_pct_sym, L.reset_font, 0);
    lv_obj_set_style_text_color(lbl_session_pct_sym, COL_TEXT, 0);
    lv_obj_add_flag(lbl_session_pct_sym, LV_OBJ_FLAG_HIDDEN);

    lbl_spending_desc = lv_label_create(panel_session);
    lv_label_set_text(lbl_spending_desc, "of your monthly budget");
    lv_obj_set_style_text_font(lbl_spending_desc, L.reset_font, 0);
    lv_obj_set_style_text_color(lbl_spending_desc, COL_DIM, 0);
    lv_obj_set_pos(lbl_spending_desc, 0, L.usage_reset_y);
    lv_obj_add_flag(lbl_spending_desc, LV_OBJ_FLAG_HIDDEN);

    lbl_spending_status = lv_label_create(panel_session);
    lv_label_set_text(lbl_spending_status, "");
    lv_obj_set_style_text_font(lbl_spending_status, L.pace_font, 0);
    lv_obj_set_pos(lbl_spending_status, 0, L.usage_reset_y + 20);
    lv_obj_add_flag(lbl_spending_status, LV_OBJ_FLAG_HIDDEN);

    panel_weekly = make_usage_panel(usage_group,
                     L.content_y + L.usage_panel_h + L.usage_panel_gap, "Weekly",
                     &lbl_weekly_pct, &lbl_weekly_label,
                     &bar_weekly, &lbl_weekly_reset);
    // Recolor enabled so enterprise period box can color pace and reset separately
    lv_label_set_recolor(lbl_weekly_reset, true);
    lbl_plan = make_pill(panel_weekly, "");
    lv_obj_add_flag(lbl_plan, LV_OBJ_FLAG_HIDDEN);

    build_pair_group(ws[WS_USAGE]);
    build_idle_group(ws[WS_USAGE]);

    // Status line — always visible on the usage view. Driven by ui_tick_anim().
    lbl_anim = lv_label_create(usage_container);
    lv_label_set_text(lbl_anim, "");
    lv_obj_set_style_text_font(lbl_anim, L.anim_font, 0);
    lv_obj_set_style_text_color(lbl_anim, COL_ACCENT, 0);
    lv_obj_align(lbl_anim, LV_ALIGN_BOTTOM_MID, 0, L.anim_y);

    // Two labels rather than a recolored one: recolor can't escape a '#' in a title.
    done_box = lv_obj_create(usage_container);
    lv_obj_remove_style_all(done_box);
    lv_obj_set_size(done_box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(done_box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(done_box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(done_box, L.margin / 2, 0);
    lv_obj_set_style_pad_hor(done_box, L.panel_pad_x, 0);
    lv_obj_set_style_pad_ver(done_box, L.pill_pad_y, 0);
    lv_obj_set_style_bg_color(done_box, COL_PANEL, 0);
    lv_obj_set_style_bg_opa(done_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(done_box, COL_LINE, 0);
    lv_obj_set_style_border_width(done_box, 1, 0);
    lv_obj_set_style_radius(done_box, L.radius, 0);
    lv_obj_remove_flag(done_box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(done_box, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t* check = lv_label_create(done_box);
    lv_label_set_text_static(check, CHECK);
    lv_obj_set_style_text_font(check, L.anim_font, 0);
    lv_obj_set_style_text_color(check, COL_DONE, 0);
    lbl_done_task = lv_label_create(done_box);
    lv_obj_set_style_text_font(lbl_done_task, L.stat_value_font, 0);
    lv_obj_set_style_text_color(lbl_done_task, COL_TEXT, 0);
    lv_label_set_long_mode(lbl_done_task, LV_LABEL_LONG_SCROLL_CIRCULAR);
    done_task_max_w = L.content_w - 2 * L.panel_pad_x - L.margin / 2
                      - text_width(CHECK, L.anim_font);
    // Centered in the space under the dancing Clawd's feet (big_feet_y in splash.cpp).
    lv_obj_update_layout(done_box);
    const int32_t below = L.scr_h - L.scr_h * 7 / 8;
    lv_obj_align(done_box, LV_ALIGN_BOTTOM_MID, 0, -(below - lv_obj_get_height(done_box)) / 2);
}

// LVGL sends CLICKED for any press that didn't scroll, swipes included. A tap
// is a press released near where it started.
static const int32_t TAP_SLOP_PX = 20;
static lv_point_t    press_pt;

static void press_cb(lv_event_t* e) { (void)e; lv_indev_get_point(lv_indev_active(), &press_pt); }

static bool was_tap(void) {
    lv_point_t p;
    lv_indev_get_point(lv_indev_active(), &p);
    return LV_ABS(p.x - press_pt.x) <= TAP_SLOP_PX && LV_ABS(p.y - press_pt.y) <= TAP_SLOP_PX;
}

// ---- Attention: the mascot walk-on (splash_mascot_attention) plus the
// message on the status line. Ends on a tap, after 2 minutes, or ui_hide_alert().
static const uint32_t ALERT_TIMEOUT_MS = 120000;
static bool        alert_active = false;
static lv_timer_t* alert_timer;

static void alert_timer_cb(lv_timer_t* t) { (void)t; ui_hide_alert(); }

// Registered on the input device, so it sees every tap. A tap that dismisses
// does nothing else.
static void alert_tap_cb(lv_event_t* e) {
    if (!alert_active || !was_tap()) return;
    ui_hide_alert();
    lv_indev_stop_processing((lv_indev_t*)lv_event_get_user_data(e));
}

// ---- Celebration: the finished task's name in place of the status line until
// the dancing Clawd leaves, and at least CELEBRATE_MIN_MS on boards without him.
static const uint32_t CELEBRATE_MIN_MS = 10000;
static bool     celebrating = false;
static uint32_t celebrate_since;

static void end_celebration(void) {
    celebrating = false;
    lv_obj_add_flag(done_box, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(lbl_anim, LV_OBJ_FLAG_HIDDEN);
}

static void init_alert(void) {
    alert_timer = lv_timer_create(alert_timer_cb, ALERT_TIMEOUT_MS, NULL);
    lv_timer_pause(alert_timer);
    lv_indev_t* indev = lv_indev_get_next(NULL);
    if (indev) {
        lv_indev_add_event_cb(indev, press_cb, LV_EVENT_PRESSED, NULL);
        lv_indev_add_event_cb(indev, alert_tap_cb, LV_EVENT_CLICKED, indev);
    }
    lv_obj_set_style_text_align(lbl_anim, LV_TEXT_ALIGN_CENTER, 0);
}

// ======== Public API ========

void ui_init(void) {
    compute_layout(board_caps());

    lv_obj_t* scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, COL_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

#ifndef BOARD_HAS_PSRAM
    // Static corner mascot (see clawd_still.h) — the animated one needs PSRAM.
    if (L.small_icons) init_icon_dsc_rgb565a8(&logo_dsc, CLAWD_STILL_SMALL_W, CLAWD_STILL_SMALL_H, clawd_still_small_data);
    else               init_icon_dsc_rgb565a8(&logo_dsc, CLAWD_STILL_W, CLAWD_STILL_H, clawd_still_data);
#endif
    init_battery_icons();

    init_usage_screen(scr);
    splash_init(scr);

    if (splash_get_root()) {
        lv_obj_add_event_cb(splash_get_root(), global_click_cb, LV_EVENT_CLICKED, NULL);
    }

    // Corner mascot in the old logo slot. The still Clawd is shorter than the
    // 80/40 px slot the spark logo used; center it vertically in that slot.
    {
        const int slot  = L.small_icons ? LOGO_SMALL_HEIGHT : LOGO_HEIGHT;
        const int art_h = L.small_icons ? CLAWD_STILL_SMALL_H : CLAWD_STILL_H;
        const int top   = L.logo_y + (slot - art_h) / 2;
#ifdef BOARD_HAS_PSRAM
        // Animated: idles, does acts, and takes walk-off/lurk trips.
        splash_mascot_create(scr, L.margin, top + art_h, L.small_icons ? 2 : 3);
#else
        logo_img = lv_image_create(scr);
        lv_image_set_src(logo_img, &logo_dsc);
        lv_obj_set_pos(logo_img, L.margin, top);
#endif
    }

    battery_img = lv_image_create(scr);
    lv_image_set_src(battery_img, &battery_dscs[0]);
    lv_obj_set_pos(battery_img, L.scr_w - L.batt_w - L.margin, L.batt_y);
    // Boards without battery telemetry never show the indicator (per the HAL
    // contract; previously every board drew the empty-battery glyph).
    if (!board_caps().has_battery) {
        lv_obj_del(battery_img);
        battery_img = nullptr;
    }

    init_alert();
}

void ui_update(const UsageData* data) {
    if (!data->valid) return;
    data_ok = data->ok;
    beat_since_connect = true;
    if (!data->ok) return;          // a {"ok":false} "no data" beat → fall through to idle, keep last numbers
    last_data_ms = lv_tick_get();   // a real usage update just landed
    data_received = true;

    if (data->clock_epoch > 0) {    // daemon supplied wall-clock time → drive the title clock
        clock_base_epoch = data->clock_epoch;
        clock_base_ms = last_data_ms;
        clock_fmt = data->clock_fmt;
    } else if (clock_base_epoch != 0) {   // clock turned off daemon-side → back to the workspace title
        clock_base_epoch = 0;
        clock_last_min = -1;
        lv_label_set_text(lbl_title, ws_title[ws_cur]);
    }

    int s_pct = (int)(data->session_pct + 0.5f);

    if (data->enterprise) {
        // Spending box: big number-only label + small "%" symbol + desc + pace
        lv_obj_set_style_text_font(lbl_session_pct, L.ent_pct_font, 0);
        lv_label_set_text(lbl_session_label, "Spending");
        lv_obj_add_flag(lbl_session_reset, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(lbl_session_pct_sym, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(lbl_spending_desc,   LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lbl_spending_status,   LV_OBJ_FLAG_HIDDEN);
        if (panel_weekly) lv_obj_clear_flag(panel_weekly, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_set_style_text_font(lbl_session_pct, L.pct_font, 0);
        lv_label_set_text(lbl_session_label, "Current");
        lv_obj_clear_flag(lbl_session_reset, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lbl_session_pct_sym, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lbl_spending_desc,   LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lbl_spending_status, LV_OBJ_FLAG_HIDDEN);
        if (panel_weekly) lv_obj_clear_flag(panel_weekly, LV_OBJ_FLAG_HIDDEN);
    }

    char buf[48];

    // Pace vars used in both enterprise blocks below
    const char* pace_text = "Under pace";
    lv_color_t  pace_color = COL_TEXT;
    const char* pace_hex   = "faf9f5";   // recolor hex for each token in theme.h
    if (data->session_pct > (float)data->time_pct + 15.0f) {
        pace_text = "Over pace";  pace_color = COL_ALERT;  pace_hex = "c6613f";
    } else if (data->session_pct > (float)data->time_pct - 15.0f) {
        pace_text = "On pace";    pace_color = COL_ACCENT; pace_hex = "d97757";
    }

    if (data->enterprise) {
        lv_label_set_text_fmt(lbl_session_pct, "%d", s_pct);
        lv_obj_align_to(lbl_session_pct_sym, lbl_session_pct,
                        LV_ALIGN_OUT_RIGHT_TOP, 4, 12);
    } else {
        lv_label_set_text_fmt(lbl_session_pct, "%d%%", s_pct);
        format_reset_time(data->session_reset_mins, s_pct >= 100, buf, sizeof(buf));
        lv_label_set_text(lbl_session_reset, buf);
    }

    lv_bar_set_value(bar_session, s_pct, LV_ANIM_ON);
    lv_obj_set_style_bg_color(bar_session, pct_color(data->session_pct), LV_PART_INDICATOR);
    set_mini_bar(0, data->enterprise ? "Spend" : "5h", s_pct, pct_color(data->session_pct));

    if (data->enterprise) {
        // Period box: time % + dynamic pace color + "Resets <date>" label
        lv_label_set_text(lbl_weekly_label, "Period");
        lv_label_set_text_fmt(lbl_weekly_pct, "%d%%", data->time_pct);
        lv_bar_set_value(bar_weekly, data->time_pct, LV_ANIM_ON);
        lv_color_t bar_pace = (data->session_pct <= (float)data->time_pct + 15.0f) ? COL_ACCENT : COL_ALERT;
        lv_obj_set_style_bg_color(bar_weekly, bar_pace, LV_PART_INDICATOR);
        set_mini_bar(1, "Period", data->time_pct, bar_pace);
        snprintf(buf, sizeof(buf), "#%s %s# - #faf9f5 Resets %s#",
                 pace_hex, pace_text, data->reset_date);
        lv_label_set_text(lbl_weekly_reset, buf);
    } else {
        int w_pct = (int)(data->weekly_pct + 0.5f);
        lv_label_set_text_fmt(lbl_weekly_pct, "%d%%", w_pct);
        lv_bar_set_value(bar_weekly, w_pct, LV_ANIM_ON);
        lv_obj_set_style_bg_color(bar_weekly, pct_color(data->weekly_pct), LV_PART_INDICATOR);
        set_mini_bar(1, "7d", w_pct, pct_color(data->weekly_pct));
        format_reset_time(data->weekly_reset_mins, w_pct >= 100, buf, sizeof(buf));
        lv_label_set_text(lbl_weekly_reset, buf);
    }

    if (!data->plan[0]) {
        lv_obj_add_flag(lbl_plan, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_label_set_text(lbl_plan, data->plan);
        lv_obj_clear_flag(lbl_plan, LV_OBJ_FLAG_HIDDEN);
        lv_obj_update_layout(panel_weekly);
        lv_obj_align_to(lbl_plan, lbl_weekly_label, LV_ALIGN_OUT_LEFT_MID, -L.pill_pad_x / 2, 0);
    }
}

// Pick the usage-view sub-screen: pairing hint (nothing bonded), the waiting
// screen, or the live usage panels. Only re-lays-out on an actual change. The
// status line stays visible everywhere and names the link state.
static Link link_state(uint32_t now) {
    if (!s_ble_connected) {
        if (!ble_has_bonds()) return LINK_UNPAIRED;
        return now - disconnected_at_ms < CONNECT_GRACE_MS ? LINK_CONNECTING : LINK_OFFLINE;
    }
    if (data_received && data_ok && now - last_data_ms < DATA_FRESH_MS) return LINK_LIVE;
    if (!beat_since_connect && now - connected_at_ms < DATA_FRESH_MS) return LINK_LOADING;
    return LINK_NO_DATA;
}

static void update_view_state(void) {
    if (!usage_group || !pair_group || !idle_group) return;
    const Link link = link_state(lv_tick_get());
    const int v = link == LINK_UNPAIRED ? 0 : link == LINK_LIVE ? 2 : 1;
    if (v == 1) splash_mini_set(link == LINK_CONNECTING || link == LINK_LOADING ? "jumping" : "cloud");
    if (v == view_state) return;
    view_state = v;
    lv_obj_add_flag(pair_group, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(idle_group, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(usage_group, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(v == 0 ? pair_group : v == 1 ? idle_group : usage_group,
                      LV_OBJ_FLAG_HIDDEN);
}

// Turns the spinner on working rows and pulses the mark on rows that need you.
// Runs on its own clock: the status line's spinner stops while an alert holds it.
static void sess_tick(uint32_t now) {
    static uint32_t last_ms;
    static uint8_t  phase, frame;
    if (ws_cur != WS_SESSIONS || now - last_ms < spinner_ms[frame]) return;
    last_ms = now;
    phase = (phase + 1) % SPINNER_PHASES;
    frame = phase < SPINNER_COUNT ? phase : SPINNER_PHASES - phase;
    for (int i = 0; i < SESSION_ROWS; i++) {
        if (sess_state[i] == 'w') lv_label_set_text_static(sess[i].mark, spinner_frames[frame]);
        else if (sess_state[i] == 'a')
            lv_obj_set_style_opa(sess[i].mark, phase < SPINNER_PHASES / 2 ? LV_OPA_COVER : LV_OPA_30, 0);
    }
}

void ui_tick_anim(void) {
    if (current_screen != SCREEN_USAGE) return;
    update_view_state();
    if (view_state == 1) splash_mini_tick();   // animate the creature on the waiting screen

    uint32_t now = lv_tick_get();
    sess_tick(now);

    // Title clock: once the daemon has sent wall-clock time, replace the
    // workspace title with the live time, advanced locally so it ticks every
    // minute between payloads.
    if (clock_base_epoch > 0) {
        time_t cur = (time_t)(clock_base_epoch + (now - clock_base_ms) / 1000);
        struct tm tmv;
        gmtime_r(&cur, &tmv);
        if (tmv.tm_min != clock_last_min) {   // only rewrite the title when the minute changes
            clock_last_min = tmv.tm_min;
            char tbuf[12];
            format_clock(0, tbuf, sizeof(tbuf));
            lv_label_set_text(lbl_title, tbuf);
        }
    }

    if (alert_active) return;          // the status line holds the alert message
    if (celebrating) {
        if (now - celebrate_since < CELEBRATE_MIN_MS || splash_mascot_celebrating()) return;
        end_celebration();
    }

    if (now - anim_msg_start >= ANIM_MSG_MS) {
        anim_msg_idx = (anim_msg_idx + 1) % ANIM_MSG_COUNT;
        anim_msg_start = now;
    }

    if (now - anim_last_ms < spinner_ms[anim_spinner_idx]) return;
    anim_last_ms = now;
    anim_phase = (anim_phase + 1) % SPINNER_PHASES;
    anim_spinner_idx = (anim_phase < SPINNER_COUNT) ? anim_phase
                                                    : (SPINNER_PHASES - anim_phase);

    // Status text by priority. The spinner runs only while something is
    // happening; settled states are plain words. Whimsical messages only while
    // Claude Code works.
    const char* text = "";
    const char* still = nullptr;
    switch (link_state(now)) {
    case LINK_UNPAIRED:   still = "Not paired"; break;
    case LINK_CONNECTING: text = "Connecting"; break;
    case LINK_OFFLINE:    still = "Not connected"; break;
    case LINK_LOADING:    text = "Loading data"; break;
    case LINK_NO_DATA:    still = "No data"; break;
    case LINK_LIVE:
        if (now - connected_at_ms < 5000) text = "Connected";
        else if (!s_working)              still = "Idle";
        else                              text = anim_messages[anim_msg_idx];
        break;
    }
    if (still) {
        lv_label_set_text(lbl_anim, still);
        return;
    }

    // All states share the whimsical style: "<glyph> <Title-case word>…"
    static char buf[80];
    snprintf(buf, sizeof(buf), "%s %s\xE2\x80\xA6",
             spinner_frames[anim_spinner_idx], text);
    lv_label_set_text(lbl_anim, buf);
}

static screen_t prev_non_splash_screen = SCREEN_USAGE;
static void apply_battery_visibility(void) {
    if (!battery_img) return;
    if (current_screen == SCREEN_SPLASH) lv_obj_add_flag(battery_img, LV_OBJ_FLAG_HIDDEN);
    else                                  lv_obj_clear_flag(battery_img, LV_OBJ_FLAG_HIDDEN);
}

static void global_click_cb(lv_event_t* e) {
    (void)e;
    if (!was_tap()) return;
    if (current_screen == SCREEN_SPLASH) ui_show_screen(prev_non_splash_screen);
    else                                  ui_show_screen(SCREEN_SPLASH);
}

void ui_show_screen(screen_t screen) {
    lv_obj_add_flag(usage_container, LV_OBJ_FLAG_HIDDEN);
    splash_hide();

    switch (screen) {
    case SCREEN_SPLASH:  splash_show(); break;
    case SCREEN_USAGE:   lv_obj_clear_flag(usage_container, LV_OBJ_FLAG_HIDDEN); break;
    default: break;
    }

    splash_mascot_set_visible(screen != SCREEN_SPLASH);
    if (logo_img) {
        if (screen == SCREEN_SPLASH) lv_obj_add_flag(logo_img, LV_OBJ_FLAG_HIDDEN);
        else                          lv_obj_clear_flag(logo_img, LV_OBJ_FLAG_HIDDEN);
    }

    if (screen != SCREEN_SPLASH) prev_non_splash_screen = screen;
    current_screen = screen;
    apply_battery_visibility();
}

void ui_toggle_splash(void) {
    if (current_screen == SCREEN_SPLASH) ui_show_screen(prev_non_splash_screen);
    else                                  ui_show_screen(SCREEN_SPLASH);
}

void ui_next_workspace(void) {
    if (current_screen == SCREEN_SPLASH) ui_show_screen(prev_non_splash_screen);
    else                                  show_workspace((ws_cur + 1) % WS_COUNT);
}

screen_t ui_get_current_screen(void) {
    return current_screen;
}

void ui_update_ble_status(ble_state_t state, const char* name, const char* mac) {
    (void)name; (void)mac;
    bool was_connected = s_ble_connected;
    s_ble_connected = (state == BLE_STATE_CONNECTED);

    if (s_ble_connected && !was_connected) {
        connected_at_ms = lv_tick_get();
        beat_since_connect = false;
    }
    if (!s_ble_connected && was_connected) disconnected_at_ms = lv_tick_get();
    // pair / idle / usage — picked from connection + data freshness.
    update_view_state();
}

void ui_update_battery(int percent, bool charging, bool usb_power) {
    if (!battery_img) return;
    int idx;
    if (percent < 0 && usb_power) {
        idx = 5;   // no battery, running on USB: show the plug-zap glyph, not an empty battery
    } else if (charging) {
        idx = 4;
    } else if (percent < 0) {
        idx = 0;
    } else if (percent <= 10) {
        idx = 0;
    } else if (percent <= 35) {
        idx = 1;
    } else if (percent <= 75) {
        idx = 2;
    } else {
        idx = 3;
    }
    lv_image_set_src(battery_img, &battery_dscs[idx]);
    apply_battery_visibility();
}

void ui_show_alert(const char* message) {
    alert_active = true;
    if (celebrating) end_celebration();
    if (current_screen == SCREEN_SPLASH) ui_show_screen(SCREEN_USAGE);
    splash_mascot_attention(true);
    lv_label_set_long_mode(lbl_anim, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(lbl_anim, L.content_w);
    lv_label_set_text(lbl_anim, message);
    lv_timer_reset(alert_timer);
    lv_timer_resume(alert_timer);
}

void ui_hide_alert(void) {
    if (!alert_active) return;
    alert_active = false;
    splash_mascot_attention(false);
    lv_label_set_long_mode(lbl_anim, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl_anim, LV_SIZE_CONTENT);
    lv_timer_pause(alert_timer);
}

void ui_celebrate(const char* task) {
    if (alert_active) return;           // the alert's Clawd is already out
    if (current_screen == SCREEN_SPLASH) ui_show_screen(SCREEN_USAGE);
    splash_mascot_celebrate();
    if (!task[0]) return;
    lv_label_set_text(lbl_done_task, task);
    // A fixed width, so a name that doesn't fit scrolls.
    lv_obj_set_width(lbl_done_task, LV_MIN(text_width(task, L.stat_value_font), done_task_max_w));
    lv_obj_add_flag(lbl_anim, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(done_box, LV_OBJ_FLAG_HIDDEN);
    celebrating = true;
    celebrate_since = lv_tick_get();
}

void ui_update_stats(const StatsData* stats) {
    stats_data = stats;
    if (stats->days || stats->stats) lv_obj_add_flag(stats_empty, LV_OBJ_FLAG_HIDDEN);
    set_stat_grid(&stats_grid, stats->stat, stats->stats);
    lv_obj_invalidate(heat_obj);
}

void ui_update_sessions(const SessionsData* sessions) {
    s_working = sessions->activity[0] != 0;
    lv_obj_set_flag(sess_empty, LV_OBJ_FLAG_HIDDEN, sessions->rows > 0);
    const int32_t name_h = lv_font_get_line_height(L.stat_value_font);
    const int32_t sub_h  = lv_font_get_line_height(L.stat_label_font);
    const int32_t mark_h = lv_font_get_line_height(L.anim_font);
    const int32_t gap    = name_h / 2;
    // Rows fill the space down to the status line. The daemon sends the ones
    // that need you first.
    const int32_t bottom = L.scr_h + L.anim_y - mark_h - gap;
    const int32_t text_x = L.margin + sess_mark_w;
    const int32_t top    = L.content_y + sess_use_h;
    // Rows get their second line while every session fits that way, and one
    // line each otherwise (always on small screens). When even that runs out,
    // the last slot says how many are left.
    const int total = LV_MAX(sessions->total, sessions->rows);
    int32_t need = top - gap;
    for (int i = 0; i < sessions->rows; i++) {
        const auto& r = sessions->row[i];
        need += name_h + gap + (r.doing[0] || r.branch[0] ? sub_h : 0);
    }
    const bool compact = L.scr_h < 300 || total > sessions->rows || need > bottom;
    const int fit = LV_MAX((bottom - top + gap) / (name_h + gap), 1);
    int rows = compact ? LV_MIN(sessions->rows, fit) : sessions->rows;
    if (rows < total) rows = LV_MIN(rows, fit - 1);
    int32_t y = top;
    int last = -1;
    char buf[12];
    for (int i = 0; i < SESSION_ROWS; i++) {
        SessRow& w = sess[i];
        const auto& r = sessions->row[i];
        const bool shown = i < rows;
        const bool two = shown && !compact && (r.doing[0] || r.branch[0]);
        const int32_t h = name_h + (two ? sub_h : 0);
        sess_state[i] = shown ? r.state : 0;
        lv_obj_set_flag(w.mark, LV_OBJ_FLAG_HIDDEN, !shown);
        lv_obj_set_flag(w.name, LV_OBJ_FLAG_HIDDEN, !shown);
        lv_obj_set_flag(w.time, LV_OBJ_FLAG_HIDDEN, !shown);
        lv_obj_set_flag(w.branch, LV_OBJ_FLAG_HIDDEN, !two || !shown);
        lv_obj_set_flag(w.doing, LV_OBJ_FLAG_HIDDEN, !two || !shown);
        lv_obj_add_flag(w.rule, LV_OBJ_FLAG_HIDDEN);
        if (!shown) continue;
        if (last >= 0) lv_obj_clear_flag(sess[last].rule, LV_OBJ_FLAG_HIDDEN);
        last = i;

        const bool done = r.state == 'd';
        if (r.mins == 0)     snprintf(buf, sizeof(buf), "now");
        else if (r.mins < 60) snprintf(buf, sizeof(buf), done ? "%um ago" : "%um", r.mins);
        else                 snprintf(buf, sizeof(buf), done ? "%uh ago" : "%uh", r.mins / 60);
        lv_label_set_text(w.time, buf);
        const int32_t time_w = text_width(buf, L.stat_label_font);
        // Same baseline as the name.
        lv_obj_set_pos(w.time, L.margin + L.content_w - time_w,
                       y + name_h - L.stat_value_font->base_line - sub_h + L.stat_label_font->base_line);

        lv_label_set_text_static(w.mark, spinner_frames[done ? 0 : r.state == 'a' ? 3 : 1]);
        lv_obj_set_style_text_color(w.mark, done ? COL_DIM : COL_ACCENT, 0);
        lv_obj_set_style_opa(w.mark, LV_OPA_COVER, 0);
        lv_obj_set_pos(w.mark, L.margin, y + (name_h - mark_h) / 2);

        lv_label_set_text(w.name, r.name);
        lv_obj_set_style_text_color(w.name, done ? COL_DIM : COL_TEXT, 0);
        lv_obj_set_size(w.name, L.content_w - sess_mark_w - time_w - L.margin / 2, name_h);
        lv_obj_set_pos(w.name, text_x, y);

        // The branch goes under the name and what it is doing under the time.
        if (two) {
            lv_label_set_text_fmt(w.doing, r.state == 'w' ? "%s\xE2\x80\xA6" : "%s", r.doing);
            lv_obj_set_style_text_color(w.doing, r.state == 'a' ? COL_ACCENT : COL_DIM, 0);
            const int32_t doing_w = text_width(lv_label_get_text(w.doing), L.stat_label_font);
            lv_obj_set_pos(w.doing, L.margin + L.content_w - doing_w, y + name_h);
            lv_label_set_text(w.branch, r.branch);
            lv_obj_set_size(w.branch, L.content_w - sess_mark_w - doing_w - L.margin / 2, sub_h);
            lv_obj_set_pos(w.branch, text_x, y + name_h);
        }
        lv_obj_set_pos(w.rule, L.margin, y + h + gap / 2);
        y += h + gap;
    }
    lv_obj_set_flag(sess_more, LV_OBJ_FLAG_HIDDEN, rows >= total);
    if (rows < total) {
        if (last >= 0) lv_obj_clear_flag(sess[last].rule, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text_fmt(sess_more, "+%d more", total - rows);
        lv_obj_set_pos(sess_more, text_x, y + (name_h - sub_h) / 2);
    }
}

void ui_update_today(const TodayData* today) {
    today_data = today;
    lv_obj_set_flag(today_empty, LV_OBJ_FLAG_HIDDEN, today->stats > 0);
    set_stat_grid(&today_grid, today->stat, today->stats);
    lv_obj_invalidate(hours_obj);
}
