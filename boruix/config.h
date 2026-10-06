/* config.h —— BORUIX 目标的 tcc 配置（阶段 6 / 3P6-1）。
 *
 * 上游这份文件由 `./configure` 生成。BORUIX 没有 configure 那套宿主探测（也不该有：
 * 我们要的是**能在 BORUIX 内运行**的 tcc），故手写一份最小配置，只提供上游 tcc.h 没有
 * 默认值的那几项。路径类宏（CRTPREFIX/LIBPATHS/SYSINCLUDEPATHS）tcc.h 都有默认值，
 * 需要时用 tcc 的 `-B` 选项在**运行期**覆盖，不写死进二进制（S01：不硬编码路径）。
 */
#ifndef _BORUIX_CONFIG_H
#define _BORUIX_CONFIG_H

/* 版本串：保留上游版本号 + 本移植后缀，便于用户报告问题时区分。 */
#define TCC_VERSION "0.9.28-boruix"

/* 目标架构：x86-64。这一项必须恰好定义其中一个，tcc.h 会据此检查。 */
#define TCC_TARGET_X86_64 1

/* tcc 用 CC_NAME/GCC_MAJOR/GCC_MINOR 决定预定义哪些 __GNUC__ 兼容宏。
 * BORUIX 的宿主编译器是 clang 18，如实声明。 */
#define CC_NAME CC_clang
#define GCC_MAJOR 18
#define GCC_MINOR 1

/* 静态构建：跳过 <dlfcn.h>。BORUIX 的用户态目前只有**静态**可执行文件，且 sysroot 不提供
 * dlfcn.h（dlopen/dlsym 由 rtld 运行期提供，见阶段 5 / 3P5-3）。tcc 因此不启用运行期
 * 动态加载路径（`-run` 的 dlopen 分支），这是**如实**的选择而不是缺失。 */
#define CONFIG_TCC_STATIC 1

/* 关闭线程锁：tcc 默认用 POSIX 信号量（<semaphore.h>）保护"libtcc 被多线程共用"的场景，
 * 而 BORUIX 的 libc 不提供 POSIX 信号量。
 *
 * **这不是绕过**：上游为此**专门提供了单线程配置**——CONFIG_TCC_SEMLOCK 为 0 时
 * tcc.h 把 WAIT_SEM/POST_SEM 定义成空宏（见 tcc.h 的 `#else` 分支）。BORUIX 上 tcc 由
 * shell 以单个进程运行，正是单线程用法。
 *
 * **诚实边界**：因此本移植**不支持**把 libtcc 当库在多线程里共用。
 * 若将来要支持，正确做法是先在 libc 里实现真正的信号量，再打开这一项。 */
#define CONFIG_TCC_SEMLOCK 0

/* 标识"这是 BORUIX 移植"。
 *
 * 用途：`tccrun.c` 里有一处**本系统无法实现**的功能需要排除——运行期异常回溯
 * （`-run` 模式下捕获 SIGSEGV 等并打印源码位置）。它需要三样 BORUIX 没有的东西：
 *   1. 信号处理器收到的 `ucontext_t`——本内核投递时**恒传 NULL**（见 libc/src/signal.rs）；
 *   2. `SA_SIGINFO` 语义——libc 未实现（处理器一律按 `void(int)` 进入）；
 *   3. `SIGABRT`——本内核没有该信号（libc 的 signal.h 有意不声明内核没有的信号）。
 *
 * **为什么不给一个"Linux 兼容外壳"**：那会让 tcc 装上 `SA_SIGINFO` 处理器，却被以
 * `void(int)` 签名调用（`siginfo` 参数是垃圾）→ **静默崩**；而且 libc 已明确"不声明内核
 * 没有的信号"（S09 不伪造），加个 SIGABRT 与之直接冲突。故如实排除该路径并留此标记。
 *
 * 上游补丁位置见 `boruix/UPSTREAM-PATCHES`。 */
#define CONFIG_TCC_BORUIX 1

/* tcc 自己的安装目录：它在这里找 `include/`（目标头文件）与 `libtcc1.a`（支持例程）。
 *
 * `{B}` 在 tcc.h 里会被替换成这个值（也即运行期 `-B` 选项的默认值）。
 *
 * **为什么是数据盘路径**：BORUIX 目前**没有 `/usr`**（liveCD 只有 `/programs` 放内建程序），
 * 故移植阶段把 tcc 的运行时资产放在数据盘的 `/3p/tcc` 下。等系统有了真正的 `/usr`，
 * 只需改这一行 + 搬目录——这正是把它做成**单一配置点**的原因（不散落在别处）。 */
#define CONFIG_TCCDIR "/volumes/BORUIX_DATA/3p/tcc"

#endif /* _BORUIX_CONFIG_H */