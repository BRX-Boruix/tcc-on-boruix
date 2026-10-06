/* probe_heap.c —— 诊断：量出本进程的堆上限（3P6-1）。
 *
 * 背景：tcc 链接时 `tcc_malloc` 拿到 NULL，报 `memory full`。libc 的 malloc 经 `brk` 增长，
 * 所以这里直接量：能连续 malloc 多少、在哪一次失败、errno 是什么。
 *
 * **逐步打印**（每 8 次一次 + 失败那次必打）：上一版把打印放在循环之后，一崩就什么都不剩。
 * 而且每次先打地址、再触碰首尾——这样"拿到地址"与"地址可用"是两件事，分开报。
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

int main(void) {
    unsigned long total = 0;
    const unsigned long CHUNK = 1024UL * 1024UL;
    int i;
    printf("probe_heap 开始, 每块 %lu 字节\n", CHUNK);
    for (i = 0; i < 1024; i++) {
        errno = 0;
        void *p = malloc(CHUNK);
        if (!p) {
            printf("#%d malloc 失败 errno=%d, 累计 %lu KB\n", i, errno, total / 1024);
            break;
        }
        if (i < 4 || (i % 8) == 0) {
            printf("#%d addr=%p", i, p);
        }
        ((volatile char *)p)[0] = 1;
        if (i < 4 || (i % 8) == 0) {
            printf(" 头ok");
        }
        ((volatile char *)p)[CHUNK - 1] = 1;
        if (i < 4 || (i % 8) == 0) {
            printf(" 尾ok 累计=%luKB\n", (total + CHUNK) / 1024);
        }
        total += CHUNK;
    }
    printf("循环结束: i=%d 累计=%lu KB\n", i, total / 1024);
    return 0;
}