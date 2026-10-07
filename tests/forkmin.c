/* forkmin.c —— fork/waitpid 挂起的**最小复现**（目标第 2 轮）。
 *
 * 背景：tcc-on-boruix/tests/wave2.c 的 fork 段在当前环境下 100% 挂起（连续 6 次），
 * 且已判定**不是** libc 改动引入的回归（A/B 回退到第 62 轮状态同样 2/2 挂起）。
 * 现象停在 [cow] clone 之后 —— 即 fork 已成功、子进程已存在，父进程卡在等待处。
 *
 * 本程序把场景缩到最小，并在**每一步都 fflush**，使串口日志能精确显示停在哪一行：
 *   ① 父：fork 之前
 *   ② 子：即将 _exit(42)        <- 若日志停在这行之后，说明子进程没退出去
 *   ③ 父：fork 返回了 pid       <- 若停在这行之前，说明 fork 本身没返回
 *   ④ 父：waitpid 返回          <- 若停在这行之前，说明是 exit->wait 的唤醒问题
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

/* 可选：先分配并**逐页触碰** argv[1] 指定的 KB 数，再 fork。
 * 目的：验证「挂起与地址空间大小相关」这一假设——forkmin 裸跑是**通过**的，
 * 而 wave2（约 150 页）100% 挂起，故下一步就是把大小当作自变量扫一遍。 */
static char *touch_kb(long kb) {
    long n = kb * 1024;
    char *p = (char *)malloc((size_t)n);
    long i;
    if (p == NULL) {
        return NULL;
    }
    for (i = 0; i < n; i += 4096) {
        p[i] = (char)(i & 0x7f);
    }
    return p;
}

int main(int argc, char **argv) {
    int pid;
    int st = 0;
    int got;
    char *blob = NULL;

    if (argc > 1) {
        long kb = atol(argv[1]);
        blob = touch_kb(kb);
        if (blob == NULL) {
            printf("forkmin: 0) malloc/touch %ld KB FAILED\n", kb);
            fflush(stdout);
            return 2;
        }
        printf("forkmin: 0) touched %ld KB at %p\n", kb, (void *)blob);
        fflush(stdout);
    }

    printf("forkmin: 1) before fork (pid=%d)\n", (int)getpid());
    fflush(stdout);

    pid = fork();

    if (pid == 0) {
        printf("forkmin: 2) child running, exiting 42\n");
        fflush(stdout);
        _exit(42);
    }

    printf("forkmin: 3) fork returned pid=%d\n", pid);
    fflush(stdout);

    got = waitpid(pid, &st, 0);
    printf("forkmin: 4) waitpid -> %d status=%d WIFEXITED=%d WEXITSTATUS=%d\n",
           got, st, WIFEXITED(st), WEXITSTATUS(st));
    fflush(stdout);

    if (got == pid && WIFEXITED(st) && WEXITSTATUS(st) == 42) {
        printf("forkmin: OK\n");
        return 0;
    }
    printf("forkmin: FAIL\n");
    return 1;
}
