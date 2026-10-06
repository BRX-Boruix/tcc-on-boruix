/* probe_pf_rcx.c v12 —— 3P6-1 根因五：A/B 已判定 #PF 路径（v11 结果：A 坏 B 好），
 * 本轮把**整个 GPR 文件**在循环出口落进内存，用来判别"哪一种写坏了 rcx"。
 *
 * v11 实测（同一进程内 A/B 对照，见 CRT-AND-LIBS）：
 *   A(fresh,#PF)     : rdi=100000010 rcx=400086(期望 400000) rdx=328010(期望 0) cs=3b MISMATCH
 *   B(pretouched)    : rdi=100401010 rcx=400000 rdx=0 r8=400000 cs=3b OK
 *   ⇒ **触发条件是 #PF 补页路径**（B 把页全部预触碰后同一段 asm 完好无损），IRQ0 被排除。
 *   ⇒ 循环体 0x400085 是存储指令，退出时 rcx = 0x400086 = **存储指令地址 + 1**。
 *     （v9 那次是 0x400082 存储 → rcx=0x400083，同一规律。）
 *
 * 候选解释（v12 用来判别）：
 *   H1「iretq 帧整体下移 5 个 qword」——若成立，则帧里
 *      rcx槽<-rip、rbx槽<-cs(0x3b)、rax槽<-rflags(0x202)，
 *      于是用户态会看到 rcx=0x400085、**rbx=0x3b、rax=0x202**。
 *   H2「只有 rcx 被单独赋值成 rip」——则 rbx/rax 完好（rbx=调用者保存的堆指针、rax 任意）。
 *   H3「sysretq 语义」——rcx 不变、RIP 取自 rcx；但那样用户态 rcx 应是**旧值**（≈0x6bff0），
 *      与实测 0x400086 不符，除非旧值恰等于 RIP。故 H3 只在"帧 rcx 槽 = 帧 rip 槽"时成立。
 *
 * 因此 v12 在循环出口把 15 个 GPR 全量写进 g_regs[15]（asm 直接写内存），随后打印。
 * 判别只看 rbx/rax：被改 ⇒ H1；完好 ⇒ H2/H3。
 */
#include <stdio.h>
#include <stdlib.h>

/* 出口全量快照：r15,r14,r13,r12,r11,r10,r9,r8,rbp,rdi,rsi,rdx,rcx,rbx,rax */
static unsigned long g_regs[15];
static unsigned long g_rsp, g_cs, g_ss;

#define RUN_LOOP(buf, off_, cnt_)                                                \
    do {                                                                         \
        const unsigned long _o = (off_), _c = (cnt_);                            \
        __asm__ __volatile__(                                                    \
            "mov %[b], %%rdi\n\t"                                                \
            "mov %[o], %%rcx\n\t"                                                \
            "mov %[c], %%rdx\n\t"                                                \
            "mov %[e], %%r8\n\t"                                                 \
            "mov $0x5a, %%sil\n\t"                                               \
            "1:\n\t"                                                            \
            "movb %%sil, (%%rdi,%%rcx)\n\t"                                      \
            "incq %%rcx\n\t"                                                    \
            "cmpq %%r8, %%rcx\n\t"                                               \
            "ja 9f\n\t"                                                         \
            "decq %%rdx\n\t"                                                    \
            "jnz 1b\n\t"                                                        \
            "9:\n\t"                                                            \
            "movq %%r15, 0(%[regs])\n\t"                                         \
            "movq %%r14, 8(%[regs])\n\t"                                         \
            "movq %%r13, 16(%[regs])\n\t"                                        \
            "movq %%r12, 24(%[regs])\n\t"                                        \
            "movq %%r11, 32(%[regs])\n\t"                                        \
            "movq %%r10, 40(%[regs])\n\t"                                        \
            "movq %%r9, 48(%[regs])\n\t"                                         \
            "movq %%r8, 56(%[regs])\n\t"                                         \
            "movq %%rbp, 64(%[regs])\n\t"                                        \
            "movq %%rdi, 72(%[regs])\n\t"                                        \
            "movq %%rsi, 80(%[regs])\n\t"                                        \
            "movq %%rdx, 88(%[regs])\n\t"                                        \
            "movq %%rcx, 96(%[regs])\n\t"                                        \
            "movq %%rbx, 104(%[regs])\n\t"                                       \
            "movq %%rax, 112(%[regs])\n\t"                                       \
            "movq %%rsp, %[rspo]\n\t"                                            \
            "xorl %%eax, %%eax\n\t"                                              \
            "movw %%cs, %%ax\n\t"                                                \
            "movq %%rax, %[cso]\n\t"                                             \
            "xorl %%eax, %%eax\n\t"                                              \
            "movw %%ss, %%ax\n\t"                                                \
            "movq %%rax, %[sso]\n\t"                                             \
            : [rspo] "=m"(g_rsp), [cso] "=m"(g_cs), [sso] "=m"(g_ss)               \
            : [b] "r"(buf), [o] "r"(_o), [c] "r"(_c), [e] "r"(_o + _c),           \
              [regs] "r"(g_regs)                                                   \
            : "rdi", "rcx", "rdx", "rsi", "r8", "rax", "memory");                  \
    } while (0)

static void report(const char *tag, unsigned long buf, unsigned long off,
                   unsigned long cnt) {
    unsigned long rcx = g_regs[12], rdx = g_regs[11], rdi = g_regs[9], r8 = g_regs[7];
    int ok = (rdi == buf && rcx == off + cnt && rdx == 0 && r8 == off + cnt);
    printf("[PF12] %s rdi=%lx(buf=%lx) rcx=%lx(exp %lx) rdx=%lx(exp 0) r8=%lx %s\n",
           tag, rdi, buf, rcx, off + cnt, rdx, r8, ok ? "OK" : "MISMATCH");
    printf("[PF12] %s GPR: r15=%lx r14=%lx r13=%lx r12=%lx r11=%lx r10=%lx r9=%lx "
           "r8=%lx rbp=%lx rdi=%lx rsi=%lx rdx=%lx rcx=%lx rbx=%lx rax=%lx cs=%lx ss=%lx rsp=%lx\n",
           tag, g_regs[0], g_regs[1], g_regs[2], g_regs[3], g_regs[4], g_regs[5],
           g_regs[6], g_regs[7], g_regs[8], g_regs[9], g_regs[10], g_regs[11],
           g_regs[12], g_regs[13], g_regs[14], g_cs, g_ss, g_rsp);
}

int main(void) {
    const unsigned long n = 4ul * 1024 * 1024;
    const unsigned long off = 0x10, cnt = n - 0x10;

    /* A：全新（未触碰）缓冲 —— 循环期间每 16KB 一次 #PF。 */
    unsigned char *a = (unsigned char *)malloc(n);
    if (!a) { printf("[PF12] malloc A failed\n"); return 1; }
    printf("[PF12] A buf=%p (fresh)\n", (void *)a);
    RUN_LOOP(a, off, cnt);
    report("A", (unsigned long)a, off, cnt);

    /* B：预触碰全部页后同一段循环 —— 循环期间无 #PF（阳性对照）。 */
    unsigned char *b = (unsigned char *)malloc(n);
    if (!b) { printf("[PF12] malloc B failed\n"); return 1; }
    for (unsigned long i = 0; i < n; i += 4096) {
        ((volatile unsigned char *)b)[i] = 0x11;
    }
    printf("[PF12] B buf=%p (pre-touched)\n", (void *)b);
    RUN_LOOP(b, off, cnt);
    report("B", (unsigned long)b, off, cnt);

    printf("[PF12] all done\n");
    return 0;
}
