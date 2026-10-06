/* probe_lseek_read.c —— 诊断：lseek 之后的 read 是否完整（3P6-1）。
 *
 * 背景：tcc 的 load_data 先 lseek 再 full_read，且丢弃 full_read 的返回值。
 * 若 lseek 之后的 read 提前返回 0/短读，缓冲区尾部就是未初始化的（已在系统内证实
 * 崩溃源于未初始化内存）。本探针直接量：lseek 之后一次 read 能读回多少字节。
 *
 * 两条路径对比：
 *   A) 不 lseek，直接顺序 read（未跟踪路径）
 *   B) 先 lseek(fd, 0, SEEK_SET)，再 read（跟踪路径 —— 走 pread）
 */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <boruix.h>

#define SZ (1024 * 1024)
static char buf[SZ];

static void report(const char *what, int fd) {
    long n = read(fd, buf, SZ);
    printf("%s: read 返回 %ld 字节\n", what, n);
}

int main(void) {
    const char *p = "/volumes/BORUIX_DATA/3p/tcc/libc.a";
    int fd = open(p, O_RDONLY);
    if (fd < 0) { printf("open 失败\n"); return 1; }
    printf("brk(0)=0x%lx\n", (unsigned long)boruix_brk(0));

    /* A) 顺序读：前 1MB。 */
    report("A 顺序读", fd);

    /* B) lseek 回 0，再读 1MB —— 这条路径走 pread。 */
    long r = lseek(fd, 0, SEEK_SET);
    printf("lseek(0,SEEK_SET) 返回 %ld\n", r);
    report("B lseek 后读", fd);

    /* C) lseek 到一个较大偏移，再读。 */
    r = lseek(fd, 2 * 1024 * 1024, SEEK_SET);
    printf("lseek(2MB,SEEK_SET) 返回 %ld\n", r);
    report("C lseek 2MB 后读", fd);

    /* D) 再 lseek 回 0，连续读两次 1MB，看第二次是否提前停。 */
    lseek(fd, 0, SEEK_SET);
    report("D1", fd);
    report("D2", fd);
    close(fd);
    return 0;
}