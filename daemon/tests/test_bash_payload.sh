#!/bin/bash
# Regression test for build_payload_for_token() and _payload_session_pct() in
# claude-usage-daemon.sh. curl is stubbed to print canned response headers.
set -u

DAEMON="$(dirname "$0")/../claude-usage-daemon.sh"

extract() { awk -v fn="$1" '$0 ~ "^"fn"\\(\\) \\{"{f=1} f{print} f&&/^\}/{exit}' "$DAEMON"; }
eval "$(extract build_payload_for_token)"
eval "$(extract _payload_session_pct)"
read_clock_setting() { echo off; }
read_chime_setting() { echo off; }

fail=0
check() {  # check <label> <got> <want>
    if [ "$2" = "$3" ]; then echo "PASS: $1"; else echo "FAIL: $1: got '$2' want '$3'"; fail=1; fi
}

NOW=$(date +%s)
DAY=$((NOW + 86400 + 20))
FIVE=$((NOW + 3600 + 20))

HEADERS=""
curl() { cat >/dev/null; printf '%s' "$HEADERS"; }

# A verified Ultra account: 7d only, no 5h window.
HEADERS=$(printf 'HTTP/2 200\r\nanthropic-ratelimit-unified-status: allowed\r\nanthropic-ratelimit-unified-7d-utilization: 0.06\r\nanthropic-ratelimit-unified-7d-reset: %s\r\n' "$DAY")
got=$(build_payload_for_token tok)
check "ultra payload" "$got" '{"s":0,"sr":-1,"w":6,"wr":1440,"st":"allowed","acct":"pro","hs":false,"ok":true}'
check "ultra activity reads weekly" "$(_payload_session_pct "$got")" "6"

# Pro/Max: both windows, no "hs" field.
HEADERS=$(printf 'HTTP/2 200\r\nanthropic-ratelimit-unified-status: allowed\r\nanthropic-ratelimit-unified-5h-utilization: 0.34\r\nanthropic-ratelimit-unified-5h-reset: %s\r\nanthropic-ratelimit-unified-5h-status: allowed\r\nanthropic-ratelimit-unified-7d-utilization: 0.5\r\nanthropic-ratelimit-unified-7d-reset: %s\r\n' "$FIVE" "$DAY")
got=$(build_payload_for_token tok)
check "max payload" "$got" '{"s":34,"sr":60,"w":50,"wr":1440,"st":"allowed","acct":"pro","ok":true}'
check "max activity reads session" "$(_payload_session_pct "$got")" "34"

exit $fail
