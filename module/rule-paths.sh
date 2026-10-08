kind=$1; mod=$2; shift 2
for f do
    case "$f" in
        "$mod"/*) v="${f#"$mod"/}";;
        *) v="$f"; f="$mod/$f";;
    esac
    case "$v" in system/*)
        p="${v#system/}"; p="${p%%/*}"
        case "$p" in vendor|system_ext|product|odm|apex|oem|optics|prism|mi_ext|my_*)
            v="$p${v#system/$p}" ;;
        esac
    ;; esac
    if [ "$kind" = pair ]; then
        printf "/%s\0%s\0" "$v" "$f"
    elif [ -d "$f" ]; then
        case "$(getfattr -n trusted.overlay.opaque "$f" 2>/dev/null)" in *"=\"y\""*) printf "/%s\0" "$v";; esac
    elif [ "${f##*/}" = ".replace" ]; then
        printf "/%s\0" "${v%/.replace}"
    else
        printf "/%s\0" "$v"
    fi
done
