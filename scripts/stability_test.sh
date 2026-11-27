#!/usr/bin/env bash
# stability_test.sh
#
# Spawns a load of processes that will exit after ~50 ms, spawns procmon,
# runs it next to that load for N minutes, and makes sure procmon never crashes
# or hangs.
#
# Usage:
#   scripts/stability_test.sh [MINUTES] [INTERVAL_SECONDS]
#       Automated testing mode (-b mode). Default: 10 minutes, 0.5 seconds.
#   scripts/stability_test.sh --load-only [MINUTES]
#       Spawns only the load, allowing you to open ./procmon in another terminal
#       and take screen captures.
#
# PASS if procmon exits with return code 0 and outputs every refresh.

cd "$(dirname "$0")/.." || exit 2

LOAD_ONLY=0
if [ "${1:-}" = "--load-only" ]; then LOAD_ONLY=1; shift; fi
MINUTES=${1:-10}
INTERVAL=${2:-0.5}
SECONDS_TOTAL=$(awk "BEGIN { printf \"%d\", $MINUTES * 60 }")

# The load
short_lived_load() {
    local end=$((SECONDS + SECONDS_TOTAL)) i
    while [ "$SECONDS" -lt "$end" ]; do
        if [ -x scripts/short_lived ]; then
            scripts/short_lived 20
            sleep 0.1
        else
            for i in $(seq 20); do sleep 0.05 & done
            wait
        fi
    done
}

( short_lived_load ) &
LOAD_PID=$!
trap 'kill "$LOAD_PID" 2>/dev/null; pkill -P "$LOAD_PID" 2>/dev/null; wait "$LOAD_PID" 2>/dev/null' EXIT

if [ "$LOAD_ONLY" -eq 1 ]; then
    echo "Short-lived load running for $MINUTES minute(s). Open ./procmon in another terminal."
    echo "Press Ctrl+C here to stop early."
    wait "$LOAD_PID"
    exit 0
fi

[ -x ./procmon ] || make || { echo "build failed"; exit 2; }

COUNT=$(awk "BEGIN { printf \"%d\", $MINUTES * 60 / $INTERVAL }")
OUT=$(mktemp); ERR=$(mktemp)
echo "Stability test: $MINUTES min, refresh every ${INTERVAL}s ($COUNT refreshes), short-lived load running."

START=$(date +%s)
timeout $((SECONDS_TOTAL + 60)) ./procmon -b -d "$INTERVAL" -n "$COUNT" > "$OUT" 2> "$ERR" &
PM_PID=$!

# sample procmon's memory at runtime to see if it grows
RSS_FIRST=""; RSS_LAST=""; RSS_MAX=0
while kill -0 "$PM_PID" 2>/dev/null; do
    RSS=$(awk '/^VmRSS:/ {print $2}' "/proc/$PM_PID/status" 2>/dev/null)
    if [ -n "$RSS" ]; then
        [ -z "$RSS_FIRST" ] && RSS_FIRST=$RSS
        RSS_LAST=$RSS
        [ "$RSS" -gt "$RSS_MAX" ] && RSS_MAX=$RSS
    fi
    sleep 5
done
wait "$PM_PID"; RC=$?
ELAPSED=$(( $(date +%s) - START ))

REFRESHES=$(grep -c '^Linux Process Monitor' "$OUT")
CLEAN=$(tail -n 1 "$OUT" | grep -c 'exited cleanly')

PASS=1
[ "$RC" -eq 0 ]              || { PASS=0; REASON="exit code $RC (124 = frozen/timeout, >128 = crashed)"; }
[ "$REFRESHES" -eq "$COUNT" ] || { PASS=0; REASON="${REASON:+$REASON; }expected $COUNT refreshes, saw $REFRESHES"; }
[ "$CLEAN" -eq 1 ]           || { PASS=0; REASON="${REASON:+$REASON; }no 'exited cleanly' line"; }

echo
echo "---- stability test result ----"
echo "date:            $(date '+%Y-%m-%d %H:%M')"
echo "duration:        ${ELAPSED}s (planned ${SECONDS_TOTAL}s)"
echo "refresh interval: ${INTERVAL}s"
echo "refreshes:       $REFRESHES of $COUNT"
echo "exit code:       $RC"
echo "procmon RSS:     first ${RSS_FIRST:-?} kB, last ${RSS_LAST:-?} kB, max $RSS_MAX kB"
echo "stderr lines:    $(wc -l < "$ERR")"
if [ "$PASS" -eq 1 ]; then echo "RESULT: PASS"; else echo "RESULT: FAIL - $REASON"; fi
rm -f "$OUT" "$ERR"
[ "$PASS" -eq 1 ]
