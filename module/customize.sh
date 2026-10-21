ui_print " "
ui_print "======================================="
ui_print "                NoMount                "
ui_print "  Native Kernel Injection Metamodule   "
ui_print "======================================="
ui_print " "

ui_print "- Device Architecture: $ARCH"

# Check root implementation
if [ "$KSU" = "true" ] || command -v ksud >/dev/null 2>&1 || [ -x /data/adb/ksu/bin/ksud ];  then
  ROOT_IMP=ksu
  ui_print "- Root implementation: KernelSU"
elif [ "$APATCH" = "true" ] || command -v apd >/dev/null 2>&1 || [ -x /data/adb/ap/bin/apd ]; then
  ROOT_IMP=ap
  ui_print "- Root implementation: APatch"
else
  abort "! Unsupported root env"
fi

if [ ! -f "$MODPATH/bin/nm-$ARCH" ]; then
  abort "! Unsupported architecture: $ARCH"
fi
mv "$MODPATH/bin/nm-$ARCH" "$MODPATH/bin/nm"
set_perm "$MODPATH/bin/nm" 0 0 0755
if [ -f "$MODPATH/bin/nomount-selftest" ]; then
  set_perm "$MODPATH/bin/nomount-selftest" 0 0 0755
fi
rm -rf "$MODPATH"/bin/nm-*

mkdir -p "/data/adb/$ROOT_IMP/bin"
if ln -sf "/data/adb/modules/nomount/bin/nm" "/data/adb/$ROOT_IMP/bin/nm"; then
    ui_print "- Symlink created."
else
    ui_print "! Failed to create 'nm' symlink, skipping.."
fi

KVER=$(uname -r | cut -d'.' -f1,2)
AKVER=$(uname -r | grep -oE 'android[0-9]+')

if [ -n "$AKVER" ]; then
  ui_print "- Detected Kernel: $KVER ($AKVER branch)"
else
  ui_print "- Detected Kernel: $KVER (Custom/Unknown branch)"
fi

ui_print "- Checking Kernel support via Netlink..."

if "$MODPATH/bin/nm" version > /dev/null 2>&1; then
  ui_print "  [OK] NoMount Netlink interface detected."
  ui_print "  [OK] System is ready for injection."
else
  ui_print " "
  ui_print "***************************************************"
  ui_print "* [!] WARNING: KERNEL DRIVER NOT DETECTED         *"
  ui_print "***************************************************"
  ui_print "* The NoMount Netlink interface is unresponsive.  *"
  ui_print "* *"
  ui_print "* This module will NOT FUNCTION until you flash   *"
  ui_print "* a Kernel compiled with KSU_KPROBES_NOMOUNT=y   *"
  ui_print "***************************************************"
  ui_print " "

  touch "$MODPATH/disable"
fi

NOMOUNT_DATA="/data/adb/nomount"
mkdir -p "$NOMOUNT_DATA"
# Keep exclusions and user options across updates; only the boot marker is
# transient and must not survive a reinstall.
rm -f "$NOMOUNT_DATA/.booting"

ui_print "- Installation complete."
