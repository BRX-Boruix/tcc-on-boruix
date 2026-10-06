#!/usr/bin/env python3
"""把 tcc 在 BORUIX 内运行所需的**资产**铺到数据盘目录（阶段 6 / 3P6-1）。

产出（默认落到 `tools/diskfiles/3p/tcc/`）：
  include/    目标头文件 = tcc 自带的编译器支持头 + 我们 libc 的 C 库头
  crt1.o      入口桥接（= sysroot 的 user_main_argv.o：提供 user_main、引用 main）
  crti.o / crtn.o   空对象（常规 POSIX 放 .init/.fini 前后缀；本系统无该机制）
  libc.a      我们的 C 库（含 libsys，入口 _start 就在里面）

为什么 crt1.o 用 `user_main_argv.o` 而不是 `user_main.o`：tcc 是第三方程序，它**编译出来的**
程序同样假定标准 POSIX argv。见 csrc/user_main_argv.c 的说明。

**这不是"安装脚本"，是移植期的铺料脚本**：等 BORUIX 有了真正的 `/usr`，这里换成装到 `/usr` 即可。

用法: python boruix/stage_assets.py --sysroot <sysroot> [--dest <目录>]
"""
import argparse
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.dirname(HERE)                 # tcc 上游源码树根
ROOT = os.path.dirname(os.path.dirname(SRC))  # 工作区根
DEFAULT_DEST = os.path.join(ROOT, "tools", "diskfiles", "3p", "tcc")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sysroot", default=os.environ.get("BORUIX_SYSROOT"))
    ap.add_argument("--dest", default=DEFAULT_DEST)
    a = ap.parse_args()
    if not a.sysroot or not os.path.isdir(a.sysroot):
        sys.exit("需要 --sysroot（或 BORUIX_SYSROOT）")
    inc_src = os.path.join(a.sysroot, "include")
    lib_src = os.path.join(a.sysroot, "lib")
    dest = a.dest
    inc = os.path.join(dest, "include")
    os.makedirs(inc, exist_ok=True)

    # 1) 头文件：tcc 自带的（stdarg/stddef/stdbool/float/... 属编译器支持）先铺，
    #    再用我们 libc 的头覆盖同名者（C 库头应当以我们的为准）。
    n = 0
    for d, src in ((inc, os.path.join(SRC, "include")), (inc, inc_src)):
        for root, _dirs, files in os.walk(src):
            rel = os.path.relpath(root, src)
            outdir = os.path.join(d, rel) if rel != "." else d
            os.makedirs(outdir, exist_ok=True)
            for f in files:
                shutil.copy(os.path.join(root, f), os.path.join(outdir, f))
                n += 1
    print("[stage] 头文件 %d 个 -> %s" % (n, inc))

    # 2) crt1.o = 入口桥接（POSIX argv 版）。
    shutil.copy(os.path.join(lib_src, "user_main_argv.o"), os.path.join(dest, "crt1.o"))
    print("[stage] crt1.o <- user_main_argv.o")

    # 3) crti.o / crtn.o = 空对象。用 sysroot 的 C 驱动同款目标编译，保证 ABI 一致。
    cc = os.environ.get("BORUIX_CLANG") or shutil.which("clang")
    if not cc:
        sys.exit("找不到 clang（可用 BORUIX_CLANG 指定）")
    empty = os.path.join(dest, "_empty.c")
    with open(empty, "w", encoding="ascii", newline="\n") as f:
        f.write("/* empty: BORUIX 无 .init/.fini 前后缀机制 */\n")
    for name in ("crti.o", "crtn.o"):
        r = subprocess.run([cc, "--target=x86_64-unknown-none", "-ffreestanding",
                            "-fno-pic", "-c", empty, "-o", os.path.join(dest, name)])
        if r.returncode != 0:
            sys.exit("生成 %s 失败" % name)
        print("[stage] %s（空对象）" % name)
    os.remove(empty)

    # 4) libc.a
    shutil.copy(os.path.join(lib_src, "libc.a"), os.path.join(dest, "libc.a"))
    print("[stage] libc.a <- %s" % os.path.join(lib_src, "libc.a"))
    return 0


if __name__ == "__main__":
    sys.exit(main())