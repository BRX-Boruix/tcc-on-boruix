/* probe_argv.c —— 诊断：打印入口桥接实际产出的 argv（3P6-1）。
 *
 * 为什么需要：tcc 报 `file .J not found`（垃圾文件名），说明它拿到的 argv 不对。
 * 与其猜是桥接的错还是 tcc 的错，不如让桥接把 argv 直接打出来。
 *
 * **先打指针、再解引用**：这样即使某个 argv[i] 是坏指针，也能看清它在哪一步坏，
 * 而不是整个进程崩掉什么都不剩（上一版就是这么白跑一次的）。
 *
 * 必须与 user_main_argv.o 链接（默认的 user_main.o 不做拆词）。
 */
#include <stdio.h>

int main(int argc, char **argv) {
    printf("argc = %d\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("argv[%d] ptr = %p\n", i, (void *)argv[i]);
        if (argv[i]) {
            printf("         str = [%s]\n", argv[i]);
        }
    }
    printf("done\n");
    return 0;
}