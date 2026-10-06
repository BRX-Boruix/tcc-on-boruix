/* probe_pf_rcx.c v4 —— 3P6-1 根因五：判别"是 libsys 的 memset 特有，还是任何跨新页的循环都会崩"。
 *
 * v3 事实：malloc(4MiB) 之后**紧跟** memset（中间无任何调用）必崩，
 *          fault = buf + <memset 存储指令地址>。
 * v4 用 C 的 volatile 逐字节循环触碰同一块新内存（编译器不会向量化、也不会换成 memset），
 *    并核对循环计数是否走完：
 *      - 若**它也崩** → 与 memset 实现无关，是"任何跨大量新页的循环"都会中招；
 *      - 若它**正常走完** → 只剩 libsys 的 memset 这一条路（或它用的那个寄存器）。
 *    之后再对**已全部驻留**的同一块做一次 memset（此时不可能有 #PF），作为正向对照。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    const unsigned long n = 4ul * 1024 * 1024;
    unsigned char *b = (unsigned char *)malloc(n);
    if (!b) {
        printf("[PF4] malloc failed\n");
        return 1;
    }
    printf("[PF4] volatile touch loop: %lu bytes at %p\n", n, (void *)b);

    volatile unsigned char *v = b;
    unsigned long i;
    for (i = 0; i < n; i++) {
        v[i] = 0x5a;
    }
    printf("[PF4] volatile loop done i=%lu (expect %lu) %s\n",
           i, n, (i == n) ? "COUNTER-OK" : "COUNTER-BAD");
    printf("[PF4] first=%d last=%d\n", b[0], b[n - 1]);

    memset(b, 0xa5, n);   /* 整块已驻留：此时不应有任何 #PF */
    printf("[PF4] memset-after-touch ok first=%d last=%d\n", b[0], b[n - 1]);
    printf("[PF4] all done\n");
    return 0;
}
