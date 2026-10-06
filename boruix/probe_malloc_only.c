/* probe_malloc_only.c —— 二分：只用 C 的 malloc，循环里**不做任何 Rust 侧分配**（不 printf）。
 *
 * 为什么：`probe_heap.c` 在循环里 printf（走 libc 的 Rust 代码 → libsys 的 buddy 全局分配器），
 * 于是 C 的 malloc 与 Rust 的 buddy **交替**推进 brk。本探针把两者分开：
 * 循环里只有 malloc + 触碰，**一次 printf 都不做**，循环外才打印一次。
 *
 * 若本探针能干净地走到配额上限 → C 的 malloc 单独没问题 → 问题在两者交互或 buddy 自身。
 * 若它也卡死 → C 的 malloc 单独就有问题。
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

int main(void) {
    const unsigned long CHUNK = 1024UL * 1024UL;
    unsigned long total = 0;
    int i;
    for (i = 0; i < 512; i++) {
        errno = 0;
        void *p = malloc(CHUNK);
        if (!p) {
            break;
        }
        ((volatile char *)p)[0] = 1;
        ((volatile char *)p)[CHUNK - 1] = 1;
        total += CHUNK;
    }
    /* 循环外只打印一次（这一行才触发 Rust 侧分配）。 */
    printf("malloc-only: i=%d total=%lu KB\n", i, total / 1024);
    return 0;
}