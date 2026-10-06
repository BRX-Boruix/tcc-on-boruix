/* probe_brk.c —— 诊断：brk 的查询/扩展语义到底是什么（3P6-1）。
 *
 * 要回答的问题：libsys 的 grow_user_heap 用同一个区间调了 8 次 add_to_heap，
 * 而它每次都以为"扩展成功"。到底是内核没更新断点，还是别的？
 *
 * 直接、逐步地查：每次调用前后都把 brk(0) 打出来。
 */
#include <stdio.h>
#include <errno.h>
#include <boruix.h>

static void show(const char *what, long r) {
    printf("%-22s -> 0x%lx  (brk(0)=0x%lx, errno=%d)\n", what, (unsigned long)r,
           (unsigned long)boruix_brk(0), errno);
}

int main(void) {
    long b0 = boruix_brk(0);
    printf("初始断点 = 0x%lx\n", (unsigned long)b0);

    /* 1) 连续两次查询：应完全相同。 */
    show("brk(0) 再查一次", boruix_brk(0));

    /* 2) 真扩展 +64KB，然后查询。 */
    show("brk(b0+64K)", boruix_brk((unsigned long)b0 + 0x10000));
    show("brk(0) 之后", boruix_brk(0));

    /* 3) 关键：再用**同一个目标**调一次（即 grow_user_heap 重复的情形）。 */
    show("brk(b0+64K) 重复", boruix_brk((unsigned long)b0 + 0x10000));
    show("brk(0) 之后", boruix_brk(0));

    /* 4) 再扩 64KB，看断点是否继续前进。 */
    show("brk(b0+128K)", boruix_brk((unsigned long)b0 + 0x20000));
    show("brk(0) 之后", boruix_brk(0));
    return 0;
}