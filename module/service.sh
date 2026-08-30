#!/system/bin/sh

MODDIR=${0%/*}
NM_BIN="$MODDIR/bin/nm"
EXCLUSION_JSON="/data/adb/nomount/.exclusion_list.json"

[ -x "$NM_BIN" ] || exit 0
[ -f "$EXCLUSION_JSON" ] || exit 0

uids=$(grep -o '"uid":"[0-9]*"' "$EXCLUSION_JSON" | cut -d'"' -f4)
if [ -n "$uids" ]; then
    set -f
    "$NM_BIN" uid add $uids >/dev/null 2>&1
fi
