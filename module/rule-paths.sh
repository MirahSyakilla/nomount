#!/system/bin/sh
# Collects the virtual/real path pairs of a set of module files.
#
#   rule-paths.sh pair <module_dir> <file>...   -> "/virtual\0/real\0" pairs
#   rule-paths.sh path <module_dir> <file>...   -> "/virtual\0" whiteout paths
#
# The vendor-like partitions are exposed by Android both directly and below
# /system, so both spellings are emitted when only one of them exists in the
# module.

kind=$1; mod=$2; shift 2

# emit <virtual> [real]
emit() {
    v="$1"
    case "$v" in
        system/vendor/*|system/product/*|system/system_ext/*|system/odm/*|system/oem/*|system/apex/*|system/optics/*|system/prism/*)
            if [ -n "$2" ]; then
                printf "/%s\0%s\0" "$v" "$2"
                if [ ! -e "$mod/${v#system/}" ] && [ ! -L "$mod/${v#system/}" ]; then
                    printf "/%s\0%s\0" "${v#system/}" "$2"
                fi
            else
                printf "/%s\0" "$v"
                if [ ! -e "$mod/${v#system/}" ] && [ ! -L "$mod/${v#system/}" ]; then
                    printf "/%s\0" "${v#system/}"
                fi
            fi
            ;;
        vendor/*|product/*|system_ext/*|odm/*|oem/*|apex/*|optics/*|prism/*|mi_ext/*|my_*)
            if [ -n "$2" ]; then
                printf "/%s\0%s\0" "$v" "$2"
                if [ ! -e "$mod/system/$v" ] && [ ! -L "$mod/system/$v" ]; then
                    printf "/system/%s\0%s\0" "$v" "$2"
                fi
            else
                printf "/%s\0" "$v"
                if [ ! -e "$mod/system/$v" ] && [ ! -L "$mod/system/$v" ]; then
                    printf "/system/%s\0" "$v"
                fi
            fi
            ;;
        *)
            if [ -n "$2" ]; then
                printf "/%s\0%s\0" "$v" "$2"
            else
                printf "/%s\0" "$v"
            fi
            ;;
    esac
}

for f do
    case "$f" in
        "$mod"/*) v="${f#"$mod"/}";;
        *) v="$f"; f="$mod/$f";;
    esac

    if [ "$kind" = pair ]; then
        emit "$v" "$f"
    elif [ -d "$f" ]; then
        # Plain directories are merged; only opaque ones are hidden.
        case "$(getfattr -n trusted.overlay.opaque "$f" 2>/dev/null)" in
            *"=\"y\""*) emit "$v";;
        esac
    elif [ "${f##*/}" = ".replace" ]; then
        emit "${v%/.replace}"
    else
        emit "$v"
    fi
done
