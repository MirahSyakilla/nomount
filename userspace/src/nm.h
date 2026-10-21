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
        register long x8 asm("x8") = n; register long x0 asm("x0") = a;
        register long x1 asm("x1") = b; register long x2 asm("x2") = c;
        __asm__ __volatile__("svc 0" : "+r"(x0) : "r"(x8), "r"(x1), "r"(x2) : "memory", "cc");
        return x0;
    }
    __asm__( ".global _start\n" ".type _start, %function\n" "_start:\n" "mov x0, sp\n" "b c_main\n" );

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
        register long r7 asm("r7") = n; register long r0 asm("r0") = a;
        register long r1 asm("r1") = b; register long r2 asm("r2") = c;
        __asm__ __volatile__("svc 0" : "+r"(r0) : "r"(r7), "r"(r1), "r"(r2) : "memory", "cc");
        return r0;
    }
    __asm__( ".global _start\n" ".type _start, %function\n" "_start:\n" "mov r0, sp\n" "b c_main\n");

#elif defined(__x86_64__)
    #define SYS_READ       0
    #define SYS_WRITE      1
    #define SYS_SOCKET     41
    #define SYS_EXIT       60
    #define SYS_GETCWD     79

    __attribute__((always_inline)) static inline long sys1(long n, long a) {
        long ret; __asm__ __volatile__("syscall" : "=a"(ret) : "a"(n), "D"(a) : "rcx", "r11", "memory", "cc");
        return ret;
    }
    __attribute__((always_inline)) static inline long sys3(long n, long a, long b, long c) {
        long ret; __asm__ __volatile__("syscall" : "=a"(ret) : "a"(n), "D"(a), "S"(b), "d"(c) : "rcx", "r11", "memory", "cc");
        return ret;
    }
    /* The SysV ABI expects rsp % 16 == 8 on entry to a function; a bare jump
     * from _start would leave it 16-byte aligned instead, which breaks the
     * compiler's aligned stack slots. */
    __asm__( ".global _start\n" ".type _start, @function\n" "_start:\n" "mov %rsp, %rdi\n" "and $-16, %rsp\n" "call c_main\n" );

#else
    #error "Arch not supported"
#endif

/* --- DEFS --- */
#define PATH_MAX        4096
#define NM_EINTR        4
#define NM_EAGAIN       11
#define NM_EINVAL       22
#define NM_ENOSYS       38
#define NM_EXIT_OK      0
#define NM_EXIT_USAGE   1
#define NM_EXIT_IO      2
#define NM_EXIT_KERNEL  3
#define NM_EXIT_UNSUP   4

/* --- GENERIC NETLINK PLUMBING --- */
#define AF_NETLINK        16
#define SOCK_RAW          3
#define NETLINK_GENERIC   16

#define NLMSG_HDRLEN      16
#define GENL_HDRLEN       4
#define NLMSG_ERROR       2
#define NLMSG_DONE        3

#define NLM_F_REQUEST     0x001
#define NLM_F_MULTI       0x002
#define NLM_F_ACK         0x004
#define NLM_F_DUMP        0x300

#define CTRL_CMD_GETFAMILY  3
#define CTRL_ATTR_FAMILY_ID   1
#define CTRL_ATTR_FAMILY_NAME 2

/* --- KERNEL-SIDE CONTRACT (KernelSU-Next kernel/nomount/nomount.h) --- */
#define NM_GENL_FAMILY          "nomount"
#define NM_GENL_FAMILY_VERSION   1

enum {
    NM_CMD_UNSPEC = 0,
    NM_CMD_GET_VERSION = 1,
    NM_CMD_ADD_RULE = 2,
    NM_CMD_DEL_RULE = 3,
    NM_CMD_CLEAR_ALL = 4,
    NM_CMD_ADD_UID = 5,
    NM_CMD_DEL_UID = 6,
    NM_CMD_GET_LIST = 7,
};

enum {
    NM_ATTR_UNSPEC = 0,
    NM_ATTR_VIRTUAL_PATH = 1,
    NM_ATTR_REAL_PATH = 2,
    NM_ATTR_FLAGS = 3,
    NM_ATTR_UID = 4,
    NM_ATTR_VERSION = 5,
    NM_ATTR_PAYLOAD = 6,
};

#define NM_FLAG_IS_DIR   (1U << 1)
#define NM_FLAG_WHITEOUT (1U << 2)

/* Packed payload records, see kernel/nomount/netlink.c.
 *   ADD_RULE: u32 flags | u16 v_len | u16 r_len | vpath | rpath
 *   DEL_RULE: u16 v_len | vpath
 * Paths are length prefixed and never NUL terminated. */
#define NM_RULE_HDR_SIZE  8
#define NM_DEL_HDR_SIZE   2

/* --- CLI MODEL --- */
enum nm_cli_action {
    ACTION_NONE = 0,
    ACTION_RULE_ADD,
    ACTION_RULE_DEL,
    ACTION_RULE_LIST,
    ACTION_UID_ADD,
    ACTION_UID_DEL,
    ACTION_CLEAR_ALL,
    ACTION_VERSION,
    /* Present in upstream but not implemented by the hookless kernel. */
    ACTION_RULE_CLEAR,
    ACTION_UID_LIST,
    ACTION_UID_CLEAR,
    ACTION_BLOCK_ISOLATED,
};

struct nm_args {
    enum nm_cli_action action;
    int data_start_idx;
    int is_json;
    int is_whiteout;
    int has_uid;
    unsigned int target_uid;
};

/* --- UTILS --- */
#define noinline __attribute__((noinline))

static noinline int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

static noinline unsigned long strlen(const char *s) {
    unsigned long len = 0;
    while (s[len]) len++;
    return len;
}

#if defined(__x86_64__)
static noinline void *memcpy(void *dst, const void *src, unsigned long n) {
    void *ret = dst;
    __asm__ __volatile__("rep movsb" : "+D"(dst), "+S"(src), "+c"(n) : : "memory");
    return ret;
}
#else
static noinline void *memcpy(void *dst, const void *src, unsigned long n) {
    char *d = dst;
    const char *s = src;
    while (n--) { *d++ = *s++; }
    return dst;
}
#endif

static noinline void print_strn(const char *s, unsigned long len) {
    if (len > 0) sys3(SYS_WRITE, 1, (long)s, len);
}

#define print_literal(s) print_strn((s), sizeof(s) - 1)

/* --- OUTPUT BUFFER --- */
#define NM_OUT_BUF_SIZE   16384
#define NM_RX_BUF_SIZE    32768
#define NM_TX_BUF_SIZE    16384
/* Keep a single netlink datagram comfortably below SKB_MAX_ALLOC so the
 * kernel never has to fall back to a fragmented allocation. */
#define NM_MAX_PAYLOAD    8192

static char sys_mem[NM_RX_BUF_SIZE + NM_TX_BUF_SIZE + PATH_MAX + NM_MAX_PAYLOAD + NM_OUT_BUF_SIZE]
    __attribute__((aligned(16)));
#define rx_buf     (&sys_mem[0])
#define tx_buf     (&sys_mem[NM_RX_BUF_SIZE])
#define cwd_buf    (&sys_mem[NM_RX_BUF_SIZE + NM_TX_BUF_SIZE])
#define payload    (&sys_mem[NM_RX_BUF_SIZE + NM_TX_BUF_SIZE + PATH_MAX])
#define out_buf    (&sys_mem[NM_RX_BUF_SIZE + NM_TX_BUF_SIZE + PATH_MAX + NM_MAX_PAYLOAD])

struct nm_output {
    char *buffer;
    unsigned int used;
    unsigned int size;
};

static noinline void list_flush(struct nm_output *out) {
    unsigned int sent = 0;
    while (sent < out->used) {
        long n = sys3(SYS_WRITE, 1, (long)(out->buffer + sent), out->used - sent);
        if (n == -NM_EINTR) continue;
        if (n <= 0) break;
        sent += n;
    }
    out->used = 0;
}

static noinline void list_print_strn(struct nm_output *out, const char *s, unsigned long len) {
    while (len) {
        if (out->used == out->size) list_flush(out);
        unsigned long n = out->size - out->used;
        if (n > len) n = len;
        for (unsigned long i = 0; i < n; i++) out->buffer[out->used++] = *s++;
        len -= n;
    }
}

static noinline void list_print_json_strn(struct nm_output *out, const char *s, unsigned long len) {
    const char *start = s;
    while (len--) {
        unsigned char c = *s++;
        if (c >= 0x20 && c != '"' && c != '\\') continue;
        list_print_strn(out, start, s - start - 1);
        char escaped[6] = {'\\', 'u', '0', '0', '0' + (c >> 4), "0123456789abcdef"[c & 15]};
        if (c >= 0x20) escaped[1] = c;
        list_print_strn(out, escaped, c < 0x20 ? sizeof(escaped) : 2);
        start = s;
    }
    list_print_strn(out, start, s - start);
}

#define list_print_literal(out, s) list_print_strn((out), (s), sizeof(s) - 1)
#define list_print_literal_offset(out, s, offset) list_print_strn((out), (s) + (offset), sizeof(s) - 1 - (offset))

static noinline void list_print_uint(struct nm_output *out, unsigned int n) {
    char buf[10];
    int i = sizeof(buf);
    do {
        buf[--i] = (n % 10) + '0';
        n /= 10;
    } while (n > 0);
    list_print_strn(out, &buf[i], sizeof(buf) - i);
}

/* --- PATH RESOLUTION --- */
static noinline int resolved_path_length(const char *cwd, const char *rel) {
    int length = 0;
    if (cwd && *rel != '/') {
        while (*cwd++) {
            if (length == PATH_MAX) return -1;
            length++;
        }
        if (length == PATH_MAX) return -1;
        length++;
    }
    while (*rel++) {
        if (length == PATH_MAX) return -1;
        length++;
    }
    return length;
}

static noinline char* resolve_path(char *p, const char *cwd, const char *rel) {
    if (cwd && *rel != '/') {
        while (*cwd) {
            *p++ = *cwd++;
        }
        *p++ = '/';
    }
    while (*rel) {
        *p++ = *rel++;
    }
    return p;
}

/* --- GENERIC NETLINK TRANSPORT --- */
struct nlmsghdr {
    unsigned int   nlmsg_len;
    unsigned short nlmsg_type;
    unsigned short nlmsg_flags;
    unsigned int   nlmsg_seq;
    unsigned int   nlmsg_pid;
};

/* Returns the payload of the first attribute with the given type, or NULL.
 * The reported length is the raw attribute payload size; string attributes are
 * NUL terminated and NLA aligned, so use nm_str_len() on them. */
static noinline const char *nm_get_attr(const void *nlh, int type, int *out_len) {
    const unsigned short *attr = (const unsigned short *)((const char *)nlh + NLMSG_HDRLEN + GENL_HDRLEN);

    while (attr[0] >= 4) {
        unsigned short len = attr[0];
        int atype = attr[1] & 0x3fff;
        if (atype == type) {
            if (out_len) *out_len = (int)len - 4;
            return (const char *)(attr + 2);
        }
        attr = (const unsigned short *)((const char *)attr + ((len + 3) & ~3));
    }
    return (const char *)0;
}

/* Length of a NUL terminated string attribute, bounded by the attribute size. */
static noinline int nm_str_len(const char *s, int max) {
    int i = 0;
    while (i < max && s[i]) i++;
    return i;
}

static unsigned int nm_seq;

static noinline void nm_build_request(int family, int cmd, int attr_type,
                                       const void *data, int len, int flags) {
    struct nlmsghdr *nlh = (void *)tx_buf;
    nlh->nlmsg_len = NLMSG_HDRLEN + GENL_HDRLEN;
    nlh->nlmsg_type = family;
    nlh->nlmsg_flags = flags;
    nlh->nlmsg_seq = ++nm_seq;
    nlh->nlmsg_pid = 0;

    tx_buf[16] = cmd;
    tx_buf[17] = NM_GENL_FAMILY_VERSION;
    tx_buf[18] = 0;
    tx_buf[19] = 0;

    if (data && len > 0) {
        unsigned short *nla = (void *)(tx_buf + NLMSG_HDRLEN + GENL_HDRLEN);
        nla[0] = 4 + len;
        nla[1] = attr_type;
        memcpy(nla + 2, data, len);
        nlh->nlmsg_len += nla[0];
    }
}

/* Sends one request. Returns the number of bytes read, or a negative errno. */
static noinline int nm_send_raw(int fd, int family, int cmd, int attr_type,
                                const void *data, int len, int flags) {
    nm_build_request(family, cmd, attr_type, data, len, flags);

    long sent = sys3(SYS_WRITE, fd, (long)tx_buf, ((struct nlmsghdr *)tx_buf)->nlmsg_len);
    if (sent < 0) return (int)sent;

    return (int)sys3(SYS_READ, fd, (long)rx_buf, NM_RX_BUF_SIZE);
}

/* Sends one request and interprets the reply. Returns > 0 on success, or a
 * negative errno when the kernel rejected the request. */
static noinline int nm_request(int fd, int family, int cmd, int attr_type,
                               const void *data, int len, int flags) {
    int res = nm_send_raw(fd, family, cmd, attr_type, data, len, flags);
    if (res < 0) return res;

    struct nlmsghdr *nlh = (void *)rx_buf;
    if (nlh->nlmsg_type == NLMSG_ERROR) {
        int err = *(int *)(rx_buf + NLMSG_HDRLEN);
        return err ? err : res;
    }
    return res;
}

/* Resolves the "nomount" generic netlink family id. */
static noinline int nm_family_id(int fd) {
    /* CTRL_ATTR_FAMILY_NAME is an NLA_NUL_STRING: the payload must carry the
     * terminating NUL byte or the controller rejects the request. */
    if (nm_request(fd, 16 /* CTRL */, CTRL_CMD_GETFAMILY, CTRL_ATTR_FAMILY_NAME,
                   NM_GENL_FAMILY, sizeof(NM_GENL_FAMILY), NLM_F_REQUEST) <= 0)
        return -1;

    const char *attr = nm_get_attr(rx_buf, CTRL_ATTR_FAMILY_ID, 0);
    if (!attr) return -1;

    unsigned short id;
    memcpy(&id, attr, sizeof(id));
    return id ? (int)id : -1;
}
