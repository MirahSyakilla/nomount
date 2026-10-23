/* --- ARCH --- */
#if defined(__aarch64__)
    #define SYS_GETCWD     17
    #define SYS_READ       63
    #define SYS_WRITE      64
    #define SYS_EXIT       93
    #define SYS_ADD_KEY    217

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
    __attribute__((always_inline)) static inline long sys5(long n, long a, long b, long c, long d, long e) {
        register long x8 asm("x8") = n; register long x0 asm("x0") = a; register long x1 asm("x1") = b;
        register long x2 asm("x2") = c; register long x3 asm("x3") = d; register long x4 asm("x4") = e;
        __asm__ __volatile__("svc 0" : "+r"(x0) : "r"(x8), "r"(x1), "r"(x2), "r"(x3), "r"(x4) : "memory", "cc");
        return x0;
    }
    __asm__( ".global _start\n" ".type _start, %function\n" "_start:\n" "mov x0, sp\n" "b c_main\n" );

#elif defined(__arm__)
    #define SYS_EXIT       1
    #define SYS_READ       3
    #define SYS_WRITE      4
    #define SYS_GETCWD     183
    #define SYS_ADD_KEY    309

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
    __attribute__((always_inline)) static inline long sys5(long n, long a, long b, long c, long d, long e) {
        register long r7 asm("r7") = n; register long r0 asm("r0") = a; register long r1 asm("r1") = b;
        register long r2 asm("r2") = c; register long r3 asm("r3") = d; register long r4 asm("r4") = e;
        __asm__ __volatile__("svc 0" : "+r"(r0) : "r"(r7), "r"(r1), "r"(r2), "r"(r3), "r"(r4) : "memory", "cc");
        return r0;
    }
    __asm__( ".global _start\n" ".type _start, %function\n" "_start:\n" "mov r0, sp\n" "b c_main\n");

#elif defined(__x86_64__)
    #define SYS_READ       0
    #define SYS_WRITE      1
    #define SYS_EXIT       60
    #define SYS_GETCWD     79
    #define SYS_ADD_KEY    248

    __attribute__((always_inline)) static inline long sys1(long n, long a) {
        long ret; __asm__ __volatile__("syscall" : "=a"(ret) : "a"(n), "D"(a) : "rcx", "r11", "memory", "cc");
        return ret;
    }
    __attribute__((always_inline)) static inline long sys3(long n, long a, long b, long c) {
        long ret; __asm__ __volatile__("syscall" : "=a"(ret) : "a"(n), "D"(a), "S"(b), "d"(c) : "rcx", "r11", "memory", "cc");
        return ret;
    }
    __attribute__((always_inline)) static inline long sys5(long n, long a, long b, long c, long d, long e) {
        long ret; register long r10 asm("r10") = d; register long r8 asm("r8") = e;
        __asm__ __volatile__("syscall" : "=a"(ret) : "a"(n), "D"(a), "S"(b), "d"(c), "r"(r10), "r"(r8) : "rcx", "r11", "memory", "cc");
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
#define NM_ENODEV       19
#define NM_ENOSYS       38
#define NM_EXIT_OK      0
#define NM_EXIT_USAGE   1
#define NM_EXIT_IO      2
#define NM_EXIT_KERNEL  3
#define NM_EXIT_UNSUP   4

/* --- KERNEL-SIDE CONTRACT (KernelSU-Next kernel/nomount/nomount.h) ---
 *
 * The kernel runtime is driven through the Linux keyring: the client adds a
 * key of the custom "nomount" key type and the kernel applies the command
 * while that key type preparses the payload.  The key is never instantiated,
 * so the command is applied exactly once.
 *
 * The permission check lives in the kernel and requires CAP_SYS_ADMIN in the
 * initial user namespace.
 */
#define NM_KEY_TYPE_NAME    "nomount"
#define NM_KEY_DESCRIPTION  "trigger"
#define NM_MAGIC_SIG        0x4E4F4D4F554E54ULL
/* The kernel replaces the signature with this marker once it has applied the
 * command, so that the client can tell a processed command apart from a
 * command the kernel refused before it ran. */
#define NM_MAGIC_DONE       0x444F4E45ULL

/* Kernel ABI the client was built against.  A mismatch is reported instead of
 * silently mis-parsing the command payload. */
#define NM_KERNEL_ABI_VERSION       16
#define NM_KERNEL_TRANSPORT_VERSION 2

enum {
    NM_CMD_UNSPEC = 0,
    NM_CMD_GET_VERSION = 1,
    NM_CMD_ADD_RULE = 2,
    NM_CMD_DEL_RULE = 3,
    NM_CMD_CLEAR_ALL = 4,
    NM_CMD_ADD_UID = 5,
    NM_CMD_DEL_UID = 6,
    NM_CMD_GET_LIST = 7,
    NM_CMD_CLEAR_RULES = 8,
    NM_CMD_CLEAR_UIDS = 9,
    NM_CMD_GET_UIDS = 10,
    NM_CMD_BLOCK_ISOLATED = 11,
    NM_CMD_GET_ISOLATED_STATE = 12,
};

#define NM_FLAG_IS_DIR   (1U << 1)
#define NM_FLAG_WHITEOUT (1U << 2)
#define NM_FLAG_OPAQUE   (1U << 3)
#define NM_FLAG_MASK     (NM_FLAG_IS_DIR | NM_FLAG_WHITEOUT | NM_FLAG_OPAQUE)

/* Packed payload records, see kernel/nomount/keyring.c.
 *   ADD_RULE: u32 flags | u32 uid_start | u32 uid_end | u32 deny_start |
 *             u32 deny_end | u16 v_len | u16 r_len | vpath | rpath
 *   DEL_RULE: u32 uid_start | u32 uid_end | u32 deny_start | u32 deny_end |
 *             u16 v_len | vpath
 * Paths are length prefixed and never NUL terminated.  The uid scope is
 * u32(u32) with the upstream sentinel: uid_start == uid_end == 0 selects the
 * "all uids" fallback scope (rules match for every uid), while deny_start /
 * deny_end describe the excluded window that falls back to the original
 * path. */
#define NM_RULE_HDR_SIZE  24
#define NM_DEL_HDR_SIZE   18

#define NM_PAYLOAD_BUFFER_SIZE 8160
#define NM_OUT_BUF_SIZE        16384

/* Command payload shared with the kernel.  The layout is identical on 32 bit
 * and 64 bit clients, so the 64 bit members use fixed width types. */
struct nm_payload {
    unsigned long long magic;
    unsigned int cmd;
    unsigned int target_uid;
    int status;
    unsigned int data_size;
    unsigned long long arg1;
    char buffer[NM_PAYLOAD_BUFFER_SIZE];
};

struct nm_version {
    unsigned int abi;
    unsigned int transport;
};

struct nm_workspace {
    struct nm_payload payload;
    char cwd[PATH_MAX];
    char out[NM_OUT_BUF_SIZE];
} __attribute__((aligned(4096)));

_Static_assert(sizeof(struct nm_payload) == 8192, "payload must have a fixed size");
_Static_assert(__builtin_offsetof(struct nm_payload, buffer) == 32,
               "payload buffer must keep its offset");
_Static_assert(sizeof(struct nm_version) == 8, "version must have a fixed size");
_Static_assert(__builtin_offsetof(struct nm_workspace, payload) == 0,
               "payload must start at the workspace boundary");

/* --- CLI MODEL --- */
enum nm_cli_action {
    ACTION_NONE = 0,
    ACTION_RULE_ADD,
    ACTION_RULE_DEL,
    ACTION_RULE_LIST,
    ACTION_UID_ADD,
    ACTION_UID_DEL,
    ACTION_UID_LIST,
    ACTION_RULE_CLEAR,
    ACTION_UID_CLEAR,
    ACTION_BLOCK_ISOLATED,
    ACTION_CLEAR_ALL,
    ACTION_VERSION,
};

struct nm_uid_scope {
    unsigned int start;
    unsigned int end;
};

struct nm_args {
    enum nm_cli_action action;
    int data_start_idx;
    int is_json;
    int is_whiteout;
    int is_opaque;
    /* Rule scope: target window plus the deny window for --uid-excluded. */
    struct nm_uid_scope uid;
    struct nm_uid_scope deny;
    int has_uid;
    unsigned int target_uid;
};

/* Upstream sentinel for the "all uids" fallback scope. */
#define NM_UID_SCOPE_ALL (struct nm_uid_scope){0, 0}

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

/* The workspace holds the command payload and the working directory.  It is a
 * single global object so that the payload keeps a stable address while the
 * kernel writes the command result back into it. */
static struct nm_workspace workspace __attribute__((aligned(4096)));
#define nm_cmd_payload  (&workspace.payload)
#define nm_cwd_buf      (workspace.cwd)
#define nm_out_buf      (workspace.out)

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

/* --- KEYRING TRANSPORT --- */

/* Raw result of the last add_key(2) call, used to report why the kernel
 * refused a command before it ran. */
static long nm_last_errno;

/* Hands one command to the kernel.  The kernel type "nomount" preparses the
 * payload, applies the command and writes the result back into this
 * structure.  Returns the status of the command: 0 on success, negative on
 * failure. */
static noinline int nm_send_payload(struct nm_payload *p) {
    unsigned long long ptr = (unsigned long long)p;

    /* The kernel replaces the signature with the done marker once it has
     * applied a command, so it is stamped again for every command. */
    p->magic = NM_MAGIC_SIG;
    p->status = -1;
    nm_last_errno = sys5(SYS_ADD_KEY, (long)NM_KEY_TYPE_NAME,
                          (long)NM_KEY_DESCRIPTION, (long)&ptr, sizeof(ptr),
                          -1);
    /* The kernel always cancels the key, so the syscall itself fails even
     * when the command was applied.  The command result is the status the
     * kernel wrote back. */
    if (p->magic != NM_MAGIC_DONE) {
        if (nm_last_errno < 0)
            p->status = (int)nm_last_errno;
        return p->status;
    }
    return p->status;
}
