/*
 * nm.c - NoMount CLI Userspace Tool 
 */

/* --- ARCH --- */
#if defined(__aarch64__)
    #define SYS_GETCWD     17
    #define SYS_IOCTL      29
    #define SYS_OPENAT     56
    #define SYS_CLOSE      57
    #define SYS_WRITE      64
    #define SYS_FSTATAT    79 
    #define SYS_EXIT       93

    __attribute__((always_inline))
    static inline long sys4(long n, long a, long b, long c, long d) {
        register long x8 asm("x8") = n;
        register long x0 asm("x0") = a;
        register long x1 asm("x1") = b;
        register long x2 asm("x2") = c;
        register long x3 asm("x3") = d;
        __asm__ __volatile__("svc 0" : "+r"(x0) : "r"(x8), "r"(x1), "r"(x2), "r"(x3) : "memory", "cc");
        return x0;
    }
    #define sys1(n,a) sys4(n,a,0,0,0)
    #define sys2(n,a,b) sys4(n,a,b,0,0)
    #define sys3(n,a,b,c) sys4(n,a,b,c,0)
    __attribute__((naked)) void _start(void) { __asm__ volatile("mov x0, sp\n bl c_main\n"); }

#elif defined(__arm__)
    #define SYS_EXIT       1
    #define SYS_WRITE      4
    #define SYS_CLOSE      6
    #define SYS_IOCTL      54
    #define SYS_GETCWD     183
    #define SYS_OPENAT     322
    #define SYS_FSTATAT    327

    __attribute__((always_inline))
    static inline long sys4(long n, long a, long b, long c, long d) {
        register long r7 asm("r7") = n;
        register long r0 asm("r0") = a;
        register long r1 asm("r1") = b;
        register long r2 asm("r2") = c;
        register long r3 asm("r3") = d;
        __asm__ __volatile__("svc 0" : "+r"(r0) : "r"(r7), "r"(r1), "r"(r2), "r"(r3) : "memory", "cc");
        return r0;
    }
    #define sys1(n,a) sys4(n,a,0,0,0)
    #define sys2(n,a,b) sys4(n,a,b,0,0)
    #define sys3(n,a,b,c) sys4(n,a,b,c,0)
    __attribute__((naked)) void _start(void) { __asm__ volatile("mov r0, sp\n bl c_main\n"); }
#else
    #error "Arch not supported"
#endif

/* --- DEFS --- */
typedef unsigned long size_t;
#define AT_FDCWD -100
#define AT_SYMLINK_NOFOLLOW 0x100
#define O_RDWR 2
#define STAT_MODE_IDX 4

struct ioctl_data {
    unsigned long long vp;
    unsigned long long rp;
    unsigned int flags;
    unsigned int _pad;
    unsigned long long real_ino;
};

#define IOCTL_ADD     0x40204E01
#define IOCTL_DEL     0x40204E02
#define IOCTL_CLEAR   0x4E03
#define IOCTL_VER     0x80044E04
#define IOCTL_ADD_UID 0x40044E05
#define IOCTL_DEL_UID 0x40044E06
#define IOCTL_LIST    0x80044E07

#define NM_DIR    128
#define PATH_MAX  4096

/* complete path resolution */
__attribute__((noinline))
static int resolve_path(char *result, const char *cwd, const char *rel_path, int max_len) {
    int r_pos = 0;
    int c_len = 0;

    if (rel_path[0] == '/') {
        while (rel_path[r_pos] && r_pos < max_len - 1) {
            result[r_pos] = rel_path[r_pos];
            r_pos++;
        }
    } else {
        if (cwd) {
            while (cwd[c_len] && r_pos < max_len - 1) {
                result[r_pos++] = cwd[c_len++];
            }
            if (r_pos > 0 && result[r_pos-1] != '/' && r_pos < max_len - 1) {
                result[r_pos++] = '/';
            }
        }
        int p_pos = 0;
        while (rel_path[p_pos] && r_pos < max_len - 1) {
            result[r_pos++] = rel_path[p_pos++];
        }
    }
    result[r_pos] = '\0';
    return r_pos;
}

#define printc(str) sys3(SYS_WRITE, 1, (long)str, sizeof(str) - 1)

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

    int fd = sys4(SYS_OPENAT, AT_FDCWD, (long)"/dev/nomount", O_RDWR, 0);
    if (fd < 0) {
        exit_code = (long)(-fd);
        goto do_exit;
    }

    char cmd = argv[1][0];
    struct ioctl_data data = {0};
    void *ioctl_arg = 0;
    unsigned int uid = 0;
    long ioctl_code = 0;
    int json = 0;

    switch (cmd) {
        case 'a':
        case 'd':
        case 'r': {
            int is_add = (cmd == 'a');
            int step = is_add ? 2 : 1;
            if (argc < 2 + step) goto do_exit;

            char *stack_buf = (char *)sp - 65536;
            char *cwd_buf = stack_buf;
            char *v_resolved = cwd_buf + PATH_MAX;
            char *r_resolved = v_resolved + PATH_MAX;

            long cwd_len = sys2(SYS_GETCWD, (long)cwd_buf, PATH_MAX);
            const char *cwd = (cwd_len > 0) ? cwd_buf : "/";
            unsigned long long st_buf[32];

            exit_code = 0; 

            for (int arg_idx = 2; arg_idx + step <= argc; arg_idx += step) {
                int v_len = resolve_path(v_resolved, cwd, argv[arg_idx], PATH_MAX);
                if (v_len == 0) { exit_code = 3; continue; }

                data.vp = (unsigned long)v_resolved;
                ioctl_arg = &data;

                if (!is_add) {
                    ioctl_code = IOCTL_DEL;
                    long res = sys3(SYS_IOCTL, fd, ioctl_code, (long)ioctl_arg);
                    if (res < 0) exit_code = -res;
                } else { 
                    int r_len = resolve_path(r_resolved, cwd, argv[arg_idx+1], PATH_MAX);
                    if (r_len == 0) { exit_code = 3; continue; }

                    data.rp = (unsigned long)r_resolved;
                    int slashes = 0;
                    for (int i = 0; i < v_len; i++) {
                        if (v_resolved[i] == '/') {
                            slashes++;
                            if (slashes < 2 || i == 0) continue;
                            
                            v_resolved[i] = '\0';
                            int r_cut = r_len - (v_len - i);
                            char r_saved = 0;

                            if (r_cut > 0 && r_cut < r_len) {
                                r_saved = r_resolved[r_cut];
                                r_resolved[r_cut] = '\0';
                            }

                            if (sys4(SYS_FSTATAT, AT_FDCWD, (long)v_resolved, (long)st_buf, AT_SYMLINK_NOFOLLOW) != 0) {
                                struct ioctl_data step_data = {0};
                                char *rp_to_send = (r_cut > 0 && r_cut < r_len) ? r_resolved : "/";
                                step_data.vp = (unsigned long)v_resolved;
                                step_data.rp = (unsigned long)rp_to_send;
                                step_data.flags = NM_DIR;

                                if (sys4(SYS_FSTATAT, AT_FDCWD, (long)rp_to_send, (long)st_buf, AT_SYMLINK_NOFOLLOW) == 0) {
                                    #if defined(__aarch64__)
                                        step_data.real_ino = ((unsigned long long *)st_buf)[1];
                                    #else
                                        step_data.real_ino = ((unsigned int*)st_buf)[3];
                                    #endif
                                }
                                sys3(SYS_IOCTL, fd, IOCTL_ADD, (long)&step_data);
                            }

                            v_resolved[i] = '/';
                            if (r_cut > 0 && r_cut < r_len) r_resolved[r_cut] = r_saved;
                        }
                    }

                    data.flags = 0;
                    data.real_ino = 0;
                    if (sys4(SYS_FSTATAT, AT_FDCWD, (long)r_resolved, (long)st_buf, AT_SYMLINK_NOFOLLOW) == 0) {
                        unsigned int mode = ((unsigned int *)st_buf)[STAT_MODE_IDX];
                        if ((mode & 0170000) == 0040000) data.flags |= NM_DIR;
                        #if defined(__aarch64__)
                            data.real_ino = ((unsigned long long *)st_buf)[1];
                        #else
                            data.real_ino = ((unsigned int *)st_buf)[3];
                        #endif
                    }

                    ioctl_code = IOCTL_ADD;
                    long res = sys3(SYS_IOCTL, fd, ioctl_code, (long)ioctl_arg);
                    if (res < 0) exit_code = -res;
                }
            }
            ioctl_code = 0; 
            break;
        }
        case 'b':
        case 'u': {
            if (argc < 3) goto do_exit;
            const char *s = argv[2];
            while (*s) uid = uid * 10 + (*s++ - '0');
            ioctl_arg = &uid;
            ioctl_code = (cmd == 'b') ? IOCTL_ADD_UID : IOCTL_DEL_UID;
            break;
        }
        case 'c':
            ioctl_code = IOCTL_CLEAR;
            break;
        case 'v':
            ioctl_code = IOCTL_VER;
            break;
        case 'l':
            ioctl_code = IOCTL_LIST;
            ioctl_arg = (void *)((char *)sp - 1048576); 
            break;
        default:
            goto do_exit;
    }

    if (ioctl_code) {
        long res = sys3(SYS_IOCTL, fd, ioctl_code, (long)ioctl_arg);
        
        if (cmd == 'v' && res > 0) {
            char v_buf[2] = {res + '0', '\n'};
            sys3(SYS_WRITE, 1, (long)v_buf, 2);
        }
        else if (cmd == 'l' && res > 0) {
            char *curr = (char *)ioctl_arg;
            char *end = curr + res;
            if (argc > 2 && argv[2][0] == 'j') {
                sys3(SYS_WRITE, 1, (long)"[\n", 2);

                int is_first = 1;
                while (curr < end) {
                    unsigned short total_len = *(unsigned short *)curr;
                    unsigned short v_len = *(unsigned short *)(curr + 2);
                    
                    if (total_len == 0) break;

                    char *v_path = curr + 4;
                    char *r_path = curr + 4 + v_len;
                    int r_len = total_len - 4 - v_len;

                    if (!is_first) sys3(SYS_WRITE, 1, (long)",\n", 2);
                    is_first = 0;

                    sys3(SYS_WRITE, 1, (long)"  {\n    \"virtual\": \"", 19);
                    sys3(SYS_WRITE, 1, (long)v_path, v_len - 1);
                    sys3(SYS_WRITE, 1, (long)"\",\n    \"real\": \"", 16);
                    if (r_len > 1) {
                        sys3(SYS_WRITE, 1, (long)r_path, r_len - 1);
                    }
                    sys3(SYS_WRITE, 1, (long)"\"\n  }", 5);

                    curr += total_len;
                }
                sys3(SYS_WRITE, 1, (long)"\n]\n", 3);
            } else {
                while (curr < end) {
                    unsigned short total_len = *(unsigned short *)curr;
                    unsigned short v_len = *(unsigned short *)(curr + 2);
                    
                    if (total_len == 0) break;

                    char *v_path = curr + 4;
                    char *r_path = curr + 4 + v_len;
                    int r_len = total_len - 4 - v_len;

                    sys3(SYS_WRITE, 1, (long)v_path, v_len - 1);
                    if (r_len > 1) {
                        sys3(SYS_WRITE, 1, (long)" -> ", 4);
                        sys3(SYS_WRITE, 1, (long)r_path, r_len - 1);
                    }
                    sys3(SYS_WRITE, 1, (long)"\n", 1);

                    curr += total_len;
                }
            }
        }
        
        exit_code = (res < 0) ? -res : 0;
    }

do_exit:
    sys1(SYS_EXIT, exit_code);
    __builtin_unreachable();
}
