#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
make_devkit.py —— 生成“换台电脑也能继续开发”的自包含开发包

    out\\galagino-pico-devkit\\
        galagino-pico\\        工程本体（src/tools/cmake/rom/dist + 自带 bsp/）
        galagino-max\\         上游 romconv（只留转换需要的部分）
        SETUP.md               新机器从零跑起来的三步

打包时顺手解决两个外部依赖：
  1. CMakeLists 里的 PB_BSP_DIR 默认 E:\\PicoBoy\\bsp —— 包里复制一份到
     galagino-pico\\bsp，CMakeLists 找不到原路径时会自动回落（已加逻辑）。
  2. tools/*.py 里的 upstream 默认是 PROJECT 的上一级 galagino-max —— 把
     romconv 放到 out\\galagino-max\\romconv 后正好对上，零配置。

用法：
    python tools\\make_devkit.py
    python tools\\make_devkit.py --out D:\\devkit --zip
"""

import argparse
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
WORKSPACE = os.path.dirname(PROJECT)
UPSTREAM = os.path.join(WORKSPACE, "galagino-max")
BSP_SRC = r"E:\PicoBoy\bsp"
DEFAULT_OUT = os.path.join(WORKSPACE, "devkit")

# 工程里要带的顶层文件
TOP_FILES = [
    "CMakeLists.txt",
    "pico_sdk_import.cmake",
    "memmap_pico16m.ld",
    "memmap_pico4m.ld",
    "build.ps1",
    "run_studio.bat",
    "README.md",
]
# 整目录带（rom 里的资产、src 里的全部源码）
TOP_DIRS = ["src", "tools", "cmake", "rom"]
# 只带这些产物，其余是历史探针 uf2
DIST_KEEP = [
    "galagino_16m.uf2",
    "galagino_16m_prev.uf2",
]
BUILD_KEEP = ["galagino_arcade.bin", "galagino_arcade.uf2"]
SKIP_NAMES = {"__pycache__", "studio_config.json"}

SETUP = """\
# galagino-pico 开发包（换电脑继续开发用）

这个包里已经自带了两样平时散落在外面的东西，所以拿到手不用先配环境变量：

| 依赖 | 平时在哪 | 包里在哪 |
|---|---|---|
| 板级支持包 PicoBoy-BSP | `E:\\PicoBoy\\bsp` | `galagino-pico\\bsp`（`CMakeLists.txt` 找不到原路径会自动回落） |
| 上游 romconv（ROM 转换） | `…\\galagino-max\\romconv` | `galagino-max\\romconv`（`tools/*.py` 的默认 `../galagino-max` 正好对上） |

## 一、新机器要装的（只装一次）

1. **Pico SDK 1.5.1**（含 `cmake` / `ninja` / `gcc-arm-none-eabi`）。
   官方安装器会把它们放在 `C:\\Program Files\\Raspberry Pi\\Pico SDK v1.5.1\\`，
   `build.ps1` 自己会去这儿找；装在别处就设环境变量 `PICO_SDK_PATH` 指向 `…\\pico-sdk`。
   ⚠️ 必须带 `arm-none-eabi-gcc`，只有 SDK 源码编不了。
2. **Python 3.10+，且带 tkinter**（`python -c "import tkinter"` 不能报错）。
   Windows 的 Microsoft Store 版 python 有时缺 Tcl/Tk，装 python.org 的安装包最稳。
3. 可选：`py7zr`（只有你要喂 `.7z` 的 ROM 包时才需要，`.zip` 不需要）。
   ```
   python -m pip install py7zr
   ```

## 二、三条命令

```powershell
# 1) 编固件（首次会自动 cmake 配置，产物在 build\\ 与 dist\\）
.\\galagino-pico\\build.ps1

# 2) 图形化工作台：ROM 转换 / 编译 / 合并 / 烧写
.\\galagino-pico\\run_studio.bat

# 3) 合并固件 + 资产区 -> 一个能直接拖进 BOOTSEL 盘的 UF2
cd galagino-pico
python tools\\mkromregion.py merge     # 产物 dist\\galagino_16m.uf2
```

烧录：按住 BOOTSEL 插 USB，把 `dist\\galagino_16m.uf2` 拖进出现的 `RPI-RP2` 盘。

## 三、包里已经有的东西

| 路径 | 说明 |
|---|---|
| `galagino-pico\\src` | 全部源码。12 款机型的资产头已经是指针形态（`GAL_ROM_BASE + offset`），不含数据 |
| `galagino-pico\\rom\\romregion.bin` | 资产区真身，3,145,084 B / 100 个资产，烧到 flash 偏移 `0x120000` |
| `galagino-pico\\rom\\romregion.json` | 资产偏移表（name / file / type / offset / bytes） |
| `galagino-pico\\rom\\plain_src` | 12 款明文头快照，`mkromregion.py extract` 的恢复源 |
| `galagino-pico\\dist\\galagino_16m.uf2` | 十二款合并 UF2（6,741,504 B / 13,167 块），烧完即可玩 |
| `galagino-pico\\dist\\galagino_16m_prev.uf2` | 上一版（十一款），回滚用 |
| `galagino-pico\\bsp` | PicoBoy-BSP 副本（`pb_display_push_rect` 等直推接口就在这儿） |
| `galagino-max\\romconv` | 上游转换器：`convert.py` + 83 个机型的 job 表 |
| `galagino-max\\romszip\\Z80-081707.zip` | Z80 核心补丁包（只有新增机型时才用得上） |

## 四、加新机型（可选）

1. 把 ROM 包（`.zip` / `.7z`）放进 `galagino-max\\romszip\\`。
2. 打开 `galagino-pico\\run_studio.bat`，① ROM 转换页签：选机型 → **校验 SHA-1** → **一键集成**。
   它会跑 `convert.py -j1` → 回灌明文 → 拷机器源 → `plain_assets` → 修 `Logo` 的 `constexpr`
   → 三处 `MACHINES` / `add_game` 同步 → `extract`。
3. ② 固件编译 → ③ 合并 & 烧写。

命令行等价流程见 `galagino-pico\\README.md` 的“生成与烧写流水线”。

## 五、注意

- 这个包里有**商业街机 ROM 数据**（`romregion.bin`、`plain_src`、合并 UF2），
  只能自用，**不要上传到公开仓库或公开分享**。
- 代码是上游 galagino 的衍生物，GPLv3（见 `galagino-pico\\LICENSE`、`NOTICE.md`）。
  上游 `NOTICE.md` 明确写了 Z80 核心的许可证禁止分发、不要分发固件二进制。
- `build-diag` / `build-real` 这类目录没带：里面的 CMake 缓存写死了旧机器的绝对路径，
  换机器必须重新配置，带了也用不上。
"""


def copy_tree(src, dst, skip_names=()):
    for dp, dn, fn in os.walk(src):
        dn[:] = [d for d in dn if d not in SKIP_NAMES and d not in skip_names]
        for f in fn:
            if f in SKIP_NAMES or f.endswith(".pyc"):
                continue
            sp = os.path.join(dp, f)
            dp2 = os.path.join(dst, os.path.relpath(sp, src))
            os.makedirs(os.path.dirname(dp2), exist_ok=True)
            shutil.copy2(sp, dp2)


def clean(path):
    if os.path.isdir(path):
        shutil.rmtree(path, ignore_errors=True)


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
    ap.add_argument("--zip", action="store_true", help="顺便压成 zip")
    args = ap.parse_args()

    kit = os.path.join(args.out, "galagino-pico-devkit")
    clean(kit)
    os.makedirs(kit)
    proj_dst = os.path.join(kit, "galagino-pico")
    os.makedirs(proj_dst)

    print("生成开发包：%s" % kit)

    for name in TOP_FILES:
        src = os.path.join(PROJECT, name)
        if os.path.isfile(src):
            shutil.copy2(src, os.path.join(proj_dst, name))
        else:
            print("  (缺) %s" % name)

    for name in TOP_DIRS:
        src = os.path.join(PROJECT, name)
        if os.path.isdir(src):
            copy_tree(src, os.path.join(proj_dst, name))
        else:
            print("  (缺) %s/" % name)

    # dist：只留合并 UF2 与回滚版
    dist_dst = os.path.join(proj_dst, "dist")
    os.makedirs(dist_dst, exist_ok=True)
    for name in DIST_KEEP:
        src = os.path.join(PROJECT, "dist", name)
        if os.path.isfile(src):
            shutil.copy2(src, os.path.join(dist_dst, name))
    # 单独固件（不含资产区，只更新固件时烧这个不会擦资产）
    for name in BUILD_KEEP:
        src = os.path.join(PROJECT, "build-diag", name)
        if os.path.isfile(src):
            shutil.copy2(src, os.path.join(dist_dst, name))

    # 自带 bsp（CMakeLists 找 E:\PicoBoy\bsp 失败时的回落目标）
    if os.path.isdir(BSP_SRC):
        copy_tree(BSP_SRC, os.path.join(proj_dst, "bsp"))
        print("  + bsp/（PicoBoy-BSP 副本）")
    else:
        print("  !! 找不到 %s，包里不会有 bsp，编译会失败" % BSP_SRC)

    # 上游 romconv
    up_dst = os.path.join(kit, "galagino-max")
    if os.path.isdir(os.path.join(UPSTREAM, "romconv")):
        copy_tree(os.path.join(UPSTREAM, "romconv"), os.path.join(up_dst, "romconv"))
        print("  + galagino-max/romconv/")
    else:
        print("  !! 找不到上游 romconv")
    for name in ("LICENSE", "NOTICE.md"):
        src = os.path.join(UPSTREAM, name)
        if os.path.isfile(src):
            shutil.copy2(src, os.path.join(up_dst, name))
    z80 = os.path.join(UPSTREAM, "romszip", "Z80-081707.zip")
    if os.path.isfile(z80):
        os.makedirs(os.path.join(up_dst, "romszip"), exist_ok=True)
        shutil.copy2(z80, os.path.join(up_dst, "romszip", "Z80-081707.zip"))
        print("  + galagino-max/romszip/Z80-081707.zip")

    with open(os.path.join(kit, "SETUP.md"), "wb") as f:
        f.write(SETUP.encode("utf-8-sig"))       # 带 BOM：Windows 记事本也不乱码

    n, total = dir_stats(kit)
    print("  包内容：%d 个文件 / %.2f MB" % (n, total / 1048576))

    if args.zip:
        zpath = shutil.make_archive(kit, "zip", os.path.dirname(kit),
                                    os.path.basename(kit))
        print("  zip：%s（%.2f MB）" % (zpath, os.path.getsize(zpath) / 1048576))
    print("DEVKIT_OK")
    return 0


if __name__ == "__main__":
    sys.exit(main() or 0)
