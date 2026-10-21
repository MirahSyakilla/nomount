#!/system/bin/sh

MODDIR=${0%/*}
LOADER="$MODDIR/bin/nm"
NOMOUNT_DATA="/data/adb/nomount"
LOG_FILE="$NOMOUNT_DATA/nomount.log"
BOOT_SEMAPHORE="$NOMOUNT_DATA/.booting"
PROP_FILE="$MODDIR/module.prop"
BASE_DESC="A metamodule that replaces OverlayFS/MagicMount with VFS path redirection."

[ -d "$NOMOUNT_DATA" ] || mkdir -p "$NOMOUNT_DATA"

echo "=== NoMount Boot Log | Started: $(date) ===" > "$LOG_FILE"
echo "Kernel Version: $(uname -r)" >> "$LOG_FILE"

# Safe mode: the WebUI or the installer can request that nothing is injected.
if [ -f "$NOMOUNT_DATA/disable" ]; then
    echo "[INFO] Safe Mode active. Skipping NoMount initialization." >> "$LOG_FILE"
    sed -i "s|^description=.*|description=[🛡️ SAFE MODE: Injection Disabled] \\\\n$BASE_DESC|" "$PROP_FILE"
    rm -f "$BOOT_SEMAPHORE"
    exit 0
fi

# Bootloop guard: a marker that survived the previous boot means NoMount
# crashed the device.
if [ -f "$BOOT_SEMAPHORE" ]; then
    echo "[FATAL] Bootloop detected! NoMount caused a crash on the last boot." >> "$LOG_FILE"
    echo "[INFO] Disabling NoMount for safety..." >> "$LOG_FILE"
    touch "$MODDIR/disable"
    sed -i "s|^description=.*|description=[🚨 DISABLED: Bootloop Prevented] \\\\n$BASE_DESC|" "$PROP_FILE"
    rm -f "$BOOT_SEMAPHORE"
    exit 1
fi

touch "$BOOT_SEMAPHORE"

# The NoMount runtime is compiled into KernelSU-Next, so there is no loadable
# kernel module to insert here; only the Generic Netlink family is probed.
echo "[INFO] Checking NoMount kernel support via Generic Netlink..." >> "$LOG_FILE"
if ! "$LOADER" version > /dev/null 2>&1; then
    echo "[FATAL] NoMount Netlink interface missing/unresponsive." >> "$LOG_FILE"
    touch "$MODDIR/disable"
    sed -i "s|^description=.*|description=[❌ ERROR: Kernel not patched] \\\\n$BASE_DESC|" "$PROP_FILE"
    rm -f "$BOOT_SEMAPHORE"
    exit 1
fi
echo "[OK] Netlink socket responding properly." >> "$LOG_FILE"

# Apply the rules of every other module.
sh "$MODDIR/metamount.sh"
inject_rc=$?

if [ "$inject_rc" -eq 0 ]; then
    echo "[OK] Boot phase completed safely." >> "$LOG_FILE"
    sed -i "s|^description=.*|description=$BASE_DESC|" "$PROP_FILE"
else
    echo "[WARN] Rule injection failed (rc=$inject_rc); see the log for details." >> "$LOG_FILE"
fi
rm -f "$BOOT_SEMAPHORE"

exit $inject_rc
