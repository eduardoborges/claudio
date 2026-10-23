#pragma once
#include <Arduino.h>

struct UsageData {
    float session_pct;       // utilization 0-100 (5h window Pro/Max; spending % Enterprise)
    int session_reset_mins;  // minutes until reset
    float weekly_pct;        // 7-day utilization (Pro/Max only; 0 for Enterprise)
    int weekly_reset_mins;   // minutes until weekly reset (Pro/Max only)
    char status[16];         // "allowed", "limited", etc.
    bool chime;              // play the session-reset chime; false unless daemon opts in
    bool beep;               // Claude needs attention: play the chime once for this packet
    char alert_msg[128];     // attention message for the status line; empty = no alert
    bool alert_clear;        // the session moved on: end the alert
    bool enterprise;         // true = Enterprise spending-limit account
    int time_pct;            // 0-100: fraction of billing period elapsed (Enterprise)
    int period_days;         // total billing period length in days (Enterprise)
    char reset_date[12];     // formatted reset date e.g. "Jul 1" (Enterprise)
    char plan[16];           // subscription, e.g. "Max 20x"; empty = not sent
    long clock_epoch;        // local wall-clock epoch (s) from daemon; 0 = not provided
    int  clock_fmt;          // 12 or 24 (hour format from daemon); defaults to 24
    bool ok;                 // data parse succeeded
    bool valid;              // false until first successful parse
};

#define STATS_WEEKS 26       // heatmap columns; the daemon's STATS_WEEKS must match

struct StatPair { char label[16]; char value[16]; };   // one cell of a stat grid

// Stats workspace, filled by the daemon's "hm" (heatmap) and "st" messages.
struct StatsData {
    uint8_t level[STATS_WEEKS * 7];    // 0..4 per day, week by week from the oldest Sunday
    uint8_t days;                      // valid days; the rest of this week is in the future
    struct { char name[4]; uint8_t col; } month[8];
    uint8_t months;
    StatPair stat[6];
    uint8_t stats;
};

// Today workspace, filled by the daemon's "td" message: today's activity from
// the Claude Code transcripts.
struct TodayData {
    uint8_t hour[24];        // messages per hour, 0..8 relative to the busiest hour
    StatPair stat[6];
    uint8_t stats;
};

#define SESSION_ROWS 6       // the daemon's ROWS must match

// Sessions workspace, filled by the daemon's "ss" message: open Claude Code
// sessions, the ones that need you first.
struct SessionsData {
    struct {
        char name[33];       // "project | title", ASCII
        char state;          // 'a' needs you, 'w' working, 'd' done
        uint16_t mins;       // minutes since the task started (working) or the last event
        char doing[17];      // second line: what it is doing, "" for done
        char branch[25];     // second line too, "" outside git
    } row[SESSION_ROWS];
    uint8_t rows;
    uint8_t total;           // every open session, more than rows when some were left out
    char activity[12];       // animation the corner Clawd plays while a session works, "" = none
    uint32_t done_id;        // when a long task just finished (daemon clock), 0 = none
    char done_task[33];      // its "project | title", "" from an older daemon
    bool beep;               // chime along with that celebration
};
