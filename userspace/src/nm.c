/*
 * nm.c - NoMount CLI Userspace Tool
 *
 */
#include "nm.h"

/* --- MAIN --- */
__attribute__((noreturn, used))
void c_main(long *sp) {
    long argc = *sp;
    char **argv = (char **)(sp + 1);
    long exit_code = 1; 
    
    if (argc < 2) {
        printc("nm add|del|cls|blk|unblk|ls\n");
        goto do_exit;
    }

    /* 1. Setup Netlink Socket */
    int fd = sys3(SYS_SOCKET, AF_NETLINK, SOCK_RAW, NETLINK_GENERIC);
    if (fd < 0) { exit_code = 2; goto do_exit; }

    struct sockaddr_nl local = { .nl_family = AF_NETLINK };
    sys3(SYS_BIND, fd, (long)&local, sizeof(local));

    /* 2. Resolve 'nomount' Family ID */
    int nm_family = get_nomount_family_id(fd);
    if (nm_family < 0) {
        printc("Error: NoMount kernel module not loaded.\n");
        exit_code = 3; 
        goto do_exit;
    }

    char cmd = argv[1][0];
    struct nlmsghdr *nlh = NULL;
    int req_flags = NLM_F_REQUEST | NLM_F_ACK;

    switch (cmd) {
        case 'a':
        case 'd': {
            int is_add = (cmd == 'a');
            int step = is_add ? 2 : 1;
            if (argc < 2 + step) goto do_exit;

            long cwd_len = sys2(SYS_GETCWD, (long)cwd_buf, PATH_MAX);
            const char *cwd = (cwd_len > 0) ? cwd_buf : "/";
            exit_code = 0;
            static char payload[MAX_PAYLOAD];
            char *cursor = payload;
            size_t max_payload = MAX_PAYLOAD - (PATH_MAX * 2) - 16;
            int target_cmd = is_add ? NOMOUNT_CMD_ADD_RULE : NOMOUNT_CMD_DEL_RULE;

            for (int arg_idx = 2; arg_idx + step <= argc; arg_idx += step) {
                int v_len = resolve_path(v_resolved, cwd, argv[arg_idx], PATH_MAX);
                if (v_len == 0) { exit_code = 3; continue; }

                int r_len = 0;
                if (is_add) {
                    r_len = resolve_path(r_resolved, cwd, argv[arg_idx+1], PATH_MAX);
                    if (r_len == 0) { exit_code = 3; continue; }
                }

                int item_size = is_add ? (8 + v_len + r_len) : (2 + v_len);
                if ((cursor - payload) + item_size > max_payload) {
                    nlh = init_msg(nm_family, target_cmd, req_flags);
                    add_attr(nlh, NOMOUNT_ATTR_PAYLOAD, payload, cursor - payload);
                    long res = send_and_recv(fd, nlh);
                    if (res < 0) exit_code = -res;
                    cursor = payload;
                }

                u16 vp_len_u = (u16)v_len;
                if (is_add) {
                    u32 flags = 0;
                    u16 rp_len_u = (u16)r_len;
                    memcpy(cursor, &flags, 4); cursor += 4;
                    memcpy(cursor, &vp_len_u, 2); cursor += 2;
                    memcpy(cursor, &rp_len_u, 2); cursor += 2;
                    memcpy(cursor, v_resolved, v_len); cursor += v_len;
                    memcpy(cursor, r_resolved, r_len); cursor += r_len;
                } else {
                    memcpy(cursor, &vp_len_u, 2); cursor += 2;
                    memcpy(cursor, v_resolved, v_len); cursor += v_len;
                }
            }

            if (cursor > payload) {
                nlh = init_msg(nm_family, target_cmd, req_flags);
                add_attr(nlh, NOMOUNT_ATTR_PAYLOAD, payload, cursor - payload);
                long res = send_and_recv(fd, nlh);
                if (res < 0) exit_code = -res;
            }
            goto do_exit;
        }
        case 'b':
        case 'u': {
            if (argc < 3) goto do_exit;
            unsigned int uid = 0;
            const char *s = argv[2];
            while (*s) uid = uid * 10 + (*s++ - '0');

            nlh = init_msg(nm_family, (cmd == 'b') ? NOMOUNT_CMD_ADD_UID : NOMOUNT_CMD_DEL_UID, req_flags);
            add_attr(nlh, NOMOUNT_ATTR_UID, &uid, sizeof(uid));
            break;
        }
        case 'c':
            nlh = init_msg(nm_family, NOMOUNT_CMD_CLEAR_ALL, req_flags);
            break;
        case 'v':
            nlh = init_msg(nm_family, NOMOUNT_CMD_GET_VERSION, req_flags);
            break;
        case 'l': {
            /* The Dumpit Magic for 'ls' */
            nlh = init_msg(nm_family, NOMOUNT_CMD_GET_LIST, NLM_F_REQUEST | NLM_F_DUMP);
            
            struct sockaddr_nl dest = { .nl_family = AF_NETLINK };
            sys6(SYS_SENDTO, fd, (long)nlh, nlh->nlmsg_len, 0, (long)&dest, sizeof(dest));

            int is_json = (argc > 2 && argv[2][0] == 'j');
            if (is_json) printc("[\n");
            int first_item = 1;

            while (1) {
                long len = sys6(SYS_RECVFROM, fd, (long)rx_buf, sizeof(rx_buf), 0, 0, 0);
                if (len <= 0) break;

                int stop = 0;

               for (struct nlmsghdr *msg = (struct nlmsghdr *)rx_buf; NLMSG_OK(msg, len); msg = NLMSG_NEXT(msg, len)) {
                    if (msg->nlmsg_type == NLMSG_DONE || msg->nlmsg_type == NLMSG_ERROR) {
                        stop = 1;
                        break;
                    }

                    struct nlattr *tb[NOMOUNT_ATTR_PAYLOAD + 1];
                    struct nlattr *attrs = (struct nlattr *)((char *)msg + NLMSG_HDRLEN + NLMSG_ALIGN(sizeof(struct genlmsghdr)));
                    int attr_len = msg->nlmsg_len - NLMSG_HDRLEN - NLMSG_ALIGN(sizeof(struct genlmsghdr));
                    
                    parse_attrs(tb, NOMOUNT_ATTR_PAYLOAD, attrs, attr_len);
                    
                    if (tb[NOMOUNT_ATTR_VIRTUAL_PATH] && tb[NOMOUNT_ATTR_REAL_PATH]) {
                        char *v = (char *)tb[NOMOUNT_ATTR_VIRTUAL_PATH] + NLA_HDRLEN;
                        char *r = (char *)tb[NOMOUNT_ATTR_REAL_PATH] + NLA_HDRLEN;
                        
                        if (is_json) {
                            if (!first_item) printc(",\n");
                            first_item = 0;
                            printc("  {\n    \"virtual\": \"");
                            sys3(SYS_WRITE, 1, (long)v, strlen(v));
                            printc("\",\n    \"real\": \"");
                            sys3(SYS_WRITE, 1, (long)r, strlen(r));
                            printc("\"\n  }");
                        } else {
                            sys3(SYS_WRITE, 1, (long)v, strlen(v));
                            printc(" -> ");
                            sys3(SYS_WRITE, 1, (long)r, strlen(r));
                            printc("\n");
                        }
                    }
                    msg = (struct nlmsghdr *)((char *)msg + NLMSG_ALIGN(msg->nlmsg_len));
                }
                if (stop) break;
            }
            if (is_json) printc("\n]\n");
            
            exit_code = 0;
            goto do_exit;
        }
    }

    if (nlh) {
        long res = send_and_recv(fd, nlh);
        exit_code = (res < 0) ? -res : 0;

        if (cmd == 'v' && res > 0) {
            /* Handle Version Reply */
            struct nlmsghdr *rep = (struct nlmsghdr *)rx_buf;
            struct nlattr *tb[6];
            struct nlattr *attrs = (struct nlattr *)((char *)rep + NLMSG_HDRLEN + NLMSG_ALIGN(sizeof(struct genlmsghdr)));
            int attr_len = rep->nlmsg_len - NLMSG_HDRLEN - NLMSG_ALIGN(sizeof(struct genlmsghdr));
            
            parse_attrs(tb, 5, attrs, attr_len);
            if (tb[NOMOUNT_ATTR_VERSION]) {
                unsigned int ver = *(unsigned int *)((char *)tb[NOMOUNT_ATTR_VERSION] + NLA_HDRLEN);
                char v_str[2] = { ver + '0', '\n' };
                sys3(SYS_WRITE, 1, (long)v_str, 2);
            }
        }
    }

do_exit:
    if (fd >= 0) sys1(SYS_CLOSE, fd);
    sys1(SYS_EXIT, exit_code);
    __builtin_unreachable();
}
