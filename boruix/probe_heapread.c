/* probe_heapread.c —— 诊断：堆缓冲上的 read 与按需分页（3P6-1）。
 *
 * 背景：tcc 读对象文件时，读进**刚 malloc 出来、尚未触碰**的堆缓冲会得到 EFAULT，
 * 而缓冲地址经验算确实在 brk 之内。假设是：内核的 read 用户区**预校验**要求页面
 * 「已映射」，而 brk 区域是**按需分页**的，尚未触碰的页因此被拒。
 *
 * 本探针把两种情形并排问清楚：
 *   A) malloc 后**不触碰**，直接 read
 *   B) malloc 后**先写一遍**（触发补页），再 read
 */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <boruix.h>

#define SZ (256 * 1024)

static void try(const char *tag, int fd, int touch) {
    unsigned char *p = malloc(SZ);
    if (!p) { printf("%s: malloc 失败\n", tag); return; }
    if (touch) {
        for (long i = 0; i < SZ; i += 4096) p[i] = 1;   /* 每页写一个字节 -> 触发补页 */
    }
    printf("%s: buf=%p brk=%p touch=%d\n", tag, (void *)p,
           (void *)(unsigned long)boruix_brk(0), touch);
    errno = 0;
    long n = read(fd, p, SZ);
    printf("%s: read=%ld errno=%d\n", tag, n, errno);
    free(p);
}

int main(void) {
    int fd = open("/volumes/BORUIX_DATA/3p/tcc/libc.a", O_RDONLY);
    if (fd < 0) { printf("open 失败\n"); return 1; }
    try("A 未触碰", fd, 0);
    try("B 已触碰", fd, 1);
    close(fd);
    return 0;
}