/* probe_pf_rcx.c v9 —— 3P6-1 根因五：给循环加**独立硬上界**，从而在被破坏时仍能存活并报告。
 *
 * v8 事实：形态升级——循环跑过了预期终点（fault_addr 恰为堆断点 0x100401000），
 *          且 ret=0x0（frame.rsp 也坏）=> 中断帧多字段被破坏。
 * v9 的 asm 用 \`%r8\` 存一个**独立硬上界**（off+count）：即使 \`rdx\`（倒计数）被改大，
 *    循环也会在 \`rcx\` 越过硬上界时退出，然后**打印三个寄存器**，从而回答：
 *      - 到底是 \`rdx\` 被改大（循环因此想跑更远）？
 *      - 还是 \`rcx\` 被跳大（硬上界立刻触发）？
 *      - 还是 \`rdi\` 被改（写地址整体偏移）？
 *    三种情形打印出的值不同，一次即可判定。且因为硬上界保证不越界，**不会再崩**。
 */
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    const unsigned long n = 4ul * 1024 * 1024;
    unsigned char *b = (unsigned char *)malloc(n);
    if (!b) {
        printf("[PF9] malloc failed\n");
        return 1;
    }
    printf("[PF9] buf=%p\n", (void *)b);

    unsigned long rdi_end = 0, rcx_end = 0, rdx_end = 0;
    const unsigned long off = 0x10, cnt = n - 0x10;
    __asm__ __volatile__(
        "mov %[b], %%rdi\n\t"
        "mov %[o], %%rcx\n\t"
        "mov %[c], %%rdx\n\t"
        "mov %[e], %%r8\n\t"
        "mov $0x5a, %%sil\n\t"
        "1:\n\t"
        "movb %%sil, (%%rdi,%%rcx)\n\t"
        "incq %%rcx\n\t"
        "cmpq %%r8, %%rcx\n\t"
        "ja 9f\n\t"
        "decq %%rdx\n\t"
        "jnz 1b\n\t"
        "9:\n\t"
        "mov %%rdi, %[ro]\n\t"
        "mov %%rcx, %[rco]\n\t"
        "mov %%rdx, %[rdo]\n\t"
        : [ro] "=r"(rdi_end), [rco] "=r"(rcx_end), [rdo] "=r"(rdx_end)
        : [b] "r"(b), [o] "r"(off), [c] "r"(cnt), [e] "r"(off + cnt)
        : "rdi", "rcx", "rdx", "rsi", "r8", "memory");

    printf("[PF9] rdi=%lx (buf=%lx) rcx=%lx (expect %lx) rdx=%lx (expect 0) %s\n",
           rdi_end, (unsigned long)b, rcx_end, off + cnt, rdx_end,
           (rdi_end == (unsigned long)b && rcx_end == off + cnt && rdx_end == 0)
               ? "OK" : "MISMATCH");
    printf("[PF9] all done\n");
    return 0;
}
