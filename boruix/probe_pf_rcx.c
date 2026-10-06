/* probe_pf_rcx.c v7 —— 3P6-1 根因五：把"跨页次数"提到 memset 的量级（约 1024 次）。
 *
 * 已确证的事实：
 *   v4：C volatile 循环（计数器不在 rcx）跨约 1024 次 #PF -> **正常**；
 *   v5/v6：asm 循环（计数器在 rcx）跨 1 次 / 17 次 #PF -> **rcx 正确恢复**；
 *   v2：libsys memset（计数器在 rcx）跨约 1024 次 #PF -> **崩**（fault = buf + rip）。
 *
 * v7 用**同一个 asm 循环**把跨度提到约 1024 页（从偏移 0x10 起写 4MiB-0x10），
 * 计数器仍在 rcx。这样把"寄存器"与"跨页次数"两个变量彻底分开：
 *   - 若崩 -> 触发条件是"**rcx 计数 + 上千次 #PF**"（寄存器+规模），与 memset 实现无关；
 *   - 若正常 -> 触发条件只剩 memset 这个函数本身（调用约定/代码形状）。
 */
#include <stdio.h>
#include <stdlib.h>

static unsigned long write_span(unsigned char *base, unsigned long off, unsigned long count)
{
    unsigned long out = 0;
    __asm__ __volatile__(
        "mov %[b], %%rdi\n\t"
        "mov %[o], %%rcx\n\t"
        "mov $0x5a, %%sil\n\t"
        "1:\n\t"
        "movb %%sil, (%%rdi,%%rcx)\n\t"
        "incq %%rcx\n\t"
        "cmpq %[end], %%rcx\n\t"
        "jb 1b\n\t"
        "mov %%rcx, %[out]\n\t"
        : [out] "=r"(out)
        : [b] "r"(base), [o] "r"(off), [end] "r"(off + count)
        : "rdi", "rcx", "rsi", "memory");
    return out;
}

int main(void) {
    const unsigned long n = 4ul * 1024 * 1024;
    unsigned char *b = (unsigned char *)malloc(n);
    if (!b) {
        printf("[PF7] malloc failed\n");
        return 1;
    }
    printf("[PF7] buf=%p\n", (void *)b);

    unsigned long e1 = write_span(b, 0x10, n - 0x10);   /* 约 1024 个页边界 */
    printf("[PF7] span1024 rcx=%lx expect=%lx %s\n", e1, n,
           (e1 == n) ? "RCX-RESTORED" : "RCX-CLOBBERED");

    printf("[PF7] all done\n");
    return 0;
}
