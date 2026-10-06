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

#endif /* _BORUIX_CONFIG_H */