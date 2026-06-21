/*
 * nm.c - NoMount CLI Userspace Tool
 */
#include "nm.h"

/* --- MAIN --- */
__attribute__((noreturn, used))
void c_main(long *sp) {
    long argc = *sp;
    char **argv = (char **)(sp + 1);
    int exit_code = 1;

    if (argc < 2) {
        print_str("nm <command>\n");
        goto do_exit;
    }

    int fd = sys3(SYS_SOCKET, AF_NETLINK, SOCK_RAW, NETLINK_GENERIC);
    if (fd < 0) { exit_code = 2; goto do_exit; }

    int nm_family = -1;
    /* flags: 1 = REQUEST */
    if (do_nm_cmd(fd, 16, 3, 2, "nomount", 8, 1) > 0) { /* 16=CTRL, 3=GETFAMILY, 2=NAME */
        unsigned short *fam_id = get_attr(rx_buf, 1); /* 1=FAMILY_ID */
        if (fam_id) nm_family = *fam_id;
    }
    if (nm_family < 0) { exit_code = 3; goto do_exit; }

    char cmd = argv[1][0];
    if (cmd == 'a' || cmd == 'd') {
        int is_add = (cmd == 'a');
        int step = 1 + is_add;
        if (argc < 2 + step) { exit_code = 0; goto do_exit; }

        const char *cwd = (sys3(SYS_GETCWD, (long)cwd_buf, PATH_MAX, 0) > 0) ? cwd_buf : "/";
        char *cursor = payload;
        int target_cmd = 3 - is_add; /* 2=ADD_RULE, 3=DEL_RULE */
        exit_code = 0;

        for (int i = 2; i + step <= argc; i += step) {
            char *v_end = resolve_path(v_resolved, cwd, argv[i]);
            int v_len = v_end - v_resolved;
            if (!v_len) { exit_code = 3; continue; }

            int r_len = 0;
            if (is_add) {
                char *r_end = resolve_path(r_resolved, cwd, argv[i+1]);
                r_len = r_end - r_resolved;
                if (!r_len) { exit_code = 3; continue; }
            }

            if ((cursor - payload) + 8 + v_len + r_len > MAX_PAYLOAD) {
                /* 6=PAYLOAD, 5=REQ|ACK */
                exit_code |= (do_nm_cmd(fd, nm_family, target_cmd, 6, payload, cursor - payload, 5) < 0);
                cursor = payload;
            }

            *(unsigned short*)(cursor + (is_add << 2)) = v_len;
            if (is_add) {
                *(unsigned int*)cursor = 0; 
                *(unsigned short*)(cursor + 6) = r_len; 
                memcpy(cursor + 8, v_resolved, v_len);
                memcpy(cursor + 8 + v_len, r_resolved, r_len);
                cursor += 8 + v_len + r_len;
            } else {
                memcpy(cursor + 2, v_resolved, v_len); 
                cursor += 2 + v_len;
            }
        }

        if (cursor > payload)
            exit_code |= (do_nm_cmd(fd, nm_family, target_cmd, 6, payload, cursor - payload, 5) < 0);

        goto do_exit;

    } else if (cmd == 'b' || cmd == 'u') {
        if (argc < 3) goto do_exit;
        unsigned int uid = 0; const char *s = argv[2];
        while (*s) uid = (uid << 3) + (uid << 1) + (*s++ - '0');
        /* 5=ADD_UID, 6=DEL_UID, 4=ATTR_UID, 5=REQ|ACK */
        exit_code = (do_nm_cmd(fd, nm_family, 6 - (cmd == 'b'), 4, &uid, 4, 5) < 0);
        goto do_exit;

    } else if (cmd == 'c') {
        /* 4=CLEAR_ALL, 5=REQ|ACK */
        exit_code = (do_nm_cmd(fd, nm_family, 4, 0, (void *)0, 0, 5) < 0);
        goto do_exit;

    } else if (cmd == 'v') {
        if (do_nm_cmd(fd, nm_family, 1, 0, (void *)0, 0, 5) > 0) { /* 1=GET_VERSION */
            unsigned int *ver = get_attr(rx_buf, 5 /* ATTR_VERSION */);
            if (ver) {
                unsigned int v = *ver; char v_str[4] = {0};
                unsigned char tens = ((v << 7) + (v << 6) + (v << 3) + (v << 2) + v) >> 11;
                v = v - ((tens << 3) + (tens << 1));
                v_str[0] = tens + '0'; v_str[1] = v + '0'; v_str[2] = '\n';
                print_str(v_str);
                exit_code = 0; goto do_exit;
            }
        }

    } else if (cmd == 'l') {
        unsigned int len = do_nm_cmd(fd, nm_family, 7, 0, (void *)0, 0, 0x301); /* 7=GET_LIST, 0x301=REQ|DUMP */
        int is_json = (argc > 2 && argv[2][0] == 'j');
        int offset = 2;
        if (is_json) print_str("[\n");

        while (len > 0) {
            for (struct nlmsghdr *msg = (void *)rx_buf; NLMSG_OK(msg, len); msg = NLMSG_NEXT(msg, len)) {
                if (msg->nlmsg_type == 3 || msg->nlmsg_type == 2) goto list_done; /* 3=DONE, 2=ERROR */
                char *v = get_attr(msg, 1); /* 1=VIRTUAL_PATH */
                char *r = get_attr(msg, 2); /* 2=REAL_PATH */

                if (v && r) {
                    if (is_json) {
                        print_str((const char *)",\n  {\n    \"virtual\": \"" + offset); offset = 0;
                        print_str(v); print_str("\",\n    \"real\": \""); print_str(r); print_str("\"\n  }");
                    } else {
                        print_str(v); print_str(" -> "); print_str(r); print_str("\n");
                    }
                }
            }
            len = sys3(SYS_READ, fd, (long)rx_buf, RX_BUF_SIZE);
        }
list_done:
        if (is_json) print_str("\n]\n");
        exit_code = 0;
    }

do_exit:
    sys1(SYS_EXIT, exit_code);
}
