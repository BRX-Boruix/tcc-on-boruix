/* probe_pf_rcx.c v3 —— 3P6-1 根因五：判定"printf 前后 callee-saved 寄存器是否被破坏"。
 *
 * v2 事实：`malloc(4MiB)` 后紧跟 `memset` 会崩，fault = buf + <memset 存储指令地址>；
 *          而反汇编显示 main 把 buf 放在 **%rbx**（callee-saved）里，printf 之后
 *          直接 `movq %rbx, %rdi; call memset`。若 %rbx 被 printf 破坏，rdi 就是垃圾。
 *          同时"哨兵 rcx 跨 16 次 #PF"是 OK 的 —— 所以不是 #PF 破坏 rcx。
 *
 * v3 用**对照实验**判定：同一形态做两次，唯一差别是"memset 之前有没有 printf"。
 *   第一次：malloc -> memset（中间无任何调用）
 *   第二次：malloc -> printf -> memset
 * 若第一次过、第二次崩 ⇒ printf（或其调用链）破坏了 callee-saved 寄存器。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N (4ul * 1024 * 1024)

int main(void) {
    /* ---- 第一次：memset 之前**没有任何调用** ---- */
    unsigned char *a = (unsigned char *)malloc(N);
    if (!a) { printf("[PF3] malloc a failed\n"); return 1; }
    memset(a, 0x5a, N);
    printf("[PF3] A(no-call-before) ok first=%d last=%d\n", a[0], a[N - 1]);

    /* ---- 第二次：同样形态，但 memset 之前插一次 printf ---- */
    unsigned char *b = (unsigned char *)malloc(N);
    if (!b) { printf("[PF3] malloc b failed\n"); return 1; }
    printf("[PF3] B buf=%p n=%lu ...\n", (void *)b, N);
    memset(b, 0x5a, N);
    printf("[PF3] B(with-printf-before) ok first=%d last=%d\n", b[0], b[N - 1]);

    printf("[PF3] all done\n");
    return 0;
}
