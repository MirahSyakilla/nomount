/*
 * nm.c - NoMount CLI Userspace Tool
 *
 * Talks to the hookless NoMount runtime that ships inside KernelSU-Next.
 * The kernel side speaks Generic Netlink (family "nomount", version 1); the
 * command and attribute numbers below mirror kernel/nomount/nomount.h and the
 * packed payload layout mirrors kernel/nomount/netlink.c.
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

/* Streams the rule list out of the kernel and renders it as text or JSON. */
static noinline int nm_dump_rules(int fd, int family, struct nm_output *out, int is_json) {
    int res = nm_send_raw(fd, family, NM_CMD_GET_LIST, 0, 0, 0, NLM_F_REQUEST | NLM_F_DUMP);
    if (res < 0) return res;

    int offset = 2;
    int started = 0;

    while (res > 0) {
        int remaining = res;
        char *cursor = rx_buf;

        while (remaining >= (int)sizeof(struct nlmsghdr)) {
            struct nlmsghdr *nlh = (void *)cursor;
            int v_len = 0, r_len = 0, f_len = 0;
            const char *v, *r, *f;
            unsigned int flags = 0;

            if (nlh->nlmsg_len < sizeof(struct nlmsghdr) ||
                nlh->nlmsg_len > (unsigned int)remaining)
                break;
            if (nlh->nlmsg_type == NLMSG_DONE)
                goto list_done;
            if (nlh->nlmsg_type == NLMSG_ERROR) {
                int err = *(int *)(cursor + NLMSG_HDRLEN);
                return err ? err : NM_EINVAL;
            }

            v = nm_get_attr(nlh, NM_ATTR_VIRTUAL_PATH, &v_len);
            r = nm_get_attr(nlh, NM_ATTR_REAL_PATH, &r_len);
            f = nm_get_attr(nlh, NM_ATTR_FLAGS, &f_len);
            if (f && f_len >= 4) memcpy(&flags, f, 4);

            if (v && v_len > 0) {
                v_len = nm_str_len(v, v_len);
                if (r && r_len > 0) r_len = nm_str_len(r, r_len);
                if (v_len <= 0) {
                    remaining -= nlh->nlmsg_len;
                    cursor += nlh->nlmsg_len;
                    continue;
                }

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

            remaining -= nlh->nlmsg_len;
            cursor += nlh->nlmsg_len;
        }

        res = (int)sys3(SYS_READ, fd, (long)rx_buf, NM_RX_BUF_SIZE);
        if (res == -NM_EINTR) { res = 0; break; }
    }

list_done:
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

    int fd = sys3(SYS_SOCKET, AF_NETLINK, SOCK_RAW, NETLINK_GENERIC);
    if (fd < 0) { exit_code = NM_EXIT_IO; goto do_exit; }

    int family = nm_family_id(fd);
    if (family < 0) { exit_code = NM_EXIT_KERNEL; goto do_exit; }

    switch (args.action) {
        case ACTION_RULE_ADD:
        case ACTION_RULE_DEL: {
            int step = (args.action == ACTION_RULE_ADD && !args.is_whiteout) ? 2 : 1;
            if (p_count < step) goto do_exit;

            const char *cwd = (sys3(SYS_GETCWD, (long)cwd_buf, PATH_MAX, 0) > 0) ? cwd_buf : "/";
            int target_cmd = (args.action == ACTION_RULE_DEL) ? NM_CMD_DEL_RULE : NM_CMD_ADD_RULE;
            int header_size = (target_cmd == NM_CMD_ADD_RULE) ? NM_RULE_HDR_SIZE : NM_DEL_HDR_SIZE;
            char *cursor = payload;
            int pending = 0;

            for (int i = 0; i + step - 1 < p_count; i += step) {
                int v_len = resolved_path_length(cwd, argv[i]);
                if (v_len <= 0) { exit_code = NM_EXIT_USAGE; continue; }

                int r_len = 0;
                if (target_cmd == NM_CMD_ADD_RULE && !args.is_whiteout) {
                    r_len = resolved_path_length(cwd, argv[i + 1]);
                    if (r_len <= 0) { exit_code = NM_EXIT_USAGE; continue; }
                }

                int record_size = header_size + v_len + r_len;
                if (record_size > NM_MAX_PAYLOAD) { exit_code = NM_EXIT_USAGE; continue; }

                if (pending + record_size > NM_MAX_PAYLOAD) {
                    int res = nm_request(fd, family, target_cmd, NM_ATTR_PAYLOAD,
                                         payload, pending, NLM_F_REQUEST | NLM_F_ACK);
                    if (res < 0) exit_code = NM_EXIT_KERNEL;
                    cursor = payload;
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
                int res = nm_request(fd, family, target_cmd, NM_ATTR_PAYLOAD,
                                     payload, pending, NLM_F_REQUEST | NLM_F_ACK);
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
                int res = nm_request(fd, family, target_cmd, NM_ATTR_UID,
                                     &uid, sizeof(uid), NLM_F_REQUEST | NLM_F_ACK);
                if (res < 0) exit_code = NM_EXIT_KERNEL;
            }
            break;
        }

        case ACTION_CLEAR_ALL: {
            int res = nm_request(fd, family, NM_CMD_CLEAR_ALL, 0, 0, 0,
                                 NLM_F_REQUEST | NLM_F_ACK);
            if (res < 0) exit_code = NM_EXIT_KERNEL;
            break;
        }

        case ACTION_VERSION: {
            int res = nm_request(fd, family, NM_CMD_GET_VERSION, 0, 0, 0,
                                 NLM_F_REQUEST | NLM_F_ACK);
            if (res > 0) {
                int len = 0;
                const char *ver = nm_get_attr(rx_buf, NM_ATTR_VERSION, &len);
                if (ver && len >= 4) {
                    unsigned int value = 0;
                    memcpy(&value, ver, sizeof(value));

                    struct nm_output out = { out_buf, 0, NM_OUT_BUF_SIZE };
                    list_print_uint(&out, value);
                    list_print_literal(&out, "\n");
                    list_flush(&out);
                    break;
                }
            }
            exit_code = NM_EXIT_KERNEL;
            break;
        }

        case ACTION_RULE_LIST: {
            struct nm_output out = { out_buf, 0, NM_OUT_BUF_SIZE };
            if (nm_dump_rules(fd, family, &out, args.is_json) < 0)
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
