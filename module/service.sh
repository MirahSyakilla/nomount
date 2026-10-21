#!/system/bin/sh

MODDIR=${0%/*}
NM_BIN="$MODDIR/bin/nm"
NOMOUNT_DATA="/data/adb/nomount"
EXCLUSION_JSON="$NOMOUNT_DATA/.exclusion_list.json"
LEGACY_EXCLUSION_FILE="$NOMOUNT_DATA/.exclusion_list"
DISABLE_FLAG="$NOMOUNT_DATA/disable"

[ -x "$NM_BIN" ] || exit 0
[ -f "$DISABLE_FLAG" ] && exit 0

# The kernel exposes no way to enumerate the excluded UIDs, so the persistent
# store is the source of truth. The legacy plain-text list is still honoured
# so upgrades keep the previous exclusions.
set -f
uids=$(
    {
        [ -f "$EXCLUSION_JSON" ] && grep -o '"uid":"[0-9]*"' "$EXCLUSION_JSON" 2>/dev/null | cut -d'"' -f4
        [ -f "$LEGACY_EXCLUSION_FILE" ] && tr ',; \t\r' '\n' < "$LEGACY_EXCLUSION_FILE" 2>/dev/null
    } | grep -E '^[0-9]+$'
)

[ -z "$uids" ] && exit 0

# Normalize, discard malformed UIDs and keep one UID per entry.
seen=""
for uid in $uids; do
    case " $seen " in
        *" $uid "*) continue ;;
    esac
    seen="$seen $uid"
done

[ -z "$seen" ] && exit 0

# shellcheck disable=SC2086
"$NM_BIN" uid add $seen > /dev/null 2>&1
