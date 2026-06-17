/* --- ARCH --- */
#if defined(__aarch64__)
    #define SYS_GETCWD     17
    #define SYS_CLOSE      57
    #define SYS_WRITE      64
    #define SYS_EXIT       93
    #define SYS_SOCKET     198
    #define SYS_BIND       200
    #define SYS_SENDTO     206
    #define SYS_RECVFROM   207

    __attribute__((always_inline)) static inline long sys1(long n, long a) {
        register long x8 asm("x8") = n; register long x0 asm("x0") = a;
        __asm__ __volatile__("svc 0" : "+r"(x0) : "r"(x8) : "memory", "cc");
        return x0;
    }
    __attribute__((always_inline)) static inline long sys2(long n, long a, long b) {
        register long x8 asm("x8") = n; register long x0 asm("x0") = a; register long x1 asm("x1") = b;
        __asm__ __volatile__("svc 0" : "+r"(x0) : "r"(x8), "r"(x1) : "memory", "cc");
        return x0;
    }
    __attribute__((always_inline)) static inline long sys3(long n, long a, long b, long c) {
        register long x8 asm("x8") = n; register long x0 asm("x0") = a; register long x1 asm("x1") = b; register long x2 asm("x2") = c;
        __asm__ __volatile__("svc 0" : "+r"(x0) : "r"(x8), "r"(x1), "r"(x2) : "memory", "cc");
        return x0;
    }
    __attribute__((always_inline)) static inline long sys4(long n, long a, long b, long c, long d) {
        register long x8 asm("x8") = n; register long x0 asm("x0") = a; register long x1 asm("x1") = b; register long x2 asm("x2") = c; register long x3 asm("x3") = d;
        __asm__ __volatile__("svc 0" : "+r"(x0) : "r"(x8), "r"(x1), "r"(x2), "r"(x3) : "memory", "cc");
        return x0;
    }
    __attribute__((always_inline)) static inline long sys6(long n, long a, long b, long c, long d, long e, long f) {
        register long x8 asm("x8") = n; register long x0 asm("x0") = a; register long x1 asm("x1") = b; register long x2 asm("x2") = c; register long x3 asm("x3") = d; register long x4 asm("x4") = e; register long x5 asm("x5") = f;
        __asm__ __volatile__("svc 0" : "+r"(x0) : "r"(x8), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5) : "memory", "cc");
        return x0;
    }
    __attribute__((naked)) void _start(void) { __asm__ volatile("mov x0, sp\n bl c_main\n"); }

#elif defined(__arm__)
    #define SYS_EXIT       1
    #define SYS_WRITE      4
    #define SYS_CLOSE      6
    #define SYS_GETCWD     183
    #define SYS_SOCKET     281
    #define SYS_BIND       282
    #define SYS_SENDTO     290
    #define SYS_RECVFROM   292

    __attribute__((always_inline)) static inline long sys1(long n, long a) {
        register long r7 asm("r7") = n; register long r0 asm("r0") = a;
        __asm__ __volatile__("svc 0" : "+r"(r0) : "r"(r7) : "memory", "cc");
        return r0;
    }
    __attribute__((always_inline)) static inline long sys2(long n, long a, long b) {
        register long r7 asm("r7") = n; register long r0 asm("r0") = a; register long r1 asm("r1") = b;
        __asm__ __volatile__("svc 0" : "+r"(r0) : "r"(r7), "r"(r1) : "memory", "cc");
        return r0;
    }
    __attribute__((always_inline)) static inline long sys3(long n, long a, long b, long c) {
        register long r7 asm("r7") = n; register long r0 asm("r0") = a; register long r1 asm("r1") = b; register long r2 asm("r2") = c;
        __asm__ __volatile__("svc 0" : "+r"(r0) : "r"(r7), "r"(r1), "r"(r2) : "memory", "cc");
        return r0;
    }
    __attribute__((always_inline)) static inline long sys4(long n, long a, long b, long c, long d) {
        register long r7 asm("r7") = n; register long r0 asm("r0") = a; register long r1 asm("r1") = b; register long r2 asm("r2") = c; register long r3 asm("r3") = d;
        __asm__ __volatile__("svc 0" : "+r"(r0) : "r"(r7), "r"(r1), "r"(r2), "r"(r3) : "memory", "cc");
        return r0;
    }
    __attribute__((always_inline)) static inline long sys6(long n, long a, long b, long c, long d, long e, long f) {
        register long r7 asm("r7") = n; register long r0 asm("r0") = a; register long r1 asm("r1") = b; register long r2 asm("r2") = c; register long r3 asm("r3") = d; register long r4 asm("r4") = e; register long r5 asm("r5") = f;
        __asm__ __volatile__("svc 0" : "+r"(r0) : "r"(r7), "r"(r1), "r"(r2), "r"(r3), "r"(r4), "r"(r5) : "memory", "cc");
        return r0;
    }
    __attribute__((naked)) void _start(void) { __asm__ volatile("mov r0, sp\n bl c_main\n"); }
#else
    #error "Arch not supported"
#endif

/* Internal macro to dispatch based on argument count */
#define __GET_SYSCALL_MACRO(_1, _2, _3, _4, _5, _6, _7, NAME, ...) NAME

/* * Unified syscall wrapper.
 * It automatically expands to the correct sysX macro depending on how many
 * arguments you provide (up to 6 arguments + the syscall number).
 */
#define syscall(...) __GET_SYSCALL_MACRO(__VA_ARGS__, sys6, sys5, sys4, sys3, sys2, sys1, sys0)(__VA_ARGS__)

/* --- NETLINK DEFS --- */
#define AF_NETLINK 16
#define SOCK_RAW 3
#define NETLINK_GENERIC 16
#define ALIGN4(len) (((len) + 3) & ~3)

#define NLMSG_OK(nlh, len) ((len) >= 16 && (nlh)->nlmsg_len >= 16 && (nlh)->nlmsg_len <= (len))
#define NLMSG_NEXT(nlh, len) ((len) -= ALIGN4((nlh)->nlmsg_len), (struct nlmsghdr *)(((char *)(nlh)) + ALIGN4((nlh)->nlmsg_len)))

struct nlmsghdr {
    unsigned int   nlmsg_len;
    unsigned short nlmsg_type;
    unsigned short nlmsg_flags;
    unsigned int   nlmsg_seq;
    unsigned int   nlmsg_pid;
};

struct nlmsgerr {
    int error;
    struct nlmsghdr msg;
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
static int nl_seq = 0;
#define PATH_MAX  4096
#define RX_BUF_SIZE (PATH_MAX + 4)
#define MAX_PAYLOAD (RX_BUF_SIZE - 88)

static char sys_mem[(RX_BUF_SIZE * 2) + (PATH_MAX * 3) + MAX_PAYLOAD];
#define tx_buf     (&sys_mem[0])
#define rx_buf     (&sys_mem[RX_BUF_SIZE])
#define v_resolved (&sys_mem[RX_BUF_SIZE * 2])
#define r_resolved (&sys_mem[(RX_BUF_SIZE * 2) + PATH_MAX])
#define cwd_buf    (&sys_mem[(RX_BUF_SIZE * 2) + (PATH_MAX * 2)])
#define payload    (&sys_mem[(RX_BUF_SIZE * 2) + (PATH_MAX * 3)])

__attribute__((noinline))
static inline void *memcpy(void *dst, const void *src, unsigned long n) {
    char *d = dst; const char *s = src;
    while (n--) *d++ = *s++;
    return dst;
}

__attribute__((noinline))
static void print_str(const char *s) {
    long len = 0;
    while (s[len]) len++;
    syscall(SYS_WRITE, 1, (long)s, len);
}

/* complete path resolution */
__attribute__((noinline))
static char* resolve_path(char *p, const char *cwd, const char *rel) {
    if (*rel != '/' && cwd) {
        while (*cwd) *p++ = *cwd++;
        if (p[-1] != '/') *p++ = '/';
    }
    while ((*p++ = *rel++));
    return p - 1; /* Points exactly to '\0' */
}

__attribute__((noinline))
static void *get_attr(struct nlmsghdr *msg, int type) {
    int rem = msg->nlmsg_len - 20; /* 16 (nlmsghdr) + 4 (genlmsghdr) */
    struct nlattr *attr = (void *)((char *)msg + 20);
    while (rem >= 4) {
        if (attr->nla_type == type) return (char *)attr + 4;
        int alen = ALIGN4(attr->nla_len);
        if (!alen || alen > rem) break;
        attr = (void *)((char *)attr + alen);
        rem -= alen;
    }
    return (void *)0;
}

/* init_msg + add_attr + send_and_recv unified */
__attribute__((noinline))
static int do_nm_cmd(int fd, int fam, int cmd, int atype, const void *data, int len, int flags) {
    int *p = (int *)tx_buf; p[0] = 0; p[1] = 0; p[2] = 0;
    struct nlmsghdr *nlh = (void *)tx_buf;
    struct genlmsghdr *gnlh = (void *)(tx_buf + 16);

    nlh->nlmsg_type = fam;
    nlh->nlmsg_flags = flags;
    nlh->nlmsg_seq = 1; /* Sequence is irrelevant for synchronous solitary calls */
    gnlh->cmd = cmd;
    gnlh->version = 1;
    nlh->nlmsg_len = 20;

    if (data) {
        struct nlattr *nla = (void *)(tx_buf + 20);
        nla->nla_type = atype;
        nla->nla_len = 4 + len;
        memcpy(nla + 1, data, len);
        nlh->nlmsg_len = 20 + ALIGN4(nla->nla_len);
    }

    if (syscall(SYS_SENDTO, fd, (long)nlh, nlh->nlmsg_len, 0, 0, 0) < 0) return -1;
    int res = syscall(SYS_RECVFROM, fd, (long)rx_buf, RX_BUF_SIZE, 0, 0, 0);
    if (res < 0) return -1;
    return (((struct nlmsghdr *)rx_buf)->nlmsg_type == 2) ? ((struct nlmsgerr *)(rx_buf + 16))->error : res;
}
