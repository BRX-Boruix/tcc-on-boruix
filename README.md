# tcc-on-boruix

**简体中文** | [English](#english)

把 **TCC**（Tiny C Compiler）带到 BORUIX 上——让这个系统能**在自己内部**编译 C 程序。

> **仓库状态：进行中。** 上游源码树已引入，移植工作已开始；12 个核心源文件里 **6 个已能编译通过**，
> 其余 6 个卡在 libc 宽度缺口上（清单见下）。

---

## 目标

BORUIX 目前的编译器情况是：

| 用途 | 现状 |
| --- | --- |
| 系统自身（内核、用户态程序） | Rust 工具链 |
| 少量自由式 C 程序 | clang / LLD **交叉**编译 |

两条路都成立，但都**不是自举的**：编译器不在 BORUIX 上运行。这个仓库补上这一环——
**让 C 编译器在 BORUIX 内部可用**（`docs/TODO/3p.md` 的 `3P6-1`）。

## 为什么先做 TCC 而不是 GCC

TCC 体积小（百 KB 级）、依赖少、本就是为自举设计的，本体是 AOT、**不需要 `PROT_EXEC`**。
先打通它，能把阶段 4 的成果（TLS、environ、PIE、可执行映射、exec 分段装载）一次性兑现为
「系统内能编译真实程序」，并**按真实链接报错**列出 libc 还缺什么（`3P6-2`）。
GCC 留给后续（见 [`gcc-on-boruix`](https://github.com/BRX-Boruix/gcc-on-boruix)）。

## 目录结构

```text
  <上游 tcc 源码树，克隆在仓库根>   # 见 UPSTREAM
  boruix/config.h                   # BORUIX 目标的 tcc 配置（上游由 ./configure 生成）
  boruix/build.py                   # 交叉构建脚本：产出能在 BORUIX 内运行的 tcc.elf
  boruix/_probe_types.c             # 探针：实测定宽类型，PRI 宏必须与之匹配
```

## 构建

```text
python tools/main.py install --prefix <sysroot>     # 在 BORUIX 主仓生成 sysroot
set BORUIX_SYSROOT=<sysroot>
set BORUIX_CLANG=<clang>   &  set BORUIX_LLD=<ld.lld>   # 未在 PATH 上时需显式指定
python boruix/build.py --sysroot %BORUIX_SYSROOT%
```

工具链路径**不写死**（S01）：经环境变量覆盖，否则按 PATH 查找。

## 当前进度

已编译通过（6/12）：`tccgen.c`、`tccdbg.c`、`tccasm.c`、`x86_64-gen.c`、`x86_64-link.c`、`i386-asm.c`。

仍卡在 libc 宽度上（**由真实编译报错导出，不是预猜**）：

| 缺口 | 类别 |
| --- | --- |
| `sys/ucontext.h` | 头文件（`tccrun.c` 的信号处理要用 `ucontext_t`） |
| `environ` | 符号（`tccrun.c`） |
| `struct tm` + `localtime` | 类型 + 函数（`tccpp.c` 的 `__DATE__`/`__TIME__`） |
| `ldexpl` | 函数（`tccpp.c`） |
| `strerror` | 函数 |
| `remove` / `fdopen` / `freopen` | 函数（`tcctools.c`） |
| `execvp` | 函数（`tcctools.c`） |

## 诚实边界

- **不支持多线程共用 libtcc**：BORUIX 的 libc 不提供 POSIX 信号量，故本移植按上游的
  **单线程配置**（`CONFIG_TCC_SEMLOCK 0`）构建——上游为此专门提供了空宏分支，不是绕过。
- **不启用动态加载路径**（`CONFIG_TCC_STATIC`）：BORUIX 的用户态目前只有静态可执行文件。
- `gettimeofday` 的 `tv_usec` 恒为 0（墙钟来自 RTC，只有秒级分辨率；不拿单调时钟凑数）。

## 许可证

**本仓库不附带许可证文件**，这是有意的：

- TCC 由 Fabrice Bellard 等人发布，采用 **GNU 宽通用公共许可证（LGPL）**。许可证的权威文本
  与说明随上游源码一同提供（见仓库根的 `COPYING`）。
- 因此本仓库**不能**统一声明为 MIT 之类的宽松许可证——上游源码在此，整仓授权服从其条款。
  移植代码与上游源码的授权方式需要在引入时逐项明确。

## 相关项目

- [`libc`](https://github.com/BRX-Boruix/libc) —— 面向 C ABI 的 C 库实现（本移植的宽度来源）
- [`csrc`](https://github.com/BRX-Boruix/csrc) —— BORUIX 的自由式 C 运行环境
- [`gcc-on-boruix`](https://github.com/BRX-Boruix/gcc-on-boruix) —— 同一目标的 GCC 版本（后续）

## 上游

- [TinyCC](https://repo.or.cz/tinycc.git) —— 精确版本见 `UPSTREAM`

---

# English

[简体中文](#tcc-on-boruix) | **English**

Bringing **TCC** (Tiny C Compiler) to BORUIX — so the system can compile C programs **from within itself**.

> **Repository status: in progress.** The upstream source tree is vendored and porting has started;
> **6 of 12** core source files already compile, the rest are blocked on libc width (list above).

## Goal

BORUIX's compiler situation today is: the system itself is built with the Rust toolchain, and a few
freestanding C programs are **cross**-compiled with clang / LLD. Both paths work, but neither is
**self-hosting**: the compiler does not run on BORUIX. This repository closes that gap (`3P6-1` in
`docs/TODO/3p.md`).

## Why TCC before GCC

TCC is small (hundreds of KB), has few dependencies, was designed for bootstrapping, and is an AOT
compiler that does **not** need `PROT_EXEC`. Getting it working cashes in the results of stage 4
(TLS, environ, PIE, executable mappings, segmented exec) as "the system can compile real programs",
and yields the list of what libc still lacks **from real link errors** (`3P6-2`). GCC comes later
(see [`gcc-on-boruix`](https://github.com/BRX-Boruix/gcc-on-boruix)).

## Building

See the Chinese section above; toolchain paths are never hardcoded (S01) — override with
`BORUIX_CLANG` / `BORUIX_LLD`, otherwise they are looked up on `PATH`.

## Honest boundaries

- **Using libtcc from multiple threads is unsupported**: BORUIX libc has no POSIX semaphores, so the
  port is built in upstream's **single-threaded configuration** (`CONFIG_TCC_SEMLOCK 0`) — upstream
  provides no-op macro branches for exactly this, so it is a supported configuration, not a bypass.
- **The dynamic-loading path is disabled** (`CONFIG_TCC_STATIC`): BORUIX user space is static-only today.
- `gettimeofday`'s `tv_usec` is always 0 (the wall clock comes from the RTC at one-second resolution;
  we do not stitch in the monotonic clock to look more precise).

## License

**This repository ships no license file**, deliberately: TCC is published under the **GNU Lesser
General Public License (LGPL)**; the authoritative text comes with the upstream sources (see
`COPYING` at the repository root). This repository therefore cannot be declared under a permissive
license — with upstream sources present, its licensing follows theirs.

## Related projects

- [`libc`](https://github.com/BRX-Boruix/libc) — the C-ABI-facing C library implementation
- [`csrc`](https://github.com/BRX-Boruix/csrc) — BORUIX's freestanding C runtime
- [`gcc-on-boruix`](https://github.com/BRX-Boruix/gcc-on-boruix) — the GCC counterpart (later)

## Upstream

- [TinyCC](https://repo.or.cz/tinycc.git) — exact revision recorded in `UPSTREAM`