/* probe_heapwall.c —— 量出"堆墙"到底在哪里、以及它是否随物理内存变化。
 *
 * 背景（3P6-1）：tcc 在系统内编译 hello.c 时 tcc_malloc 返回 NULL -> "memory full"。
 * 已排除：
 *   - 256 MiB 下墙 = 单进程配额（physical/4 约 67 MiB），探针在 ~55 MiB 干净 ENOMEM；
 *   - **1 GiB 与 2 GiB 下 tcc 仍报 memory full**，而配额按公式应分别约 247/494 MiB
 *     => 墙不随物理内存走，必是另一处固定上界（或另一条失败路径）。
 *
 * 本探针把每一步 brk 扩展打出来（boruix_heap_diag(1) 的 [C] 行），并在失败时打印
 * brk 真值与累计量。用法：BORUIX_INIT_RUN=/volumes/BORUIX_DATA/3p/probe_hw.elf
 */
#include <stdio.h>
#include <stdlib.h>
#include <boruix.h>

int main(void) {
    boruix_heap_diag(1);
    printf("[HW] start brk=%ld\n", (long)boruix_brk(0));
    unsigned long total = 0;
    int i;
    for (i = 0; i < 600; i++) {
        void *p = malloc(1024 * 1024);
        if (!p) {
            printf("[HW] FAIL #%d total=%luKB brk=%ld\n", i, total / 1024,
                   (long)boruix_brk(0));
            return 1;
        }
        ((volatile unsigned char *)p)[0] = 1;
        ((volatile unsigned char *)p)[1024 * 1024 - 1] = 1;
        total += 1024 * 1024;
        if ((i % 16) == 15) {
            printf("[HW] ok #%d total=%luKB brk=%ld\n", i, total / 1024,
                   (long)boruix_brk(0));
        }
    }
    printf("[HW] ALL OK total=%luKB brk=%ld\n", total / 1024, (long)boruix_brk(0));
    return 0;
}
