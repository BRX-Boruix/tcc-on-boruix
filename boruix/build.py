#!/usr/bin/env python3
"""在宿主机上把 tcc 交叉编译成 BORUIX 可执行文件（阶段 6 / 3P6-1）。

这不是"tcc 的构建系统"——上游的 `./configure && make` 面向 POSIX 宿主，对我们没用。
这里只做一件明确的事：用 BORUIX sysroot 的头文件与 libc，把 tcc 的核心源文件编成
**能在 BORUIX 内运行**的 ELF。

用法:
    python boruix/build.py --sysroot <sysroot 目录> [--compile-only] [--verbose]

sysroot 由 `python tools/main.py install --prefix <dir>` 生成。
工具链经 BORUIX_CLANG / BORUIX_LLD 覆盖，默认按 PATH 查找（S01：不写死路径）。
"""
import argparse
import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.dirname(HERE)          # 上游 tcc 源码树根
BUILD = os.path.join(SRC, "_build")

# 上游 Makefile 的 x86_64_FILES = CORE_FILES + x86_64-gen.c + x86_64-link.c + i386-asm.c
# 源文件清单 = 上游 x86_64_FILES **减去 tcctools.c**。
#
# 为什么减：`tcc.c` 会**无条件** `#include "tcctools.c"`（tcc.c:29），所以它不能再单独编一份，
# 否则 duplicate symbol（实测 tcc_tool_ar/tcc_tool_cross/gen_makedeps）。上游 Makefile 的
# `LIBTCC_SRC = $(filter-out tcc.c tcctools.c, ...)` 正是同一个道理。
CORE = ["tcc.c", "libtcc.c", "tccpp.c", "tccgen.c", "tccdbg.c",
        "tccelf.c", "tccasm.c", "tccrun.c", "x86_64-gen.c", "x86_64-link.c", "i386-asm.c"]


def pick(env, name):
    cand = os.environ.get(env) or shutil.which(name)
    if not cand:
        sys.exit("找不到 %s（可用 %s 指定）" % (name, env))
    return cand


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sysroot", default=os.environ.get("BORUIX_SYSROOT"))
    ap.add_argument("--compile-only", action="store_true")
    ap.add_argument("--verbose", action="store_true")
    a = ap.parse_args()
    if not a.sysroot or not os.path.isdir(a.sysroot):
        sys.exit("需要 --sysroot（或 BORUIX_SYSROOT）指向已安装的 sysroot")
    inc = os.path.join(a.sysroot, "include")
    lib = os.path.join(a.sysroot, "lib")
    cc = pick("BORUIX_CLANG", "clang")
    lld = pick("BORUIX_LLD", "ld.lld")
    os.makedirs(BUILD, exist_ok=True)
    # 旗标与 sysroot 的 C 驱动保持一致，另加 tcc 自己的目标宏。
    cflags = ["--target=x86_64-unknown-none", "-ffreestanding", "-fno-builtin",
              "-fno-stack-protector", "-fno-pic", "-O2",
              "-I", HERE, "-I", inc, "-DTCC_TARGET_X86_64",
              # **-DONE_SOURCE=0 必须**：tcc.c 默认 ONE_SOURCE=1，会把其余 .c 文件 #include
              # 进来；而我们把它们**分开**编译再链接 → 不关掉就会 duplicate symbol（实测）。
              # 上游 Makefile 也是这么编 tcc.o 的（Makefile:240）。
              "-DONE_SOURCE=0"]
    objs, bad = [], 0
    for f in CORE:
        o = os.path.join(BUILD, f[:-2] + ".o")
        r = subprocess.run([cc] + cflags + ["-c", os.path.join(SRC, f), "-o", o],
                           capture_output=True, text=True)
        if r.returncode != 0:
            bad += 1
            errs = [l for l in r.stderr.splitlines() if "error" in l]
            print("%-16s %d 错" % (f, len(errs)))
            for l in errs[:4 if not a.verbose else 999]:
                print("      " + l.strip()[:150])
        else:
            print("%-16s OK" % f)
            objs.append(o)
    if bad:
        print("\n%d/%d 个文件编译失败" % (bad, len(CORE)))
        return 1
    if a.compile_only:
        return 0
    out = os.path.join(BUILD, "tcc.elf")
    # **用 user_main_argv.o 而不是 user_main.o**：tcc 是第三方程序，假定标准 POSIX argv
    # 数组（逐个解析 argv[1..]）。而 BORUIX 原生 ABI 是 argc=1、argv[0]=整条命令行，
    # 直接用会让 tcc"看不到任何参数"（实测：静默退出 0、无输出）。
    # 该桥接把命令行拆成真 argv；见 csrc/user_main_argv.c 的说明。
    cmd = [lld, "-o", out, "-e", "_start", "-nostdlib", "--no-dynamic-linker",
           os.path.join(lib, "user_main_argv.o")] + objs + [
           os.path.join(lib, "libc.a"), "-z", "noexecstack", "-z", "norelro",
           "-T", os.path.join(lib, "linker.ld")]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        print("\n链接失败：")
        for l in r.stderr.splitlines()[:40]:
            print("  " + l.strip()[:150])
        return 1
    print("\n[build_tcc] " + out)
    return 0


if __name__ == "__main__":
    sys.exit(main())