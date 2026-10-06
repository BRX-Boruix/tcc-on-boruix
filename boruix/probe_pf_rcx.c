/* probe_pf_rcx.c v8 —— 3P6-1 根因五：判别被污染的是 rcx 还是"编译器分配的上界寄存器"。
 *
 * v7 读数 rcx=0x40006e（期望 0x400000）。算术上"逐 1 递增不可能跳过"，故唯一自洽解释是
 * **上界寄存器被改成了 0x40006e**，rcx 是正常递增出来的（见 CRT-AND-LIBS 第 45 次更正）。
 *
 * v8 把上界做成**循环内部自带的倒计数**（\`mov cnt,rdx; decq rdx; jnz\`），只在进入时读一次
 * 编译器给的 cnt，循环内不再引用任何编译器分配的寄存器作为边界：
 *   - 若 rcx 正确走到 0x400000 且 rdx=0  => 上一版坏的是编译器分配的 end 寄存器；
 *   - 若 rcx 仍不对                     => 坏的是 rcx 本身。
 * 两种结果都指向"内核把帧 rcx 槽里的代码地址恢复进了用户 GPR"。
 */
#include <stdio.h>
#include <stdlib.h>

static void write_span2(unsigned char *base, unsigned long off, unsigned long count,
                        unsigned long *rcx_out, unsigned long *rdx_out)
{
    unsigned long r = 0, d = 0;
    __asm__ __volatile__(
        "mov %[b], %%rdi\n\t"
        "mov %[o], %%rcx\n\t"
        "mov %[c], %%rdx\n\t"
        "mov $0x5a, %%sil\n\t"
        "1:\n\t"
        "movb %%sil, (%%rdi,%%rcx)\n\t"
        "incq %%rcx\n\t"
        "decq %%rdx\n\t"
        "jnz 1b\n\t"
        "mov %%rcx, %[rout]\n\t"
        "mov %%rdx, %[dout]\n\t"
        : [rout] "=r"(r), [dout] "=r"(d)
        : [b] "r"(base), [o] "r"(off), [c] "r"(count)
        : "rdi", "rcx", "rdx", "rsi", "memory");
    *rcx_out = r;
    *rdx_out = d;
}

int main(void) {
    const unsigned long n = 4ul * 1024 * 1024;
    unsigned char *b = (unsigned char *)malloc(n);
    if (!b) {
        printf("[PF8] malloc failed\n");
        return 1;
    }
    printf("[PF8] buf=%p\n", (void *)b);

    unsigned long rcx = 0, rdx = 1;
    write_span2(b, 0x10, n - 0x10, &rcx, &rdx);
    printf("[PF8] rcx=%lx expect=%lx  rdx=%lx expect=0  %s\n",
           rcx, n, rdx,
           (rcx == n && rdx == 0) ? "BOTH-OK" : "MISMATCH");

    printf("[PF8] all done\n");
    return 0;
}
