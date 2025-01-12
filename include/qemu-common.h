
/* Common header file that is included by all of QEMU.
 *
 * This file is supposed to be included only by .c files. No header file should
 * depend on qemu-common.h, as this would easily lead to circular header
 * dependencies.
 *
 * If a header file uses a definition from qemu-common.h, that definition
 * must be moved to a separate header file, and the header that uses it
 * must include that header.
 */
#ifndef QEMU_COMMON_H
#define QEMU_COMMON_H

#include "qemu/fprintf-fn.h"

#if defined(__arm__) || defined(__sparc__) || defined(__mips__) || defined(__hppa__) || defined(__ia64__)
#define WORDS_ALIGNED
#endif

#define TFR(expr) do { if ((expr) != -1) break; } while (errno == EINTR)

#include "qemu/option.h"
#include "qemu/host-utils.h"

void cpu_ticks_init(void);

/* icount */
void configure_icount(QemuOpts *opts, Error **errp);
extern int use_icount;
extern int icount_align_option;
/* drift information for info jit command */
extern int64_t max_delay;
extern int64_t max_advance;
void dump_drift_info(FILE *f, fprintf_function cpu_fprintf);

#include "qemu/bswap.h"

/* FIXME: Remove NEED_CPU_H.  */
#ifdef NEED_CPU_H
#include "cpu.h"
#endif /* !defined(NEED_CPU_H) */

/* main function, renamed */
#if defined(CONFIG_COCOA)
int qemu_main(int argc, char **argv, char **envp);
#endif

void qemu_get_timedate(struct tm *tm, int offset);
int qemu_timedate_diff(struct tm *tm);

#define qemu_isalnum(c)		isalnum((unsigned char)(c))
#define qemu_isalpha(c)		isalpha((unsigned char)(c))
#define qemu_iscntrl(c)		iscntrl((unsigned char)(c))
#define qemu_isdigit(c)		isdigit((unsigned char)(c))
#define qemu_isgraph(c)		isgraph((unsigned char)(c))
#define qemu_islower(c)		islower((unsigned char)(c))
#define qemu_isprint(c)		isprint((unsigned char)(c))
#define qemu_ispunct(c)		ispunct((unsigned char)(c))
#define qemu_isspace(c)		isspace((unsigned char)(c))
#define qemu_isupper(c)		isupper((unsigned char)(c))
#define qemu_isxdigit(c)	isxdigit((unsigned char)(c))
#define qemu_tolower(c)		tolower((unsigned char)(c))
#define qemu_toupper(c)		toupper((unsigned char)(c))
#define qemu_isascii(c)		isascii((unsigned char)(c))
#define qemu_toascii(c)		toascii((unsigned char)(c))

void *qemu_oom_check(void *ptr);

ssize_t qemu_write_full(int fd, const void *buf, size_t count)
    QEMU_WARN_UNUSED_RESULT;

#ifndef _WIN32
int qemu_pipe(int pipefd[2]);
/* like openpty() but also makes it raw; return master fd */
int qemu_openpty_raw(int *aslave, char *pty_name);
#endif

#ifdef _WIN32
/* MinGW needs type casts for the 'buf' and 'optval' arguments. */
#define qemu_getsockopt(sockfd, level, optname, optval, optlen) \
    getsockopt(sockfd, level, optname, (void *)optval, optlen)
#define qemu_setsockopt(sockfd, level, optname, optval, optlen) \
    setsockopt(sockfd, level, optname, (const void *)optval, optlen)
#define qemu_recv(sockfd, buf, len, flags) recv(sockfd, (void *)buf, len, flags)
#define qemu_sendto(sockfd, buf, len, flags, destaddr, addrlen) \
    sendto(sockfd, (const void *)buf, len, flags, destaddr, addrlen)
#else
#define qemu_getsockopt(sockfd, level, optname, optval, optlen) \
    getsockopt(sockfd, level, optname, optval, optlen)
#define qemu_setsockopt(sockfd, level, optname, optval, optlen) \
    setsockopt(sockfd, level, optname, optval, optlen)
#define qemu_recv(sockfd, buf, len, flags) recv(sockfd, buf, len, flags)
#define qemu_sendto(sockfd, buf, len, flags, destaddr, addrlen) \
    sendto(sockfd, buf, len, flags, destaddr, addrlen)
#endif

void tcg_exec_init(unsigned long tb_size);
bool tcg_enabled(void);

void cpu_exec_init_all(void);

/* Unblock cpu */
void qemu_cpu_kick_self(void);

/* work queue */
struct qemu_work_item {
    struct qemu_work_item *next;
    void (*func)(void *data);
    void *data;
    int done;
    bool free;
};


/**
 * Sends a (part of) iovec down a socket, yielding when the socket is full, or
 * Receives data into a (part of) iovec from a socket,
 * yielding when there is no data in the socket.
 * The same interface as qemu_sendv_recvv(), with added yielding.
 * XXX should mark these as coroutine_fn
 */
ssize_t qemu_co_sendv_recvv(int sockfd, struct iovec *iov, unsigned iov_cnt,
                            size_t offset, size_t bytes, bool do_send);
#define qemu_co_recvv(sockfd, iov, iov_cnt, offset, bytes) \
  qemu_co_sendv_recvv(sockfd, iov, iov_cnt, offset, bytes, false)
#define qemu_co_sendv(sockfd, iov, iov_cnt, offset, bytes) \
  qemu_co_sendv_recvv(sockfd, iov, iov_cnt, offset, bytes, true)

/**
 * The same as above, but with just a single buffer
 */
ssize_t qemu_co_send_recv(int sockfd, void *buf, size_t bytes, bool do_send);
#define qemu_co_recv(sockfd, buf, bytes) \
  qemu_co_send_recv(sockfd, buf, bytes, false)
#define qemu_co_send(sockfd, buf, bytes) \
  qemu_co_send_recv(sockfd, buf, bytes, true)

void qemu_progress_init(int enabled, float min_skip);
void qemu_progress_end(void);
void qemu_progress_print(float delta, int max);
const char *qemu_get_vm_name(void);

#define QEMU_FILE_TYPE_BIOS   0
#define QEMU_FILE_TYPE_KEYMAP 1
char *qemu_find_file(int type, const char *name);

/* OS specific functions */
void os_setup_early_signal_handling(void);
char *os_find_datadir(void);
void os_parse_cmd_args(int index, const char *optarg);

#include "qemu/module.h"

/*
 * Hexdump a buffer to a file. An optional string prefix is added to every line
 */

void qemu_hexdump(const char *buf, FILE *fp, const char *prefix, size_t size);

/* vector definitions */
#ifdef __ALTIVEC__
#include <altivec.h>
/* The altivec.h header says we're allowed to undef these for
 * C++ compatibility.  Here we don't care about C++, but we
 * undef them anyway to avoid namespace pollution.
 */
#undef vector
#undef pixel
#undef bool
#define VECTYPE        __vector unsigned char
#define SPLAT(p)       vec_splat(vec_ld(0, p), 0)
#define ALL_EQ(v1, v2) vec_all_eq(v1, v2)
#define VEC_OR(v1, v2) ((v1) | (v2))
/* altivec.h may redefine the bool macro as vector type.
 * Reset it to POSIX semantics. */
#define bool _Bool
#elif defined __SSE2__
#include <emmintrin.h>
#define VECTYPE        __m128i
#define SPLAT(p)       _mm_set1_epi8(*(p))
#define ALL_EQ(v1, v2) (_mm_movemask_epi8(_mm_cmpeq_epi8(v1, v2)) == 0xFFFF)
#define VEC_OR(v1, v2) (_mm_or_si128(v1, v2))
#else
#define VECTYPE        unsigned long
#define SPLAT(p)       (*(p) * (~0UL / 255))
#define ALL_EQ(v1, v2) ((v1) == (v2))
#define VEC_OR(v1, v2) ((v1) | (v2))
#endif

#define BUFFER_FIND_NONZERO_OFFSET_UNROLL_FACTOR 8
static inline bool
can_use_buffer_find_nonzero_offset(const void *buf, size_t len)
{
    return (len % (BUFFER_FIND_NONZERO_OFFSET_UNROLL_FACTOR
                   * sizeof(VECTYPE)) == 0
            && ((uintptr_t) buf) % sizeof(VECTYPE) == 0);
}
size_t buffer_find_nonzero_offset(const void *buf, size_t len);

/*
 * helper to parse debug environment variables
 */
int parse_debug_env(const char *name, int max, int initial);

const char *qemu_ether_ntoa(const MACAddr *mac);
void page_size_init(void);

/* returns non-zero if dump is in progress, otherwise zero is
 * returned. */
bool dump_in_progress(void);

#endif
