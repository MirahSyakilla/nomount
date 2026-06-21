/* --- ARCH --- */
#if defined(__aarch64__)
    #define SYS_GETCWD     17
    #define SYS_READ       63
    #define SYS_WRITE      64
    #define SYS_EXIT       93
    #define SYS_SOCKET     198

    __attribute__((always_inline)) static inline long sys1(long n, long a) {
        register long x8 asm("x8") = n; register long x0 asm("x0") = a;
        __asm__ __volatile__("svc 0" : "+r"(x0) : "r"(x8) : "memory", "cc");
        return x0;
    }
    __attribute__((always_inline)) static inline long sys3(long n, long a, long b, long c) {
        register long x8 asm("x8") = n; register long x0 asm("x0") = a; register long x1 asm("x1") = b; register long x2 asm("x2") = c;
        __asm__ __volatile__("svc 0" : "+r"(x0) : "r"(x8), "r"(x1), "r"(x2) : "memory", "cc");
        return x0;
    }
    __asm__( ".global _start\n" ".type _start, %function\n" "_start:\n" "    mov x0, sp\n" "    bl c_main\n" );

#elif defined(__arm__)
    #define SYS_EXIT       1
    #define SYS_READ       3
    #define SYS_WRITE      4
    #define SYS_GETCWD     183
    #define SYS_SOCKET     281

    __attribute__((always_inline)) static inline long sys1(long n, long a) {
        register long r7 asm("r7") = n; register long r0 asm("r0") = a;
        __asm__ __volatile__("svc 0" : "+r"(r0) : "r"(r7) : "memory", "cc");
        return r0;
    }
    __attribute__((always_inline)) static inline long sys3(long n, long a, long b, long c) {
        register long r7 asm("r7") = n; register long r0 asm("r0") = a; register long r1 asm("r1") = b; register long r2 asm("r2") = c;
        __asm__ __volatile__("svc 0" : "+r"(r0) : "r"(r7), "r"(r1), "r"(r2) : "memory", "cc");
        return r0;
    }
    __asm__( ".global _start\n" ".type _start, %function\n" "_start:\n" "    mov r0, sp\n" "    bl c_main\n");
#else
    #error "Arch not supported"
#endif

/* --- NETLINK DEFS --- */
#define AF_NETLINK 16
#define SOCK_RAW 3
#define NETLINK_GENERIC 16
#define ALIGN4(len) (((len) + 3) & -4)

#define NLMSG_OK(nlh, len) ((nlh)->nlmsg_len && (nlh)->nlmsg_len <= (len))
#define NLMSG_NEXT(nlh, len) ((len) -= ALIGN4((nlh)->nlmsg_len), (struct nlmsghdr *)(((char *)(nlh)) + ALIGN4((nlh)->nlmsg_len)))

struct nlmsghdr {
    unsigned int   nlmsg_len;
    unsigned short nlmsg_type;
    unsigned short nlmsg_flags;
    unsigned int   nlmsg_seq;
    unsigned int   nlmsg_pid;
};

struct genlmsghdr {
    unsigned char cmd;
    unsigned char version;
    unsigned short reserved;
};

struct nlattr {
    unsigned short nla_len;
    unsigned short nla_type;
};

/* --- NETLINK ENGINE --- */
#define PATH_MAX  4096
#define RX_BUF_SIZE (PATH_MAX + 8)
#define MAX_PAYLOAD (RX_BUF_SIZE - 88)

static char sys_mem[(RX_BUF_SIZE * 2) + (PATH_MAX * 3) + MAX_PAYLOAD] __attribute__((aligned(8)));
#define tx_buf     (&sys_mem[0])
#define rx_buf     (&sys_mem[RX_BUF_SIZE])
#define v_resolved (&sys_mem[RX_BUF_SIZE * 2])
#define r_resolved (&sys_mem[(RX_BUF_SIZE * 2) + PATH_MAX])
#define cwd_buf    (&sys_mem[(RX_BUF_SIZE * 2) + (PATH_MAX * 2)])
#define payload    (&sys_mem[(RX_BUF_SIZE * 2) + (PATH_MAX * 3)])

__attribute__((noinline))
static void *memcpy(void *dst, const void *src, unsigned int n) {
    char *d = dst; const char *s = src;
    while (n--) *d++ = *s++;
    return dst;
}

__attribute__((noinline))
static void print_str(const char *s) {
    long len = 0;
    while (s[len]) len++;
    sys3(SYS_WRITE, 1, (long)s, len);
}

/* complete path resolution */
__attribute__((noinline))
static char* resolve_path(char *p, const char *cwd, const char *rel) {
    if (cwd && *rel != '/') {
        while (*cwd) *p++ = *cwd++;
        /* Linux VFS treats "//" exactly as "/" */
        *p++ = '/'; 
    }

    while ((*p++ = *rel++));
    return p - 1; /* Points exactly to '\0' */
}

__attribute__((noinline))
static void *get_attr(const void *nh, int type) {
    struct nlattr *attr = (void *)((char *)nh + 20);
    while (attr->nla_len) {
        if (attr->nla_type == type) return (char *)attr + 4;
        attr = (void *)((char *)attr + ALIGN4(attr->nla_len));
    }
    return (void *)0;
}

/* init_msg + add_attr + send_and_recv unified */
__attribute__((noinline))
static int do_nm_cmd(int fd, int fam, int cmd, int atype, const void *data, int len, int flags) {
    struct nlmsghdr *nlh = (void *)tx_buf;
    struct genlmsghdr *gnlh = (void *)(tx_buf + 16);

    nlh->nlmsg_type = fam;
    nlh->nlmsg_flags = flags;
    nlh->nlmsg_len = 20;
    gnlh->cmd = cmd;
    gnlh->version = 1;

    if (data) {
        struct nlattr *nla = (void *)(tx_buf + 20);
        nla->nla_type = atype;
        nla->nla_len = 4 + len;
        memcpy(nla + 1, data, len);
        nlh->nlmsg_len = 20 + ALIGN4(nla->nla_len);
    }

    int res = sys3(SYS_WRITE, fd, (long)nlh, nlh->nlmsg_len);
    if (res < 0) return res;
    res = sys3(SYS_READ, fd, (long)rx_buf, RX_BUF_SIZE);
    if (res >= 0 && ((struct nlmsghdr *)rx_buf)->nlmsg_type == 2) res = *(int *)(rx_buf + 16);

    return res;
}
