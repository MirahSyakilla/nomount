/*
 * nm.c - NoMount CLI Userspace Tool
 *
 * Talks to the hookless NoMount runtime that ships inside KernelSU-Next.
 * The kernel side is driven through the Linux keyring: the command payload is
 * handed to the kernel with add_key(2) using the custom "nomount" key type,
 * and the kernel applies the command while that key type preparses the
 * payload.  The permission check in the kernel requires CAP_SYS_ADMIN in the
 * initial user namespace.
 *
 * The command numbers below mirror kernel/nomount/nomount.h and the packed
 * payload layout mirrors kernel/nomount/keyring.c.  Rules are global: one
 * rule per virtual path, matched exactly or as a directory prefix.
 */
#include "nm.h"

static noinline void parse_arguments(long argc, char **argv, struct nm_args *parsed_args) {
    parsed_args->action = ACTION_NONE;
    parsed_args->data_start_idx = 2;
    parsed_args->is_json = 0;
    parsed_args->is_whiteout = 0;
    parsed_args->has_uid = 0;
    parsed_args->target_uid = 0;

    if (argc < 2) return;

    const char *c1 = argv[1];
    if (strcmp(c1, "rule") == 0 && argc >= 3) {
        const char *c2 = argv[2];
        if (strcmp(c2, "add") == 0) parsed_args->action = ACTION_RULE_ADD;
        else if (strcmp(c2, "del") == 0) parsed_args->action = ACTION_RULE_DEL;
        else if (strcmp(c2, "list") == 0) parsed_args->action = ACTION_RULE_LIST;
        else if (strcmp(c2, "clear") == 0) parsed_args->action = ACTION_RULE_CLEAR;
        parsed_args->data_start_idx = 3;
    } else if (strcmp(c1, "uid") == 0 && argc >= 3) {
        const char *c2 = argv[2];
        if (strcmp(c2, "add") == 0) parsed_args->action = ACTION_UID_ADD;
        else if (strcmp(c2, "del") == 0) parsed_args->action = ACTION_UID_DEL;
        else if (strcmp(c2, "list") == 0) parsed_args->action = ACTION_UID_LIST;
        else if (strcmp(c2, "clear") == 0) parsed_args->action = ACTION_UID_CLEAR;
        else if (strcmp(c2, "block_isolated") == 0) parsed_args->action = ACTION_BLOCK_ISOLATED;
        parsed_args->data_start_idx = 3;
    } else if (strcmp(c1, "clear") == 0) {
        if (argc >= 3 && strcmp(argv[2], "rules") == 0) parsed_args->action = ACTION_RULE_CLEAR;
        else if (argc >= 3 && strcmp(argv[2], "uid") == 0) parsed_args->action = ACTION_UID_CLEAR;
        else parsed_args->action = ACTION_CLEAR_ALL;
        parsed_args->data_start_idx = (argc >= 3) ? 3 : 2;
    }

    /* Legacy Commands */
    else if (strcmp(c1, "add") == 0 || strcmp(c1, "a") == 0) { parsed_args->action = ACTION_RULE_ADD; }
    else if (strcmp(c1, "w") == 0 || strcmp(c1, "whiteout") == 0) { parsed_args->action = ACTION_RULE_ADD; parsed_args->is_whiteout = 1; }
    else if (strcmp(c1, "del") == 0 || strcmp(c1, "d") == 0) { parsed_args->action = ACTION_RULE_DEL; }
    else if (strcmp(c1, "block") == 0 || strcmp(c1, "b") == 0) { parsed_args->action = ACTION_UID_ADD; }
    else if (strcmp(c1, "unblock") == 0 || strcmp(c1, "u") == 0) { parsed_args->action = ACTION_UID_DEL; }
    else if (strcmp(c1, "list") == 0 || strcmp(c1, "l") == 0) {
        if (argc >= 3 && strcmp(argv[2], "uid") == 0) { parsed_args->action = ACTION_UID_LIST; parsed_args->data_start_idx = 3; }
        else { parsed_args->action = ACTION_RULE_LIST; }
    } else if (strcmp(c1, "version") == 0 || strcmp(c1, "v") == 0 || strcmp(c1, "-v") == 0) { parsed_args->action = ACTION_VERSION; }
}

static noinline int parse_uid(const char *s, unsigned int *out) {
    unsigned int uid = 0;
    if (!s || !*s) return -1;
    while (*s) {
        if (*s < '0' || *s > '9') return -1;
        uid = (uid << 3) + (uid << 1) + (*s++ - '0');
    }
    *out = uid;
    return 0;
}

static noinline void report_unsupported(const char *feature) {
    print_literal("nm: ");
    print_strn(feature, strlen(feature));
    print_literal(" is not supported by the KernelSU-Next hookless NoMount kernel\n");
}

static noinline void report_errno(const char *what, int err) {
    print_literal("nm: ");
    print_strn(what, strlen(what));
    print_literal(" failed (errno ");
    struct nm_output out = { nm_out_buf, 0, NM_OUT_BUF_SIZE };
    list_print_uint(&out, err < 0 ? -err : err);
    list_print_literal(&out, ")\n");
    list_flush(&out);
}

/*
 * Asks the kernel for its ABI version.  A mismatch is reported instead of
 * letting the command payload be mis-parsed, and a missing key type is
 * reported as the kernel not providing the runtime at all.
 */
static noinline int nm_query_version(struct nm_version *version) {
    nm_cmd_payload->magic = NM_MAGIC_SIG;
    nm_cmd_payload->cmd = NM_CMD_GET_VERSION;
    nm_cmd_payload->target_uid = 0;
    nm_cmd_payload->arg1 = 0;
    nm_cmd_payload->data_size = 0;
    nm_cmd_payload->status = -1;

    if (nm_send_payload(nm_cmd_payload) < 0) {
        if (nm_last_errno == -NM_ENODEV || nm_last_errno == -NM_ENOSYS) {
            print_literal("nm: the kernel does not provide the NoMount key type; "
                          "reboot into a kernel with the hookless NoMount runtime\n");
        } else {
            report_errno("the NoMount keyring command", (int)nm_last_errno);
        }
        return -1;
    }
    if (nm_cmd_payload->data_size < sizeof(struct nm_version)) {
        print_literal("nm: the kernel returned a malformed version\n");
        return -1;
    }

    memcpy(version, nm_cmd_payload->buffer, sizeof(*version));
    return 0;
}

static noinline int nm_check_kernel(struct nm_version *version) {
    if (nm_query_version(version) < 0)
        return -1;

    struct nm_output out = { nm_out_buf, 0, NM_OUT_BUF_SIZE };

    if (version->abi != NM_KERNEL_ABI_VERSION) {
        print_literal("nm: kernel ABI version ");
        list_print_uint(&out, version->abi);
        list_print_literal(&out, " does not match the client ABI version ");
        list_print_uint(&out, NM_KERNEL_ABI_VERSION);
        list_print_literal(&out, "\n");
        list_flush(&out);
        return -1;
    }
    if (version->transport != NM_KERNEL_TRANSPORT_VERSION) {
        print_literal("nm: kernel transport version ");
        list_print_uint(&out, version->transport);
        list_print_literal(&out, " does not match the client transport version ");
        list_print_uint(&out, NM_KERNEL_TRANSPORT_VERSION);
        list_print_literal(&out, "\n");
        list_flush(&out);
        return -1;
    }
    return 0;
}

/* Streams the rule list out of the kernel and renders it as text or JSON. */
static noinline int nm_dump_rules(struct nm_output *out, int is_json) {
    int offset = 2;
    int started = 0;
    unsigned long long token = ~0ULL;
    unsigned int pages = 0;

    nm_cmd_payload->cmd = NM_CMD_GET_LIST;
    nm_cmd_payload->arg1 = 0;

    for (;;) {
        int res = nm_send_payload(nm_cmd_payload);
        if (res < 0) return res;
        if (nm_cmd_payload->data_size == 0) break;
        /* The kernel advances the continuation token with every page.  Stop
         * when it does not, so that a kernel that keeps returning the same
         * page cannot spin this loop forever. */
        if (nm_cmd_payload->arg1 == token) break;
        token = nm_cmd_payload->arg1;
        if (++pages > 4096) break;

        char *data = nm_cmd_payload->buffer;
        unsigned int used = 0;
        unsigned int size = nm_cmd_payload->data_size;

        while (used + NM_RULE_HDR_SIZE <= size) {
            unsigned int flags = 0;
            unsigned short v_len = 0, r_len = 0;
            const char *v, *r;

            memcpy(&flags, data + used, sizeof(flags));
            memcpy(&v_len, data + used + 4, sizeof(v_len));
            memcpy(&r_len, data + used + 6, sizeof(r_len));
            used += NM_RULE_HDR_SIZE;

            if (used + v_len + r_len > size) break;

            v = data + used;
            used += v_len;
            r = data + used;
            used += r_len;

            if (v_len == 0) continue;

            int is_whiteout = (flags & NM_FLAG_WHITEOUT) != 0;
            int is_virtual_dir = (flags & NM_FLAG_IS_DIR) != 0;

            if (!started) {
                started = 1;
                if (is_json) list_print_literal(out, "[\n");
            }

            if (is_json) {
                list_print_literal_offset(out, ",\n  {\n    \"virtual\": \"", offset);
                offset = 0;
                list_print_json_strn(out, v, v_len);
                if (is_whiteout) {
                    list_print_literal(out, "\",\n    \"whiteout\": true");
                } else {
                    list_print_literal(out, "\",\n    \"real\": \"");
                    list_print_json_strn(out, r, r_len);
                    list_print_literal(out, "\"");
                    if (is_virtual_dir)
                        list_print_literal(out, ",\n    \"virtual_dir\": true");
                }
                list_print_literal(out, "\n  }");
            } else {
                list_print_strn(out, v, v_len);
                if (is_whiteout) {
                    list_print_literal(out, " (whiteout)");
                } else {
                    list_print_literal(out, " -> ");
                    list_print_strn(out, r, r_len);
                    if (is_virtual_dir)
                        list_print_literal(out, " (virtual dir)");
                }
                list_print_literal(out, "\n");
            }
        }
    }

    if (is_json) {
        if (!started) list_print_literal(out, "[\n");
        list_print_literal(out, "\n]\n");
    }
    list_flush(out);
    return 0;
}

/* --- MAIN --- */
__attribute__((noreturn, used))
void c_main(long *sp) {
    long argc = *sp;
    char **argv = (char **)(sp + 1);
    int exit_code = NM_EXIT_OK;

    struct nm_args args;
    struct nm_version version;
    parse_arguments(argc, argv, &args);

    if (args.action == ACTION_NONE) {
        print_literal("usage: nm <rule|uid|clear|version> ...\n");
        exit_code = NM_EXIT_USAGE;
        goto do_exit;
    }

    /* Strip the option flags out of the positional argument list. */
    int p_count = 0;
    int uid_requested = 0;
    if (argc >= 2) {
        for (int i = args.data_start_idx; i < argc; i++) {
            if (strcmp(argv[i], "--uid") == 0) {
                uid_requested = 1;
                if (i + 1 < argc) i++;
            } else if (strcmp(argv[i], "--json") == 0) {
                args.is_json = 1;
            } else if ((args.action == ACTION_RULE_LIST || args.action == ACTION_UID_LIST) &&
                       strcmp(argv[i], "json") == 0) {
                args.is_json = 1;
            } else if (strcmp(argv[i], "--whiteout") == 0) {
                args.is_whiteout = 1;
            } else {
                argv[p_count++] = argv[i];
            }
        }
    }

    if (uid_requested) {
        report_unsupported("per-UID rule scoping");
        exit_code = NM_EXIT_UNSUP;
        goto do_exit;
    }

    switch (args.action) {
        case ACTION_RULE_CLEAR:
            report_unsupported("selective rule clear");
            exit_code = NM_EXIT_UNSUP;
            goto do_exit;

        case ACTION_UID_LIST:
            report_unsupported("listing excluded UIDs");
            exit_code = NM_EXIT_UNSUP;
            goto do_exit;

        case ACTION_UID_CLEAR:
            report_unsupported("selective UID clear");
            exit_code = NM_EXIT_UNSUP;
            goto do_exit;

        case ACTION_BLOCK_ISOLATED:
            report_unsupported("blocking isolated UIDs");
            exit_code = NM_EXIT_UNSUP;
            goto do_exit;

        default:
            break;
    }

    nm_cmd_payload->magic = NM_MAGIC_SIG;

    if (args.action == ACTION_VERSION) {
        if (nm_query_version(&version) < 0) {
            exit_code = NM_EXIT_KERNEL;
            goto do_exit;
        }
        struct nm_output out = { nm_out_buf, 0, NM_OUT_BUF_SIZE };
        list_print_uint(&out, version.abi);
        list_print_literal(&out, "\n");
        list_flush(&out);
        goto do_exit;
    }

    /* Every other command first makes sure that the kernel speaks the same
     * ABI, so that a mismatched pair is detected instead of mis-parsed. */
    if (nm_check_kernel(&version) < 0) {
        exit_code = NM_EXIT_KERNEL;
        goto do_exit;
    }

    switch (args.action) {
        case ACTION_RULE_ADD:
        case ACTION_RULE_DEL: {
            int step = (args.action == ACTION_RULE_ADD && !args.is_whiteout) ? 2 : 1;
            if (p_count < step) goto do_exit;

            const char *cwd = (sys3(SYS_GETCWD, (long)nm_cwd_buf, PATH_MAX, 0) > 0) ? nm_cwd_buf : "/";
            int target_cmd = (args.action == ACTION_RULE_DEL) ? NM_CMD_DEL_RULE : NM_CMD_ADD_RULE;
            int header_size = (target_cmd == NM_CMD_ADD_RULE) ? NM_RULE_HDR_SIZE : NM_DEL_HDR_SIZE;
            char *cursor = nm_cmd_payload->buffer;
            int pending = 0;

            nm_cmd_payload->cmd = target_cmd;
            nm_cmd_payload->arg1 = 0;
            nm_cmd_payload->data_size = 0;

            for (int i = 0; i + step - 1 < p_count; i += step) {
                int v_len = resolved_path_length(cwd, argv[i]);
                if (v_len <= 0) { exit_code = NM_EXIT_USAGE; continue; }

                int r_len = 0;
                if (target_cmd == NM_CMD_ADD_RULE && !args.is_whiteout) {
                    r_len = resolved_path_length(cwd, argv[i + 1]);
                    if (r_len <= 0) { exit_code = NM_EXIT_USAGE; continue; }
                }

                int record_size = header_size + v_len + r_len;
                if (record_size > NM_PAYLOAD_BUFFER_SIZE) { exit_code = NM_EXIT_USAGE; continue; }

                if (pending + record_size > NM_PAYLOAD_BUFFER_SIZE) {
                    nm_cmd_payload->data_size = pending;
                    int res = nm_send_payload(nm_cmd_payload);
                    if (res < 0) exit_code = NM_EXIT_KERNEL;
                    cursor = nm_cmd_payload->buffer;
                    pending = 0;
                }

                if (target_cmd == NM_CMD_ADD_RULE) {
                    unsigned int flags = args.is_whiteout ? NM_FLAG_WHITEOUT : 0;
                    unsigned short v_store = (unsigned short)v_len;
                    unsigned short r_store = (unsigned short)r_len;

                    memcpy(cursor, &flags, sizeof(flags));
                    memcpy(cursor + 4, &v_store, sizeof(v_store));
                    memcpy(cursor + 6, &r_store, sizeof(r_store));
                    cursor += NM_RULE_HDR_SIZE;

                    cursor = resolve_path(cursor, cwd, argv[i]);
                    if (r_len > 0)
                        cursor = resolve_path(cursor, cwd, argv[i + 1]);
                } else {
                    unsigned short v_store = (unsigned short)v_len;

                    memcpy(cursor, &v_store, sizeof(v_store));
                    cursor += NM_DEL_HDR_SIZE;
                    cursor = resolve_path(cursor, cwd, argv[i]);
                }

                pending += record_size;
            }

            if (pending) {
                nm_cmd_payload->data_size = pending;
                int res = nm_send_payload(nm_cmd_payload);
                if (res < 0) exit_code = NM_EXIT_KERNEL;
            }
            break;
        }

        case ACTION_UID_ADD:
        case ACTION_UID_DEL: {
            if (p_count < 1) goto do_exit;

            int target_cmd = (args.action == ACTION_UID_ADD) ? NM_CMD_ADD_UID : NM_CMD_DEL_UID;
            for (int i = 0; i < p_count; i++) {
                unsigned int uid = 0;
                if (parse_uid(argv[i], &uid) != 0) { exit_code = NM_EXIT_USAGE; continue; }
                nm_cmd_payload->cmd = target_cmd;
                nm_cmd_payload->target_uid = uid;
                nm_cmd_payload->arg1 = 0;
                nm_cmd_payload->data_size = 0;
                int res = nm_send_payload(nm_cmd_payload);
                if (res < 0) exit_code = NM_EXIT_KERNEL;
            }
            break;
        }

        case ACTION_CLEAR_ALL: {
            nm_cmd_payload->cmd = NM_CMD_CLEAR_ALL;
            nm_cmd_payload->target_uid = 0;
            nm_cmd_payload->arg1 = 0;
            nm_cmd_payload->data_size = 0;
            int res = nm_send_payload(nm_cmd_payload);
            if (res < 0) exit_code = NM_EXIT_KERNEL;
            break;
        }

        case ACTION_RULE_LIST: {
            struct nm_output out = { nm_out_buf, 0, NM_OUT_BUF_SIZE };
            if (nm_dump_rules(&out, args.is_json) < 0)
                exit_code = NM_EXIT_KERNEL;
            break;
        }

        default:
            exit_code = NM_EXIT_USAGE;
            break;
    }

do_exit:
    sys1(SYS_EXIT, exit_code);
    __builtin_unreachable();
}
