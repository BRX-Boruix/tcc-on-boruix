/* probe_eof.c —— 诊断：读越过文件末尾时的语义（3P6-1）。
 *
 * 背景：tcc 的 load_data() 在读 libc.a 时有一次 400KB 的读返回了 -1，
 * 而它的偏移已越过文件末尾。POSIX 要求此时返回**短读/0**，不是错误。
 * 本探针把三种情形一次问清楚：
 *   1) 偏移在文件内、请求跨过 EOF   -> 应返回「实际可读字节数」（短读）
 *   2) 偏移恰好等于文件大小          -> 应返回 0
 *   3) 偏移远大于文件大小            -> 应返回 0
 */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <boruix.h>

#define SZ (512 * 1024)
static char buf[SZ];

static void at(int fd, long off, long want) {
    errno = 0;
    long r = lseek(fd, off, SEEK_SET);
    long n = read(fd, buf, want);
    printf("off=%-9ld want=%-8ld -> read=%ld errno=%d\n", off, want, n, errno);
}

int main(void) {
    const char *p = "/volumes/BORUIX_DATA/3p/tcc/libc.a";
    int fd = open(p, O_RDONLY);
    if (fd < 0) { printf("open 失败\n"); return 1; }
    /* 文件大小：用 lseek(SEEK_END)（我们实现过，经 fstat）。 */
    long size = lseek(fd, 0, SEEK_END);
    printf("libc.a 大小 = %ld\n", size);
    if (size <= 0) { printf("拿不到大小，改用已知值 4718592\n"); size = 4718592; }

    at(fd, size - 100, 4096);      /* 1) 跨过 EOF */
    at(fd, size, 4096);            /* 2) 恰好在 EOF */
    at(fd, size + 100000, 4096);   /* 3) 远在 EOF 之后 */
    at(fd, 4474090, 409024);       /* 4) 复现 tcc 那次失败 */
    at(fd, 0, 409024);             /* 5) 对照：文件开头正常读 */
    close(fd);
    return 0;
}