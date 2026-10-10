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
ROOT = os.path.dirname(SRC)                   # 工作区根（SRC 是 tcc-on-boruix/）
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

    # 0) **前置检查（先于任何写入）**：本脚本就地覆盖 `dest/` 里的资产，**中途失败会留下
    #    「半更新」状态**——头文件已是新的、`libc.a` 还是旧的。那正是「改了 libc 却验了旧库」
    #    这类幽灵故障的来源（2026-10 实测：缺 `BORUIX_CLANG` 时脚本在第 3 步 `sys.exit`，
    #    而 `libc.a` 的复制在第 4 步 ⇒ 盘上的库仍是旧的，而调用方只看到一行 stderr）。
    #    故**所有前置条件在这里一次查完**：缺任何一项就立刻退出，**一个字节都不写**。
    cc = os.environ.get("BORUIX_CLANG") or shutil.which("clang")
    if not cc:
        sys.exit("前置检查失败：找不到 clang（可用 BORUIX_CLANG 指定）——未写入任何文件")
    for rel in ("lib/libc.a", "lib/user_main_argv.o"):
        p = os.path.join(a.sysroot, *rel.split("/"))
        if not os.path.isfile(p):
            sys.exit("前置检查失败：sysroot 缺少 " + p + "——未写入任何文件")

    os.makedirs(inc, exist_ok=True)

    # 1) 头文件：tcc 自带的（stdarg/stddef/stdbool/float/... 属编译器支持）先铺，
    #    再用我们 libc 的头覆盖同名者（C 库头应当以我们的为准）。
    #    **跳过 sysroot 里的 `c++/`**（2026-10 实测的缺陷链）：那是 libstdc++ 的头，
    #    tcc 用不到；更糟的是它有 800+ 个文件，会把这个目录撑到需要**多块目录**，
    #    而 `tools/tools_build/disk.py` 的 EXT2 写入器当时只支持单块目录 ⇒ 建盘直接崩
    #    （`struct.error: pack_into requires a buffer of at least 1028 bytes`）。
    #    两处都修了：这里不铺，disk.py 也支持多块目录（不再依赖"目录一定很小"这个假设）。
    stale_cxx = os.path.join(inc, "c++")
    if os.path.isdir(stale_cxx):
        shutil.rmtree(stale_cxx)
        print("[stage] 清掉旧的 include/c++（tcc 用不到；曾撑爆单块目录）")
    n = 0
    for d, src in ((inc, os.path.join(SRC, "include")), (inc, inc_src)):
        for root, dirs, files in os.walk(src):
            if src == inc_src:
                dirs[:] = [x for x in dirs if x != "c++"]
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
    #    `cc` 已在第 0 步查过（提前到那里正是为了「失败不留半更新」）。
    empty = os.path.join(dest, "_empty.c")
    with open(empty, "w", encoding="utf-8", newline="\n") as f:
        f.write("/* empty: BORUIX 无 .init/.fini 前后缀机制 */\n")
    for name in ("crti.o", "crtn.o"):
        # -fno-addrsig：clang 默认给对象加 .llvm_addrsig 节（非标准节类型 LLVM_ADDRSIG），
        # tcc 的 ELF 读取器未必认——它读归档/对象时崩（实测 SIGSEGV）的首要嫌疑。
        r = subprocess.run([cc, "--target=x86_64-unknown-none", "-ffreestanding",
                            "-fno-pic", "-fno-addrsig", "-c", empty,
                            "-o", os.path.join(dest, name)])
        if r.returncode != 0:
            sys.exit("生成 %s 失败" % name)
        print("[stage] %s（空对象）" % name)
    os.remove(empty)

    # 4) libc.a
    shutil.copy(os.path.join(lib_src, "libc.a"), os.path.join(dest, "libc.a"))
    print("[stage] libc.a <- %s" % os.path.join(lib_src, "libc.a"))

    # 4b) libtcc1.a（编译器支持例程）：由 boruix/build_libtcc1.py 产出。
    #     没有它时 tcc 链接会因为缺少 64 位除法/软浮点等例程而失败。
    libtcc1 = os.path.join(SRC, "_build", "libtcc1.a")
    if os.path.isfile(libtcc1):
        shutil.copy(libtcc1, os.path.join(dest, "libtcc1.a"))
        print("[stage] libtcc1.a <- %s" % libtcc1)
    else:
        print("[stage] 警告：未找到 libtcc1.a，先跑 boruix/build_libtcc1.py")

    # 5) 验收/驱动夹具：从**正本**（tests/*.c）拷到磁盘。
    #    为什么要脚本拷而不是手工写：手工经多层引号/转义拼源码出过事——
    #    `\n` 在某层被解释成真换行，导致 C 字符串字面量断行（tcc 报
    #    `missing terminating " character`）。正本只有一份，拷贝没有转义问题。
    disk_root = os.path.dirname(dest)          # diskfiles/3p

    # 5a) **tcc 本体**（tcc.elf）。此前**没有任何脚本**把它铺到盘上——它一直是
    #     手工拷过去的，于是「改了 tccelf.c 却测了旧 tcc」成了**真实事故**
    #     （2026-10 实测）：一次 A/B 的两臂跑的是同一份旧二进制，红绿两臂的
    #     5.8s/6.6s 全是噪声，加进去的计时仪器**一行都没被执行到**
    #     （仪器标记 TCCPROF 在两臂的串口日志里都没出现，才发现盘上不是新 tcc）。
    #     tcc.elf 由 boruix/build.py 产出到 _build/，本脚本负责**铺到盘上**。
    #     **缺失即报错**：宁可停下，也不留一份旧编译器在盘上冒充新构建
    #     （与上面 libc.a 的「不半更新」同一条理由）。
    tcc_elf = os.path.join(SRC, "_build", "tcc.elf")
    if not os.path.isfile(tcc_elf):
        sys.exit("前置检查失败：未找到 " + tcc_elf + "——先跑 boruix/build.py")
    shutil.copy(tcc_elf, os.path.join(disk_root, "tcc.elf"))
    print("[stage] tcc.elf <- %s" % tcc_elf)

    # 5b) **libtcc 能力探针**（tccmt.elf）。同一条理由：探针要证明的是「真实 libtcc
    #     在当前配置下无互斥」，若盘上留着旧探针，验的就不是当前构建——那正是
    #     tcc.elf 那次「两臂跑同一份旧二进制」事故的同一形态。故**缺失即报错**。
    probe_elf = os.path.join(SRC, "_build", "tccmt.elf")
    if not os.path.isfile(probe_elf):
        sys.exit("前置检查失败：未找到 " + probe_elf
                 + "——先跑 boruix/build.py --probe tccmt")
    shutil.copy(probe_elf, os.path.join(disk_root, "tccmt.elf"))
    print("[stage] tccmt.elf <- %s" % probe_elf)

    for name in ("hello.c", "wave2.c", "forkmin.c"):
        src = os.path.join(SRC, "tests", name)
        if os.path.isfile(src):
            shutil.copy(src, os.path.join(disk_root, name))
            print("[stage] %s <- tests/%s" % (name, name))
        else:
            print("[stage] 警告：未找到 tests/%s" % name)
    return 0


if __name__ == "__main__":
    sys.exit(main())