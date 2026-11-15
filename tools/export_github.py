#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
export_github.py —— 把 galagino-pico 整理成两个可直接推送的目录

    gh-export/galagino-pico/          公开：源码 + 工具（不含任何 ROM 数据、不含固件二进制）
    gh-export/galagino-pico-assets/   私有：上面全部 + 资产头 + romregion.bin + 合并 UF2

为什么公开仓不能带数据：
  * 上游 galagino 是 GPLv3，其 NOTICE.md 明确写 “do not distribute firmware binaries”
    （Z80 core 的许可证禁止分发）。
  * 各机型的 *_rom/*_tilemap/*_spritemap/*_logo 头是 romconv 从商业 ROM 生成的数据，
    属于受版权保护的内容；上游本来就把这些头排除在源码库外。
所以公开仓只留「代码 + 转换工具」，别人 clone 后要自己准备 ROM 跑
convert.py -> plain_assets.py -> mkromregion.py 才能得到能烧的 UF2。

用法：
    python tools\\export_github.py
    python tools\\export_github.py --out D:\\gh-export
"""

import argparse
import json
import os
import re
import shutil
import sys
from fnmatch import fnmatch

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
DEFAULT_OUT = os.path.join(os.path.dirname(PROJECT), "gh-export")

# 顶层直接拷的文件
TOP_FILES = [
    "CMakeLists.txt",
    "pico_sdk_import.cmake",
    "memmap_pico16m.ld",
    "memmap_pico4m.ld",
    "run_studio.bat",
    "README.md",
]
# 整个目录拷（含子目录）
TOP_DIRS = ["cmake", "tools"]
# 从上游借来的许可文件
UPSTREAM_FILES = ["LICENSE", "NOTICE.md"]

# 明确要排除的目录/文件（各种 build 产物、本地备份、二进制）
SKIP_DIRS = {"build", "build-diag", "build-real", "build-safe", "build-selftest",
             ".git", "__pycache__", "dist", "rom"}
SKIP_FILES = {"patch_for_picoboy.py", "patch2.py", "patch3.py", "patch4.py", "patch5.py"}

HEX = re.compile(r"0x[0-9A-Fa-f]{2}")
DATA_THRESHOLD = 2000          # 一个头里出现 ≥2000 个 0xNN 就当它是数据文件

# 长得像数据、其实是代码的（Z80 的 opcode/flag 表），必须保留
KEEP_ALWAYS = {
    os.path.join("src", "cpus", "z80", "Tables.h"),
}
# 不管多大，一律当素材排除
ALWAYS_DATA_SUFFIX = ("_logo.h",)


def read_text(path):
    with open(path, "r", encoding="utf-8", newline="") as f:
        return f.read()


def write_text(path, text):
    with open(path, "wb") as f:
        f.write(text.encode("utf-8"))


def is_data_file(path, rel=None):
    """含大量十六进制字面量 = romconv 生成的数据（ROM/图形/logo），公开仓要剔除。"""
    if rel is not None and rel.replace("/", "\\") in KEEP_ALWAYS:
        return False
    name = os.path.basename(path).lower()
    if name.endswith(ALWAYS_DATA_SUFFIX):
        return True
    try:
        text = read_text(path)
    except Exception:
        return False
    return len(HEX.findall(text)) >= DATA_THRESHOLD


GITIGNORE = """\
# 构建产物
build*/
*.elf
*.elf.map
*.dis
*.uf2

# 资产区与明文快照（含商业 ROM 数据，不入库）
rom/romregion.bin
rom/plain_src/

# 工作室本地配置
tools/studio_config.json

# Python
__pycache__/

# 说明：各 src/machines/<game>/.gitignore 里仍然忽略“数据头”，
# 但改成 flash 直读后那些头里只剩指针（不含数据），导出时已把这类行去掉，
# 所以它们会正常入库；真正含数据的机型（如 1942）仍被那里的规则挡住。
"""

PUBLIC_NOTICE = """\
> **本仓库是公开发布版**：只有代码与转换工具，**不含任何 ROM 数据、不含固件二进制**。
>
> 原因有两条：一是上游 galagino 是 GPLv3，其 `NOTICE.md` 明确写了“Z80 core 的许可证禁止
> 分发、不要分发固件二进制”；二是各机型的 `*_rom.h` / `*_tilemap.h` / `*_spritemap.h` /
> `*_logo.h` 是 `romconv` 从商业街机 ROM 生成的数据，属于受版权保护的内容，上游本来就把它们
> 排除在源码库之外。
>
> 所以要得到能烧的固件，需要自己准备 ROM 包，然后按 README 里的流水线跑：
> `convert.py` → `plain_assets.py` → `mkromregion.py extract` → `ninja` → `mkromregion.py merge`。
> 图形化版本：`run_studio.bat`（`tools\\pico_arcade_studio.py`）。
"""


def rewrite_machine_gitignore(dirpath, dst_root):
    """
    机器目录里的 .gitignore 是上游用来挡住“romconv 生成的数据头”的。
    改成 flash 直读后这些头只剩指针（不含数据），必须入库；
    所以：某条规则匹配到的现存文件全都不是数据 -> 删掉这条规则；
    匹配不到任何文件（数据已被剔除）或匹配到的还是数据 -> 原样保留。
    """
    gi = os.path.join(dirpath, ".gitignore")
    if not os.path.isfile(gi):
        return 0
    keep = []
    removed = 0
    for line in read_text(gi).splitlines():
        s = line.strip()
        if not s or s.startswith("#") or s.startswith("!"):
            keep.append(line)
            continue
        files = [f for f in os.listdir(dirpath)
                 if fnmatch(f, s) and os.path.isfile(os.path.join(dirpath, f))]
        if not files:
            keep.append(line)                 # 数据文件已被剔除，规则留着防以后误提交
            continue
        hit_data = False
        for f in files:
            full = os.path.join(dirpath, f)
            rel = os.path.relpath(full, dst_root)
            if is_data_file(full, rel):
                hit_data = True
                break
        if hit_data:
            keep.append(line)
        else:
            removed += 1                      # 全是指针/配置，这条忽略会丢掉源码
    if removed:
        write_text(gi, "\n".join(keep).rstrip("\n") + "\n")
    return removed


def copy_file(src, dst):
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    shutil.copy2(src, dst)


def build_public(out_root, log):
    dst_root = os.path.join(out_root, "galagino-pico")
    if os.path.isdir(dst_root):
        shutil.rmtree(dst_root)
    os.makedirs(dst_root)

    for name in TOP_FILES:
        src = os.path.join(PROJECT, name)
        if os.path.isfile(src):
            copy_file(src, os.path.join(dst_root, name))
        else:
            log("  (缺) %s" % name)

    for name in TOP_DIRS:
        sdir = os.path.join(PROJECT, name)
        ddir = os.path.join(dst_root, name)
        os.makedirs(ddir, exist_ok=True)
        for dp, _dn, fn in os.walk(sdir):
            for f in fn:
                if f in SKIP_FILES or f.endswith((".pyc", ".bak")):
                    continue
                sp = os.path.join(dp, f)
                rel = os.path.relpath(sp, sdir)
                copy_file(sp, os.path.join(ddir, rel))

    # src：整棵树，但剔除数据文件
    sdir = os.path.join(PROJECT, "src")
    ddir = os.path.join(dst_root, "src")
    os.makedirs(ddir, exist_ok=True)
    kept = dropped = 0
    dropped_list = []
    for dp, dn, fn in os.walk(sdir):
        dn[:] = [d for d in dn if d not in SKIP_DIRS]
        for f in fn:
            sp = os.path.join(dp, f)
            if f in SKIP_FILES:
                continue
            if f.lower().endswith((".h", ".hpp", ".c", ".cpp")) and \
                    is_data_file(sp, os.path.relpath(sp, PROJECT)):
                dropped += 1
                dropped_list.append(os.path.relpath(sp, PROJECT))
                continue
            copy_file(sp, os.path.join(ddir, os.path.relpath(sp, sdir)))
            kept += 1
    log("  src：保留 %d 个源文件，剔除 %d 个数据头" % (kept, dropped))
    for rel in sorted(dropped_list):
        log("    - %s" % rel)

    # 许可
    up = os.path.join(os.path.dirname(PROJECT), "galagino-max")
    for name in UPSTREAM_FILES:
        sp = os.path.join(up, name)
        if os.path.isfile(sp):
            copy_file(sp, os.path.join(dst_root, name))
        else:
            log("  (缺) 上游 %s" % name)

    write_text(os.path.join(dst_root, ".gitignore"), GITIGNORE)

    # README 顶部插入发布说明
    rd = os.path.join(dst_root, "README.md")
    t = read_text(rd)
    if "本仓库是公开发布版" not in t:
        lines = t.split("\n")
        # 放在第一个二级标题之前，保留原来的标题行
        idx = 1
        for i, l in enumerate(lines):
            if l.startswith("## "):
                idx = i
                break
        lines[idx:idx] = PUBLIC_NOTICE.split("\n")
        write_text(rd, "\n".join(lines))
    # 机器目录里的 .gitignore：把“已变成指针”的数据头放行
    gir = 0
    for dp, _dn, fn in os.walk(ddir):
        if ".gitignore" in fn:
            gir += rewrite_machine_gitignore(dp, dst_root)
    log("  .gitignore：放行 %d 条已不含数据的忽略规则" % gir)

    log("  公开树：%s" % dst_root)
    return dst_root, dropped_list


def build_private(out_root, public_root, log):
    dst_root = os.path.join(out_root, "galagino-pico-assets")
    if os.path.isdir(dst_root):
        shutil.rmtree(dst_root)
    shutil.copytree(public_root, dst_root)

    # 完整 src（含数据头）
    sdir = os.path.join(PROJECT, "src")
    ddir = os.path.join(dst_root, "src")
    shutil.rmtree(ddir, ignore_errors=True)
    shutil.copytree(sdir, ddir)
    # 私有仓要把所有头都收进来：这类 .gitignore 全部清掉
    killed = 0
    for dp, _dn, fn in os.walk(ddir):
        if ".gitignore" in fn:
            os.remove(os.path.join(dp, ".gitignore"))
            killed += 1
    log("  私有树：清掉 %d 个机器目录 .gitignore（数据头全部入库）" % killed)

    # 资产区 / 明文快照 / 合并固件
    for rel in ("rom/romregion.bin", "rom/romregion.json"):
        sp = os.path.join(PROJECT, rel)
        if os.path.isfile(sp):
            copy_file(sp, os.path.join(dst_root, rel))
            log("  + %s (%.2f MB)" % (rel, os.path.getsize(sp) / 1048576))
    pdir = os.path.join(PROJECT, "rom", "plain_src")
    if os.path.isdir(pdir):
        shutil.copytree(pdir, os.path.join(dst_root, "rom", "plain_src"))
        log("  + rom/plain_src (%d 款)"
            % len([d for d in os.listdir(pdir) if os.path.isdir(os.path.join(pdir, d))]))
    for rel in ("dist/galagino_16m.uf2",):
        sp = os.path.join(PROJECT, rel)
        if os.path.isfile(sp):
            copy_file(sp, os.path.join(dst_root, rel))
            log("  + %s (%.2f MB)" % (rel, os.path.getsize(sp) / 1048576))

    # 私有库的 .gitignore 放行这些数据
    write_text(os.path.join(dst_root, ".gitignore"),
               "# 私有仓：这里刻意放行资产与固件\n"
               "build*/\n__pycache__/\ntools/studio_config.json\n")

    # 顶部加“这是私有完整包”的说明
    rd = os.path.join(dst_root, "README.md")
    t = read_text(rd)
    t = t.replace(PUBLIC_NOTICE, "")
    head = ("> **私有完整包**：和公开仓同源，另外带 `rom/romregion.bin`、`rom/plain_src/` 与\n"
            "> `dist/galagino_16m.uf2`。其中的 ROM 数据是给本机自用/复现的，上游 `NOTICE.md`\n"
            "> 写明不要分发固件二进制——**不要把这个仓库转成 public**。\n")
    lines = t.split("\n")
    idx = 1
    for i, l in enumerate(lines):
        if l.startswith("## "):
            idx = i
            break
    lines[idx:idx] = head.split("\n")
    write_text(rd, "\n".join(lines))
    log("  私有树：%s" % dst_root)
    return dst_root


def dir_stats(root):
    n = total = 0
    for dp, _dn, fn in os.walk(root):
        for f in fn:
            sp = os.path.join(dp, f)
            if os.path.isfile(sp):
                n += 1
                total += os.path.getsize(sp)
    return n, total


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=DEFAULT_OUT)
    args = ap.parse_args()

    def log(m):
        print(m)

    os.makedirs(args.out, exist_ok=True)
    print("导出到：%s" % args.out)
    public_root, dropped = build_public(args.out, log)
    n, total = dir_stats(public_root)
    print("  公开：%d 个文件 / %.2f MB" % (n, total / 1048576))

    private_root = build_private(args.out, public_root, log)
    n, total = dir_stats(private_root)
    print("  私有：%d 个文件 / %.2f MB" % (n, total / 1048576))
    print("EXPORT_OK")
    return 0


if __name__ == "__main__":
    sys.exit(main() or 0)
