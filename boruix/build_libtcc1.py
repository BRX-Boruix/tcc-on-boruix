#!/usr/bin/env python3
"""构建 tcc 的运行时支持库 `libtcc1.a`（阶段 6 / 3P6-1）。

x86_64 的对象集合取自上游 `lib/Makefile`：

    OBJ-x86_64 = libtcc1.o + COMMON_O + va_list.o + LIN_O
    COMMON_O   = stdatomic.o atomic.o builtin.o alloca.o alloca-bt.o
    LIN_O      = dsohandle.o

**不含** runmain/tcov/bt-exe/bt-log/bcheck/bcheck_run——上游只把它们加进 native 编译器的
libtcc1.a（回溯与边界检查），而本移植已如实关闭那些功能（见 boruix/config.h 的 CONFIG_TCC_BORUIX）。

为什么可以用 clang 编：这些文件是**目标侧的运行时例程**（64 位除法、软浮点辅助、原子操作等），
只要目标与调用约定一致即可——不必由 tcc 自己编（那会陷入先有鸡还是先有蛋）。

用法: python boruix/build_libtcc1.py [--out <目录>]
工具链经 BORUIX_CLANG / BORUIX_LLD 覆盖，默认按 PATH 查找（S01）。
"""
import argparse
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.dirname(HERE)          # tcc 上游源码树根
LIBDIR = os.path.join(SRC, "lib")

# (源文件, 目标文件)
OBJS = [
    ("libtcc1.c", "libtcc1.o"),
    ("stdatomic.c", "stdatomic.o"),
    ("atomic.S", "atomic.o"),
    ("builtin.c", "builtin.o"),
    ("alloca.S", "alloca.o"),
    ("alloca-bt.S", "alloca-bt.o"),
    ("va_list.c", "va_list.o"),
    ("dsohandle.c", "dsohandle.o"),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(SRC, "_build"))
    a = ap.parse_args()
    cc = os.environ.get("BORUIX_CLANG") or shutil.which("clang")
    ar = os.environ.get("BORUIX_AR") or shutil.which("llvm-ar") or shutil.which("ar")
    if not cc or not ar:
        sys.exit("需要 clang 与 ar（可用 BORUIX_CLANG / BORUIX_AR 指定）")
    os.makedirs(a.out, exist_ok=True)
    # -fno-addrsig：去掉 clang 默认加的 .llvm_addrsig 节（非标准节类型 LLVM_ADDRSIG），
    # tcc 的 ELF 读取器未必认它。
    flags = ["--target=x86_64-unknown-none", "-ffreestanding", "-fno-builtin",
             "-fno-stack-protector", "-fno-pic", "-O2", "-fno-addrsig",
             "-I", LIBDIR]
    built = []
    for src, obj in OBJS:
        s = os.path.join(LIBDIR, src)
        o = os.path.join(a.out, obj)
        r = subprocess.run([cc] + flags + ["-c", s, "-o", o], capture_output=True, text=True)
        if r.returncode != 0:
            print("%-14s 失败：" % src)
            for l in r.stderr.splitlines()[:6]:
                print("    " + l.strip()[:140])
            return 1
        print("%-14s OK" % src)
        built.append(o)
    out = os.path.join(a.out, "libtcc1.a")
    if os.path.exists(out):
        os.remove(out)
    r = subprocess.run([ar, "rcs", out] + built, capture_output=True, text=True)
    if r.returncode != 0:
        print("打包失败：" + r.stderr[:300])
        return 1
    print("[build_libtcc1] %s (%d 个对象, %d KB)" % (out, len(built), os.path.getsize(out) // 1024))
    return 0


if __name__ == "__main__":
    sys.exit(main())