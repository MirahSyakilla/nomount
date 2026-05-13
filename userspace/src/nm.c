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
#define O_RDWR 2

struct ioctl_data {
    unsigned long long vp;
    unsigned long long rp;
    unsigned int flags;
};

#define IOCTL_ADD     0x40184E01
#define IOCTL_DEL     0x40184E02
#define IOCTL_CLEAR   0x00004E03
#define IOCTL_VER     0x80044E04
#define IOCTL_ADD_UID 0x40044E05
#define IOCTL_DEL_UID 0x40044E06
#define IOCTL_LIST    0x80044E07

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

static char v_resolved[PATH_MAX];
static char r_resolved[PATH_MAX];
static char cwd_buf[PATH_MAX];
static char list_buf[33554432];

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

    switch (cmd) {
        case 'a':
        case 'd':
        case 'r': {
            int is_add = (cmd == 'a');
            int step = is_add ? 2 : 1;
            if (argc < 2 + step) goto do_exit;

            long cwd_len = sys2(SYS_GETCWD, (long)cwd_buf, PATH_MAX);
            const char *cwd = (cwd_len > 0) ? cwd_buf : "/";

            exit_code = 0; 

            for (int arg_idx = 2; arg_idx + step <= argc; arg_idx += step) {
                int v_len = resolve_path(v_resolved, cwd, argv[arg_idx], PATH_MAX);
                if (v_len == 0) { exit_code = 3; continue; }

                data.vp = (unsigned long long)(unsigned long)v_resolved;
                ioctl_arg = &data;

                if (!is_add) {
                    ioctl_code = IOCTL_DEL;
                    long res = sys3(SYS_IOCTL, fd, ioctl_code, (long)ioctl_arg);
                    if (res < 0) exit_code = -res;
                } else { 
                    int r_len = resolve_path(r_resolved, cwd, argv[arg_idx+1], PATH_MAX);
                    if (r_len == 0) { exit_code = 3; continue; }

                    data.rp = (unsigned long long)(unsigned long)r_resolved;
                    data.flags = 0;

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
            ioctl_arg = list_buf;
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
                char *json_out_buf = end;
                int json_out_pos = 0;

                #define append_const(str) do { \
                    int _l = sizeof(str) - 1; \
                    for (int _i = 0; _i < _l; _i++) json_out_buf[json_out_pos++] = (str)[_i]; \
                } while(0)

                #define append_str(s, l) do { \
                    for (int _i = 0; _i < (l); _i++) json_out_buf[json_out_pos++] = (s)[_i]; \
                } while(0)

                append_const("[\n");

                int is_first = 1;
                while (curr < end) {
                    unsigned short total_len = *(unsigned short *)curr;
                    unsigned short v_len = *(unsigned short *)(curr + 2);
                    
                    if (total_len == 0) break;

                    char *v_path = curr + 4;
                    char *r_path = curr + 4 + v_len;
                    int r_len = total_len - 4 - v_len;

                    if (!is_first) append_const(",\n");
                    is_first = 0;

                    append_const("  {\n    \"virtual\": \"");
                    append_str(v_path, v_len - 1);
                    
                    append_const("\",\n    \"real\": \"");
                    if (r_len > 1) {
                        append_str(r_path, r_len - 1);
                    }
                    append_const("\"\n  }");

                    curr += total_len;
                }
                append_const("\n]\n");

                sys3(SYS_WRITE, 1, (long)json_out_buf, json_out_pos);

                #undef append_const
                #undef append_str
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
