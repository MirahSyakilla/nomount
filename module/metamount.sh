#!/system/bin/sh

MODDIR=${0%/*}
LOADER="$MODDIR/bin/nm"
MODULES_DIR="/data/adb/modules"
NOMOUNT_DATA="/data/adb/nomount"
LOG_FILE="$NOMOUNT_DATA/nomount.log"
VERBOSE_FLAG="$NOMOUNT_DATA/.verbose"
TARGET_PARTITIONS="system system_ext vendor odm product apex oem optics prism
                    mi_ext my_bigball my_carrier my_company my_engineering my_heytap
                    my_manifest my_preload my_product my_region my_reserve my_stock"

echo "=== NoMount Injection | Started: $(date) ===" >> "$LOG_FILE"

if ! "$LOADER" version > /dev/null 2>&1; then
    echo "[FATAL] NoMount Netlink interface missing/unresponsive; skipping rule injection." >> "$LOG_FILE"
    exit 1
fi

if [ -f "$NOMOUNT_DATA/disable" ]; then
    echo "[INFO] Safe Mode active. Skipping rule injection." >> "$LOG_FILE"
    exit 0
fi

VERBOSE=false
if [ -f "$VERBOSE_FLAG" ]; then
    VERBOSE=true
    echo "[CONFIG] Verbose Mode: ON" >> "$LOG_FILE"
else
    echo "[CONFIG] Verbose Mode: OFF" >> "$LOG_FILE"
fi

for mod_path in "$MODULES_DIR"/*; do
    [ -d "$mod_path" ] || continue
    mod_name="${mod_path##*/}"
    [ "$mod_name" = "nomount" ] && continue

    if [ -f "$mod_path/disable" ] || [ -f "$mod_path/remove" ] || [ -f "$mod_path/skip_mount" ]; then
        if $VERBOSE; then echo "[SKIP] Module $mod_name is disabled/removed/skipped" >> "$LOG_FILE"; fi
        continue
    fi

    for partition in $TARGET_PARTITIONS; do
        [ -d "$mod_path/$partition" ] || continue
        [ -d "/$partition" ] || [ -d "/system/$partition" ] || continue

        echo "[INFO] Mounting module: $mod_name (/$partition)" >> "$LOG_FILE"

        # Directories, character devices and .replace markers are hidden.
        find -L "$mod_path/$partition" \( -type d -o -type c -o -name ".replace" \) \
            -exec sh "$MODDIR/rule-paths.sh" path "$mod_path" {} + 2>/dev/null \
            | xargs -0 -r -n 200 "$LOADER" rule add --whiteout >> "$LOG_FILE" 2>&1

        # Regular files and symlinks are redirected to the module copy.
        find -L "$mod_path/$partition" \( -type f -o -type l \) ! -name ".replace" \
            -exec sh "$MODDIR/rule-paths.sh" pair "$mod_path" {} + 2>/dev/null \
            | xargs -0 -r -n 200 "$LOADER" rule add >> "$LOG_FILE" 2>&1
    done
done

echo "=== Injection Complete: $(date) ===" >> "$LOG_FILE"

if $VERBOSE; then
    echo "Current files injected:" >> "$LOG_FILE"
    "$LOADER" rule list >> "$LOG_FILE"
fi

exit 0
