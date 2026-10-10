/* tccmt.c —— libtcc「多线程共用」能力探针（tcc-on-boruix 移植侧机内验收）。
 *
 * 目的：把「不支持多线程共用 libtcc」从一句声明变成**机内实测**。
 *
 * 根因（全部指向上游源码的真实符号，不是模拟）：
 *   - ~tcc_state~ 是**进程级全局**（libtcc.c:73 ~ST_DATA struct TCCState *tcc_state;~）；
 *   - 公开 API 的每次调用都经 ~tcc_enter_state~ / ~tcc_exit_state~（libtcc.c:138/146）
 *     设置与清空该全局；
 *   - 这两个函数**唯一**的保护是 ~tcc_compile_sem~（libtcc.c:74），而它的存在与否由
 *     ~CONFIG_TCC_SEMLOCK~ 决定（tcc.h:1934 ~#if CONFIG_TCC_SEMLOCK~）。本移植把该宏
 *     置 0（boruix/config.h），于是 ~TCC_SEM~ / ~WAIT_SEM~ / ~POST_SEM~ 展开为空——
 *     临界区**没有任何互斥**。
 *
 * 探针做什么：起两个线程，各自 ~tcc_enter_state(自己的状态)~ 后**停在临界区内等主线程
 * 放行**，主线程有界等待「两者都在内」。观察到两者同时在内即为矛盾本身：
 * ~tcc_state~ 只能等于其中一个，而两个调用方都认为自己拥有它。
 *
 * ## 两个实测踩出来的实现要点（都不是风格问题）
 *
 * 1. **会合而非各自定长自旋**：第一版让两个线程各转固定圈数，结果 ~overlap=0~——
 *    两次派生之间只要有一次调度空档，先起的那条就已转完退出。改成「进临界区后等主线程
 *    放行」后，重叠与否只取决于两条线程能否**同时**在内，与调度快慢无关。
 * 2. **~tcc_state~ 必须以 volatile 读**：它是普通全局，而 ~-O2~ 完全可以把它的 load
 *    提到循环之外（循环里没有可见的写者）——那样 ~g_clobbered~ 永远为 0，探针会把
 *    「编译器优化掉了检查」误报成「没有损坏」。
 *
 * 两种构建下本探针**都会终止**（不会挂死）：
 *   - ~CONFIG_TCC_SEMLOCK 0~（当前移植）：两个线程同时在内 ⇒ 重叠=1、被改写=1；
 *   - ~CONFIG_TCC_SEMLOCK 1~（上游默认）：后进入者阻塞在信号量上，主线程等满预算后
 *     放行 ⇒ 重叠=0。此时断言失败并明确报出「前提已变」，而不是静默通过。
 */
#include <stdio.h>

#include "libtcc.h"

typedef unsigned long ulong;

/* libtcc.c 导出但未写进 libtcc.h 的内部入口（llvm-nm 已确认是全局符号）。 */
extern TCCState *tcc_state;
void tcc_enter_state(TCCState *s1);
void tcc_exit_state(TCCState *s1);
/* libc 的 yield（boruix.h 声明、liblibc.a 已定义）：主线程等待期间必须让出 CPU，
 * 否则在只有一个可运行体的核上会把两条工作线程饿死，"没观察到重叠"就变成测量假象。 */
extern int yield_sys(void);

/* ~tcc_state~ 的 volatile 视图：强制每次真正读内存（见文件头第 2 点）。 */
static TCCState *volatile *vs(void) {
    return (TCCState *volatile *)&tcc_state;
}

/* ---- 内核 syscall 直连（与 csrc/crtrt.c 同 ABI：int 0x80） ---- */
#define SYS_MEMORY_MAP 0x21UL
#define SYS_TASK_EXIT 0x34UL
#define SYS_TASK_THREAD_SPAWN 0x35UL
#define SYS_TASK_THREAD_JOIN 0x36UL

/* a4..a6 **必须显式落进 r10/r8/r9 并传 0**：内核从保存帧读这四个参数位，不传就是
 * 读寄存器垃圾。实测代价：只写三参数的版本里 ~SYS_MEMORY_MAP~ 的 prot（a4）拿到垃圾位，
 * 映射被内核如实拒绝，线程派生随之全线 -1。与 csrc/crtrt.c 的 ~boruix_syscall6~ 同形。 */
static long sys6(ulong nr, ulong a1, ulong a2, ulong a3, ulong a4, ulong a5, ulong a6) {
    long ret;
    register ulong r10 asm("r10") = a4;
    register ulong r8 asm("r8") = a5;
    register ulong r9 asm("r9") = a6;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(nr), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8), "r"(r9)
                     : "rcx", "r11", "memory");
    return ret;
}

static long sys3(ulong nr, ulong a1, ulong a2, ulong a3) {
    return sys6(nr, a1, a2, a3, 0, 0, 0);
}

#define STACK_BYTES 0x20000UL

/* 主线程等待「两者都在临界区内」的预算。取值要求：远大于「派生第二条线程并让它跑到
 * 临界区」所需的时间；同时有界，故在有互斥的构建里不会永久挂住。 */
#define MAIN_SPIN 40000000UL
/* 观察到两者都在内之后的宽限期：让两条线程各跑一遍自己的检查再放行。 */
#define GRACE_SPIN 4000000UL

static volatile ulong g_entered;   /* bit0 = 线程 A 已在内, bit1 = 线程 B 已在内 */
static volatile ulong g_release;   /* 主线程放行 */
static volatile ulong g_overlap;   /* 主线程观察到两个临界区同时存在 */
static volatile ulong g_clobbered; /* 线程在内时读到的 tcc_state 不是自己的 */
static volatile ulong g_sink;

struct worker_arg {
    TCCState *state;
    ulong bit;
};

__attribute__((used)) void thread_body(struct worker_arg *a) {
    ulong my = a->bit;

    tcc_enter_state(a->state);
    __sync_fetch_and_or(&g_entered, my);

    while (!g_release) {
        /* 这是真正的损坏证据：调用方仍在自己的临界区内，而全局已经指向对端的状态。
         * libtcc.c 里 ~TCCState *s1 = tcc_state;~ 形式的读取（error1 / tcc_close）
         * 拿到的将是对端的上下文。 */
        if (*vs() != a->state) g_clobbered = 1;
    }

    __sync_fetch_and_and(&g_entered, ~my);
    tcc_exit_state(a->state);

    sys3(SYS_TASK_EXIT, 0, 0, 0);
    for (;;) {}
}

/* 内核以 rdi = starter 进入入口；对齐栈后按 SysV 约定转交 thread_body。 */
__attribute__((naked)) static void thread_entry(void) {
    __asm__ volatile(
        ".intel_syntax noprefix\n\t"
        "and rsp, -16\n\t"
        "call thread_body\n\t"
        "ud2\n\t");
}

static ulong spawn(struct worker_arg *a, const char *who) {
    long stack = sys3(SYS_MEMORY_MAP, STACK_BYTES, 0, 0);
    printf("[tccmt] %s: mmap(%lu) -> 0x%lx\n", who, (unsigned long)STACK_BYTES,
           (unsigned long)stack);
    if (stack < 0) return (ulong)-1;
    long tid = sys3(SYS_TASK_THREAD_SPAWN, (ulong)&thread_entry,
                    (ulong)stack + STACK_BYTES, (ulong)a);
    printf("[tccmt] %s: thread_spawn -> %ld\n", who, tid);
    return (ulong)tid;
}

int main(int argc, char **argv) {
    TCCState *a, *b;
    struct worker_arg wa, wb;
    ulong ta, tb, i;
    int both = 0;
    int rc = 0;
    (void)argc;
    (void)argv;

    printf("[tccmt] === libtcc shared-state probe ===\n");

    a = tcc_new();
    b = tcc_new();
    printf("[tccmt] tcc_new x2 -> a=%p b=%p\n", (void *)a, (void *)b);
    if (!a || !b) {
        printf("[tccmt] FAIL: tcc_new returned NULL\n");
        return 2;
    }

    /* tcc_new 不触碰全局：全局只在 enter/exit 之间有效。 */
    printf("[tccmt] tcc_state before enter = %p (expect 0)\n", (void *)tcc_state);
    if (tcc_state != 0) {
        printf("[tccmt] FAIL: tcc_state not cleared\n");
        return 3;
    }

    /* 真实 libtcc 的可用性门：本构建必须能单线程编译，否则后面的竞争结论无意义。 */
    tcc_set_output_type(a, TCC_OUTPUT_MEMORY);
    tcc_set_output_type(b, TCC_OUTPUT_MEMORY);
    {
        int ra = tcc_compile_string(a, "int boruix_probe_fn(void){return 42;}");
        int rb = tcc_compile_string(b, "int boruix_probe_fn2(void){return 7;}");
        printf("[tccmt] single-thread compile A rc=%d B rc=%d\n", ra, rb);
        if (ra != 0 || rb != 0) {
            printf("[tccmt] FAIL: libtcc cannot compile in this build\n");
            return 4;
        }
    }

    /* ---- 竞争：两个线程各自进入自己状态的临界区并停住 ---- */
    wa.state = a;
    wa.bit = 1;
    wb.state = b;
    wb.bit = 2;
    ta = spawn(&wa, "A");
    tb = spawn(&wb, "B");
    printf("[tccmt] spawned tidA=%ld tidB=%ld\n", (long)ta, (long)tb);
    if ((long)ta < 0 || (long)tb < 0) {
        printf("[tccmt] FAIL: spawn failed\n");
        return 5;
    }

    for (i = 0; i < MAIN_SPIN; i++) {
        if ((g_entered & 3UL) == 3UL) {
            both = 1;
            break;
        }
        if ((i & 0xFFFFUL) == 0) yield_sys();
    }
    if (both) g_overlap = 1;
    for (i = 0; i < GRACE_SPIN; i++) g_sink = i;
    g_release = 1;

    sys3(SYS_TASK_THREAD_JOIN, ta, 0, 0);
    sys3(SYS_TASK_THREAD_JOIN, tb, 0, 0);

    printf("[tccmt] both-inside=%d overlap=%lu clobbered=%lu\n", both, g_overlap,
           g_clobbered);
    printf("[tccmt] tcc_state after both exited = %p (expect 0)\n", (void *)tcc_state);

    /* 断言的是**当前构建的真实前提**：无互斥 ⇒ 两个临界区重叠、全局被改写。
     * 一旦有人启用 CONFIG_TCC_SEMLOCK，这里必然失败——那正是「前提已变、文档需
     * 重新裁定」的信号，不是把失败当噪声放过。 */
    if (g_overlap != 1) {
        printf("[tccmt] FAIL: critical sections did not overlap -> mutual exclusion IS\n");
        printf("[tccmt]       in effect; the documented single-threaded limitation no\n");
        printf("[tccmt]       longer holds as stated.\n");
        rc = 6;
    }
    if (g_clobbered != 1) {
        printf("[tccmt] FAIL: tcc_state never pointed at the peer state -> no corruption\n");
        rc = 7;
    }

    tcc_delete(a);
    tcc_delete(b);

    if (rc == 0)
        printf("[tccmt] PASS: tcc_state is shared with no mutual exclusion (SEMLOCK=0)\n");
    return rc;
}
