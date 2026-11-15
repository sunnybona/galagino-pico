#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
pico_arcade_studio.py —— galagino-pico 街机移植工作台（Tk GUI）

形态参考 E:\\PicoBoy\\rom_uploader.py：纯标准库、纯逻辑函数能单测、GUI 只负责编排。
区别在于本项目的 ROM 不是运行时 tar，而是**编译期**资产：
  1) romconv 把 zip 转成 C 头（每个机型一个 job）
  2) plain_assets.py 把 LZMA 压缩 blob 展开成明文数组（XIP 直读，不占 Arena）
  3) mkromregion.py extract 把全部明文数组抽进 rom/romregion.bin（0x10120000 起）
  4) ninja 编固件；mkromregion.py merge 把固件 UF2 和资产区拼成一个文件
  5) 拖进 RPI-RP2 盘或 picotool load

依赖：只用 Python 标准库（tkinter + subprocess + zipfile）。可选的 py7zr 只用于 .7z 包。
     picotool 为可选，只有“picotool 上传”需要。

用法：
    python tools\\pico_arcade_studio.py
    python tools\\pico_arcade_studio.py --selftest      # 不开窗口，只跑纯逻辑
    python tools\\pico_arcade_studio.py --smoke out.txt # 开真窗口、量控件、写报告后退出
"""

import json
import os
import queue
import re
import shutil
import string
import struct
import subprocess
import sys
import tempfile
import threading
import time
import zipfile

import tkinter as tk
from tkinter import ttk, filedialog, messagebox

# ---- 和固件 / 脚本里的常量对齐 -----------------------------------------
HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
CONFIG_PATH = os.path.join(HERE, "studio_config.json")

GAL_ROM_BASE = 0x10120000          # CMakeLists.txt 里的 GAL_ROM_BASE
FW_LIMIT = 786432                  # cmake/check_fw_size.cmake 的上限
BUILD_DIR = os.path.join(PROJECT, "build-diag")
FW_BIN = os.path.join(BUILD_DIR, "galagino_arcade.bin")
FW_UF2 = os.path.join(BUILD_DIR, "galagino_arcade.uf2")
DIST_UF2 = os.path.join(PROJECT, "dist", "galagino_16m.uf2")
REGION_BIN = os.path.join(PROJECT, "rom", "romregion.bin")
REGION_JSON = os.path.join(PROJECT, "rom", "romregion.json")
PLAIN_SRC = os.path.join(PROJECT, "rom", "plain_src")
MACHINES_DIR = os.path.join(PROJECT, "src", "machines")
TOOLS = os.path.join(PROJECT, "tools")

UF2_FAMILY_RP2040 = 0xE48BFF56
UF2_MAGIC0 = 0x0A324655
UF2_MAGIC1 = 0x9E5D5157
UF2_MAGIC_END = 0x0AB16F30

# =====================================================================
#  基础工具
# =====================================================================
def read_text(path):
    with open(path, "r", encoding="utf-8", newline="") as f:
        return f.read()


def write_text(path, text):
    with open(path, "wb") as f:
        f.write(text.encode("utf-8"))


def backup(path):
    """改工程文件前留一份 .studio.bak（只保留最近一次，够回滚用）。"""
    if os.path.isfile(path):
        shutil.copy2(path, path + ".studio.bak")


def load_config():
    cfg = {
        "project": PROJECT,
        "upstream": os.path.join(os.path.dirname(PROJECT), "galagino-max"),
        "sdk": guess_sdk(),
        # 板级支持包：留空就用 CMakeLists 的默认（E:\PicoBoy\bsp，找不到会回落到工程自带 bsp/）
        "bsp": "",
        "jobs": "2",
    }
    if os.path.isfile(CONFIG_PATH):
        try:
            cfg.update(json.loads(read_text(CONFIG_PATH)))
        except Exception:
            pass
    return cfg


def save_config(cfg):
    write_text(CONFIG_PATH, json.dumps(cfg, ensure_ascii=False, indent=2))


def guess_sdk():
    for c in (r"E:\Program Files\Raspberry Pi\Pico SDK v1.5.1\pico-sdk",
              r"C:\Program Files\Raspberry Pi\Pico SDK v1.5.1\pico-sdk",
              r"C:\Program Files (x86)\Raspberry Pi\Pico SDK v1.5.1\pico-sdk"):
        if os.path.isdir(c):
            return c
    env = os.environ.get("PICO_SDK_PATH", "")
    return env if os.path.isdir(env) else ""


def sdk_tools(sdk):
    """从 SDK 目录推出 (cmake, ninja, gcc-bin)。SDK 的上一级是 “Pico SDK v1.5.1”。"""
    root = os.path.dirname((sdk or "").rstrip("\\/")) if sdk else ""
    cmake = os.path.join(root, "cmake", "bin", "cmake.exe")
    ninja = os.path.join(root, "ninja", "ninja.exe")
    gcc = os.path.join(root, "gcc-arm-none-eabi", "bin")
    if not os.path.isfile(cmake):
        cmake = shutil.which("cmake") or ""
    if not os.path.isfile(ninja):
        ninja = shutil.which("ninja") or ""
    if not os.path.isdir(gcc):
        gcc = ""
    return cmake, ninja, gcc


def tool_env(cfg):
    env = os.environ.copy()
    extra = []
    sdk = cfg.get("sdk", "")
    if sdk:
        env["PICO_SDK_PATH"] = sdk
        gcc = sdk_tools(sdk)[2]
        if gcc:
            extra.append(gcc)
    if sys.platform == "win32":
        # 链接期要调 cmd.exe，PATH 里必须有 System32
        extra += [r"C:\Windows\System32", r"C:\Windows", r"C:\Windows\System32\Wbem"]
    keep = [p for p in extra if os.path.isdir(p)]
    if keep:
        env["PATH"] = ";".join(keep + [env.get("PATH", "")])
    return env


def python_exe():
    return sys.executable if getattr(sys, "frozen", False) is False else "python"


def stream(cmd, cwd, log, cfg, holder=None):
    """跑子进程，逐行灌日志。holder 是个 list，用来存 Popen 以便中途 kill。"""
    kw = {}
    if sys.platform == "win32":
        kw["creationflags"] = 0x08000000          # CREATE_NO_WINDOW
    log("$ " + " ".join(os.path.basename(cmd[0]) if i == 0 else c for i, c in enumerate(cmd)))
    try:
        p = subprocess.Popen(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                             text=True, encoding="utf-8", errors="replace",
                             env=tool_env(cfg), **kw)
    except OSError as e:
        log("启动失败：%s" % e)
        return 1
    if holder is not None:
        holder[:] = [p]
    for line in p.stdout:
        line = line.rstrip()
        if line:
            log("  " + line)
    p.wait()
    if holder is not None:
        holder[:] = []
    return p.returncode


# =====================================================================
#  纯逻辑：job 扫描 / ROM 包校验
# =====================================================================
def scan_jobs(upstream):
    """扫上游 romconv 里所有 job（Job(name="x", ...)）。"""
    root = os.path.join(upstream, "romconv", "romconv", "games")
    jobs = {}
    if not os.path.isdir(root):
        return jobs
    for dp, _dn, fn in os.walk(root):
        for f in sorted(fn):
            if not f.endswith(".py") or f.startswith("__"):
                continue
            p = os.path.join(dp, f)
            try:
                t = read_text(p)
            except Exception:
                continue
            m = re.search(r'Job\(\s*name\s*=\s*"([^"]+)"', t)
            if m:
                jobs.setdefault(m.group(1), p)
    return jobs


def _paren_block(text, name):
    """取 `<name> = ( ... )` 括号内的原文（按括号深度扫，能容忍条目自己的括号）。"""
    m = re.search(r"(?m)^%s\s*=\s*\(" % re.escape(name), text)
    if not m:
        return ""
    start = m.end()
    depth, i = 1, start
    while i < len(text):
        c = text[i]
        if c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                return text[start:i]
        i += 1
    return text[start:]


def required_of(job_py):
    """job 里“本机型”的必需文件表。

    优先只取 `_REQUIRED = (...)` 那一段：有些 job 在同一个文件里还写了兄弟机型的
    表（xevious.py 里的 _SX_CPU1/_SX_CPU2 是 sxevious 的），全抓会把别人的 ROM 算进来。
    """
    t = read_text(job_py)
    scope = _paren_block(t, "_REQUIRED")
    if not scope:
        scope = t
    return re.findall(r'\(\s*"([^"]+)"\s*,\s*"([0-9a-fA-F]{40})"\s*\)', scope)


def archive_members(path):
    """{成员名: (sha1, size)}。支持 .zip 与 .7z（后者要 py7zr）。"""
    import hashlib
    out = {}
    low = path.lower()
    if low.endswith(".zip"):
        with zipfile.ZipFile(path) as z:
            for i in z.infolist():
                if i.is_dir():
                    continue
                out[i.filename] = (hashlib.sha1(z.read(i)).hexdigest(), i.file_size)
        return out
    if low.endswith(".7z"):
        try:
            import py7zr                            # 只在需要时导入
        except ImportError:
            raise RuntimeError(".7z 需要 py7zr：先跑  python -m pip install py7zr（或改用 .zip）")
        tmp = tempfile.mkdtemp(prefix="studio7z_")
        try:
            with py7zr.SevenZipFile(path) as z:
                z.extractall(tmp)
            for dp, _dn, fn in os.walk(tmp):
                for f in fn:
                    fp = os.path.join(dp, f)
                    data = open(fp, "rb").read()
                    name = os.path.relpath(fp, tmp).replace("\\", "/")
                    out[name] = (hashlib.sha1(data).hexdigest(), len(data))
        finally:
            shutil.rmtree(tmp, ignore_errors=True)
        return out
    raise RuntimeError("只支持 .zip / .7z")


def check_archive(path, reqs):
    """对照 job 的必需表逐个查 SHA-1。返回 (行列表, 包内成员数)。"""
    members = archive_members(path)
    by_hash = {}
    for name, (h, _s) in members.items():
        by_hash.setdefault(h, name)
    rows = []
    for fn, sha in reqs:
        hit = by_hash.get(sha.lower())
        rows.append((fn, sha[:12], "OK" if hit else "缺失/不符", hit or "-"))
    return rows, len(members)


def read_region():
    if not os.path.isfile(REGION_JSON):
        return []
    try:
        return json.loads(read_text(REGION_JSON))
    except Exception:
        return []


def verify_uf2(path):
    """校验块结构，返回 dict。给合并后自检用。"""
    size = os.path.getsize(path)
    n = size // 512
    blocks, nums, end = [], set(), 0
    with open(path, "rb") as f:
        for i in range(n):
            b = f.read(512)
            m0, m1, _fl, addr, psz, bno, nb, fam = struct.unpack("<8I", b[:32])
            if (m0, m1, fam) != (UF2_MAGIC0, UF2_MAGIC1, UF2_FAMILY_RP2040):
                raise ValueError("第 %d 块头魔数不对" % i)
            if struct.unpack("<I", b[508:512])[0] != UF2_MAGIC_END:
                raise ValueError("第 %d 块尾 magic 不对" % i)
            blocks.append(bno)
            nums.add(nb)
            end = max(end, addr + psz)
    return {
        "bytes": size,
        "blocks": n,
        "num_blocks": sorted(nums),
        "end": end,
        "contiguous": blocks == list(range(n)),
    }


def fw_size():
    return os.path.getsize(FW_BIN) if os.path.isfile(FW_BIN) else 0


def region_total():
    """(资产数, 资产区字节数)。字节数以 romregion.bin 的实际长度为准——
    json 里各项 bytes 之和比它小，差的是每个资产 4 字节对齐的填充。"""
    items = read_region()
    if os.path.isfile(REGION_BIN):
        return len(items), os.path.getsize(REGION_BIN)
    return len(items), sum(a.get("bytes", 0) for a in items)


# =====================================================================
#  纯逻辑：工程文件改写（全部“追加式”，改前留 .studio.bak）
# =====================================================================
def restore_all_plain(log):
    """把 rom/plain_src 里 12 款的明文头全量回灌到 src/machines（折 LF）。

    extract 是破坏性的：头里只剩指针，必须靠这一步把明文找回来。
    """
    if not os.path.isdir(PLAIN_SRC):
        raise RuntimeError("缺 rom/plain_src，无法回灌（先跑一次 plain_assets）")
    n = 0
    for d in sorted(os.listdir(PLAIN_SRC)):
        sdir = os.path.join(PLAIN_SRC, d)
        if not os.path.isdir(sdir):
            continue
        ddir = os.path.join(MACHINES_DIR, d)
        os.makedirs(ddir, exist_ok=True)
        for f in sorted(os.listdir(sdir)):
            sp = os.path.join(sdir, f)
            if not os.path.isfile(sp):
                continue
            data = open(sp, "rb").read()
            if b"\r\n" in data:
                data = data.replace(b"\r\n", b"\n")
            open(os.path.join(ddir, f), "wb").write(data)
            n += 1
    log("回灌 %d 个明文头（LF）" % n)


def snapshot_plain(game, log):
    dst = os.path.join(PLAIN_SRC, game)
    shutil.rmtree(dst, ignore_errors=True)
    shutil.copytree(os.path.join(MACHINES_DIR, game), dst)
    for f in os.listdir(dst):
        p = os.path.join(dst, f)
        if os.path.isfile(p):
            b = open(p, "rb").read()
            if b"\r\n" in b:
                open(p, "wb").write(b.replace(b"\r\n", b"\n"))
    log("明文快照 -> rom/plain_src/%s" % game)


def patch_logo(game, log):
    """romconv 原生写的是 static constexpr Logo，本工程要求 static const。"""
    hits = 0
    for base in (os.path.join(MACHINES_DIR, game), os.path.join(PLAIN_SRC, game)):
        p = os.path.join(base, "%s_logo.h" % game)
        if not os.path.isfile(p):
            continue
        t = read_text(p)
        if "static constexpr Logo" in t:
            write_text(p, t.replace("static constexpr Logo", "static const Logo"))
            hits += 1
    if hits:
        log("  修 logo：static constexpr -> static const（%d 处）" % hits)


def ensure_registration(game, log):
    """CMakeLists 的 add_game + 两个 tools 的 MACHINES 元组。全部幂等。"""
    changed = []
    cm = os.path.join(PROJECT, "CMakeLists.txt")
    t = read_text(cm)
    if not re.search(r"(?m)^add_game\(\s*%s\s*\)" % re.escape(game), t):
        lines = t.split("\n")
        idxs = [i for i, l in enumerate(lines) if l.startswith("add_game(")]
        if not idxs:
            raise RuntimeError("CMakeLists.txt 里找不到任何 add_game( )，不敢自动插")
        backup(cm)
        lines.insert(idxs[-1] + 1, "add_game(%s)" % game)
        write_text(cm, "\n".join(lines))
        changed.append("CMakeLists.txt: + add_game(%s)" % game)

    for name in ("plain_assets.py", "mkromregion.py"):
        p = os.path.join(TOOLS, name)
        t = read_text(p)
        m = re.search(r"(?m)^MACHINES = \(([^)]*)\)", t)
        if not m:
            changed.append("%s: 找不到 MACHINES，手工确认" % name)
            continue
        names = re.findall(r'"([^"]+)"', m.group(1))
        if game in names:
            continue
        backup(p)
        newlist = ", ".join('"%s"' % n for n in names + [game])
        t = t[:m.start()] + "MACHINES = (" + newlist + ")" + t[m.end():]
        write_text(p, t)
        changed.append("%s: MACHINES += %s" % (name, game))
    for c in changed:
        log("  注册：" + c)


def registration_state(game):
    """返回 (cmake_ok, tuple_ok, machines_h_ok)。"""
    cm = read_text(os.path.join(PROJECT, "CMakeLists.txt"))
    cmake_ok = bool(re.search(r"(?m)^add_game\(\s*%s\s*\)" % re.escape(game), cm))
    tuple_ok = True
    for name in ("plain_assets.py", "mkromregion.py"):
        t = read_text(os.path.join(TOOLS, name))
        m = re.search(r"(?m)^MACHINES = \(([^)]*)\)", t)
        tuple_ok = tuple_ok and bool(m) and game in re.findall(r'"([^"]+)"', m.group(1))
    mh = read_text(os.path.join(PROJECT, "src", "machines.h"))
    machines_h_ok = ("GAME_%s" % game.upper()) in mh
    return cmake_ok, tuple_ok, machines_h_ok


# =====================================================================
#  流水线各步
# =====================================================================
def step_convert(cfg, game, log, holder):
    up = cfg["upstream"]
    conv = os.path.join(up, "romconv")
    if not os.path.isfile(os.path.join(conv, "convert.py")):
        raise RuntimeError("找不到 %s\\convert.py" % conv)
    srcm = os.path.join(up, "source", "src", "machines")
    bak = os.path.join(up, "source", "src", "machines_bak_before_" + game)
    if os.path.isdir(srcm) and not os.path.isdir(bak):
        shutil.copytree(srcm, bak)
        log("备份上游 machines -> %s" % os.path.basename(bak))
    rc = stream([python_exe(), "convert.py", "-j1", game], conv, log, cfg, holder)
    if rc != 0:
        raise RuntimeError("convert.py 失败（返回 %d）" % rc)


def step_copy(cfg, game, log):
    src = os.path.join(cfg["upstream"], "source", "src", "machines", game)
    if not os.path.isdir(src):
        raise RuntimeError("上游没有 %s 目录，转换没成功？" % src)
    dst = os.path.join(MACHINES_DIR, game)
    os.makedirs(dst, exist_ok=True)
    n = 0
    for f in sorted(os.listdir(src)):
        if f.lower().endswith((".cpp", ".h", ".gitignore")):
            shutil.copy2(os.path.join(src, f), os.path.join(dst, f))
            n += 1
    log("拷入 %d 个文件 -> src/machines/%s" % (n, game))


def step_plain(cfg, game, log, holder):
    rc = stream([python_exe(), os.path.join(TOOLS, "plain_assets.py"),
                 "--only", "%s_*.h" % game], PROJECT, log, cfg, holder)
    if rc != 0:
        raise RuntimeError("plain_assets.py 失败")


def step_extract(cfg, log, holder):
    restore_all_plain(log)
    rc = stream([python_exe(), os.path.join(TOOLS, "mkromregion.py"), "extract"],
                PROJECT, log, cfg, holder)
    if rc != 0:
        raise RuntimeError("mkromregion.py extract 失败")
    n, total = region_total()
    log("资产区：%d 个资产 / %d B（末地址 0x%08X）"
        % (n, total, GAL_ROM_BASE + total))


def step_configure(cfg, log, holder):
    if os.path.isfile(os.path.join(BUILD_DIR, "build.ninja")):
        log("已有 build-diag，跳过 cmake 配置（改过 CMakeLists 才会自动重配）")
        return
    cmake, ninja, _gcc = sdk_tools(cfg.get("sdk", ""))
    if not cmake or not ninja:
        raise RuntimeError("找不到 cmake / ninja，检查“设置”里的 SDK 路径")
    os.makedirs(BUILD_DIR, exist_ok=True)
    args = [cmake, "-S", PROJECT, "-B", BUILD_DIR, "-G", "Ninja",
            "-DCMAKE_MAKE_PROGRAM=" + ninja]
    bsp = (cfg.get("bsp") or "").strip()
    if bsp:
        args.append("-DPB_BSP_DIR=" + bsp)
        log("板级支持包：%s" % bsp)
    rc = stream(args, PROJECT, log, cfg, holder)
    if rc != 0:
        raise RuntimeError("cmake 配置失败")


def step_build(cfg, log, holder):
    _cmake, ninja, _gcc = sdk_tools(cfg.get("sdk", ""))
    if not ninja:
        raise RuntimeError("找不到 ninja")
    if not os.path.isfile(os.path.join(BUILD_DIR, "build.ninja")):
        step_configure(cfg, log, holder)
    rc = stream([ninja, "-j" + str(cfg.get("jobs", "2"))], BUILD_DIR, log, cfg, holder)
    size = fw_size()
    if rc != 0:
        raise RuntimeError("ninja 失败（返回 %d）" % rc)
    log("固件 %d B / 上限 %d B" % (size, FW_LIMIT))
    if size > FW_LIMIT:
        raise RuntimeError("固件超出上限，资产区会被顶掉")


def step_merge(cfg, log, holder):
    if os.path.isfile(DIST_UF2):
        prev = DIST_UF2[:-4] + "_prev.uf2"
        try:
            os.replace(DIST_UF2, prev)
            log("旧 UF2 改名 -> %s" % os.path.basename(prev))
        except OSError as e:
            raise RuntimeError("dist/galagino_16m.uf2 被别的程序占着：%s" % e)
    rc = stream([python_exe(), os.path.join(TOOLS, "mkromregion.py"), "merge"],
                PROJECT, log, cfg, holder)
    if rc != 0:
        raise RuntimeError("merge 失败")
    info = verify_uf2(DIST_UF2)
    log("UF2 %d B / %d 块 / numBlocks=%s / 末地址 0x%08X / 块号连续=%s"
        % (info["bytes"], info["blocks"], info["num_blocks"], info["end"],
           info["contiguous"]))


def find_uf2_drive():
    """找 BOOTSEL 盘：根目录有 INFO_UF2.TXT。"""
    if sys.platform != "win32":
        for mnt in ("/media", "/Volumes"):
            if not os.path.isdir(mnt):
                continue
            for d in sorted(os.listdir(mnt)):
                p = os.path.join(mnt, d)
                if os.path.isfile(os.path.join(p, "INFO_UF2.TXT")):
                    return p
        return ""
    for L in string.ascii_uppercase:
        root = L + ":\\"
        try:
            if os.path.isfile(os.path.join(root, "INFO_UF2.TXT")):
                return root
        except OSError:
            pass
    return ""


def find_picotool():
    p = shutil.which("picotool")
    if p:
        return p
    for root in (r"C:\Program Files\Raspberry Pi", r"E:\Program Files\Raspberry Pi"):
        if not os.path.isdir(root):
            continue
        for dp, dn, fn in os.walk(root):
            for f in fn:
                if f.lower() == "picotool.exe":
                    return os.path.join(dp, f)
            if dp.count(os.sep) - root.count(os.sep) > 3:
                dn[:] = []
    return ""


def upload_drive(log, src=DIST_UF2):
    drv = find_uf2_drive()
    if not drv:
        raise RuntimeError("没找到 RPI-RP2 盘：按住 BOOTSEL 插 USB，等盘出现再试")
    dst = os.path.join(drv, os.path.basename(src))
    log("写入 %s ..." % dst)
    t0 = time.time()
    shutil.copy2(src, dst)
    log("写完（%.1f s）。板子会自动重启。" % (time.time() - t0))


def upload_picotool(log, cfg, src=DIST_UF2):
    exe = find_picotool()
    if not exe:
        raise RuntimeError("找不到 picotool（可把它加到 PATH，或用“拖盘”方式）")
    holder = []
    rc = stream([exe, "load", "-v", src], PROJECT, log, cfg, holder)
    if rc != 0:
        raise RuntimeError("picotool load 失败（返回 %d）" % rc)
    stream([exe, "reboot"], PROJECT, log, cfg, holder)


# =====================================================================
#  GUI
# =====================================================================
class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("PicoBoy 街机工作台 —— ROM 转换 / 固件编译 / 合并上传")
        self.geometry("1180x860")
        self.minsize(1040, 720)

        self.cfg = load_config()
        self.logq = queue.Queue()
        self.busy = False
        self.holder = []
        self.jobs = {}

        self._build_ui()
        self.after(60, self._drain_log)
        self.refresh_all()

    # ---------------- 布局 ----------------
    def _build_ui(self):
        style = ttk.Style(self)
        try:
            style.theme_use("vista" if sys.platform == "win32" else "clam")
        except tk.TclError:
            pass
        pad = {"padx": 8, "pady": 4}

        # --- 环境 ---
        env = ttk.LabelFrame(self, text="环境（存到 tools\\studio_config.json）")
        env.pack(fill="x", **pad)
        self.var_proj = tk.StringVar(value=self.cfg["project"])
        self.var_up = tk.StringVar(value=self.cfg["upstream"])
        self.var_sdk = tk.StringVar(value=self.cfg["sdk"])
        for i, (label, var, browse) in enumerate((
                ("项目", self.var_proj, None),
                ("上游", self.var_up, self._pick_upstream),
                ("SDK", self.var_sdk, self._pick_sdk))):
            ttk.Label(env, text=label, width=5).grid(row=0, column=i * 3, padx=(8, 2), pady=5, sticky="w")
            ttk.Entry(env, textvariable=var).grid(row=0, column=i * 3 + 1, sticky="ew", pady=5)
            if browse:
                ttk.Button(env, text="…", width=3, command=browse).grid(
                    row=0, column=i * 3 + 2, padx=(2, 8), pady=5)
        env.columnconfigure(1, weight=2)
        env.columnconfigure(4, weight=2)
        env.columnconfigure(7, weight=3)
        ttk.Button(env, text="保存并刷新", command=self._save_env).grid(
            row=0, column=9, padx=8, pady=5)

        self.nb = ttk.Notebook(self)
        self.nb.pack(fill="both", expand=True, **pad)
        self._build_tab_rom()
        self._build_tab_build()
        self._build_tab_flash()
        self._build_tab_assets()

        # --- 状态条 + 日志 ---
        bar = ttk.Frame(self)
        bar.pack(fill="x", padx=8)
        self.lbl_status = ttk.Label(bar, text="", anchor="w")
        self.lbl_status.pack(side="left")
        self.lbl_sum = ttk.Label(bar, text="", anchor="e")
        self.lbl_sum.pack(side="right")

        logf = ttk.LabelFrame(self, text="日志")
        logf.pack(fill="both", expand=True, **pad)
        self.log = tk.Listbox(logf, height=10, activestyle="none",
                              font=("Consolas", 9) if sys.platform == "win32"
                              else ("DejaVu Sans Mono", 9))
        self.log.pack(side="left", fill="both", expand=True, padx=(8, 0), pady=6)
        sb = ttk.Scrollbar(logf, orient="vertical", command=self.log.yview)
        sb.pack(side="left", fill="y", pady=6)
        self.log.configure(yscrollcommand=sb.set)
        btns = ttk.Frame(logf)
        btns.pack(side="right", fill="y", padx=6, pady=6)
        ttk.Button(btns, text="复制日志", width=9, command=self._copy_log).pack(pady=2, fill="x")
        ttk.Button(btns, text="清空", width=9,
                   command=lambda: self.log.delete(0, "end")).pack(pady=2, fill="x")

    def _build_tab_rom(self):
        tab = ttk.Frame(self.nb)
        self.nb.add(tab, text="① ROM 转换")

        top = ttk.LabelFrame(tab, text="机型（上游 romconv 里的 job）")
        top.pack(fill="x", padx=8, pady=6)
        self.var_job = tk.StringVar()
        self.cb_job = ttk.Combobox(top, textvariable=self.var_job, width=22, state="readonly")
        self.cb_job.pack(side="left", padx=(8, 6), pady=6)
        self.cb_job.bind("<<ComboboxSelected>>", lambda e: self._on_job())
        ttk.Button(top, text="扫描 job", command=self.refresh_jobs).pack(side="left", pady=6)
        self.lbl_job = ttk.Label(top, text="")
        self.lbl_job.pack(side="left", padx=(12, 8))

        zf = ttk.LabelFrame(tab, text="ROM 包（.zip / .7z）")
        zf.pack(fill="x", padx=8, pady=6)
        self.var_zip = tk.StringVar()
        ttk.Entry(zf, textvariable=self.var_zip).pack(
            side="left", fill="x", expand=True, padx=(8, 6), pady=6)
        ttk.Button(zf, text="浏览…", command=self._pick_zip).pack(side="left", pady=6)
        ttk.Button(zf, text="校验 SHA-1", command=lambda: self._task("校验 ROM 包", self._do_check)).pack(
            side="left", padx=6, pady=6)

        tb = ttk.LabelFrame(tab, text="job 必备文件（SHA-1 对照）")
        tb.pack(fill="both", expand=True, padx=8, pady=6)
        cols = ("file", "sha1", "state", "member")
        self.tree_req = ttk.Treeview(tb, columns=cols, show="headings", height=9)
        for k, txt, w in (("file", "文件", 150), ("sha1", "SHA-1 前 12 位", 130),
                          ("state", "结果", 90), ("member", "包内命中名", 320)):
            self.tree_req.heading(k, text=txt)
            self.tree_req.column(k, width=w, anchor="w" if k in ("file", "member") else "center")
        self.tree_req.pack(side="left", fill="both", expand=True, padx=(8, 0), pady=6)
        s1 = ttk.Scrollbar(tb, orient="vertical", command=self.tree_req.yview)
        s1.pack(side="left", fill="y", padx=(0, 8), pady=6)
        self.tree_req.configure(yscrollcommand=s1.set)
        self.tree_req.tag_configure("bad", foreground="#b00020")

        act = ttk.Frame(tab)
        act.pack(fill="x", padx=8, pady=8)
        ttk.Button(act, text="一键集成（转换 → 明文 → 进资产区）",
                   command=lambda: self._task("集成 %s" % self.var_job.get(), self._do_integrate),
                   width=34).pack(side="left")
        self.btn_stop = ttk.Button(act, text="停止", command=self._stop, state="disabled")
        self.btn_stop.pack(side="left", padx=8)
        ttk.Label(act, text="注意：extract 会全量重排资产区，12 款一起重算", foreground="#8a6d00").pack(
            side="left", padx=12)

    def _build_tab_build(self):
        tab = ttk.Frame(self.nb)
        self.nb.add(tab, text="② 固件编译")
        box = ttk.LabelFrame(tab, text="增量编译（改过 CMakeLists 会自动重配）")
        box.pack(fill="x", padx=8, pady=6)
        ttk.Label(box, text="并行度").pack(side="left", padx=(8, 4), pady=6)
        self.var_par = tk.StringVar(value=self.cfg.get("jobs", "2"))
        ttk.Combobox(box, textvariable=self.var_par, width=4, state="readonly",
                     values=("1", "2", "3", "4")).pack(side="left", pady=6)
        ttk.Button(box, text="配置 CMake", command=lambda: self._task("cmake 配置", self._do_configure)).pack(
            side="left", padx=8, pady=6)
        ttk.Button(box, text="开始编译（编完自动合并）", command=lambda: self._task("编译", self._do_build)).pack(
            side="left", pady=6)
        ttk.Label(box, text="8 GB 机器别超过 -j2，否则 cc1 会 out of memory", foreground="#555").pack(
            side="left", padx=12)

        info = ttk.LabelFrame(tab, text="产物")
        info.pack(fill="x", padx=8, pady=6)
        self.lbl_fw = ttk.Label(info, text="", font=("Consolas", 10))
        self.lbl_fw.pack(anchor="w", padx=8, pady=6)
        self.lbl_paths = ttk.Label(info, text="", foreground="#555")
        self.lbl_paths.pack(anchor="w", padx=8, pady=(0, 6))

    def _build_tab_flash(self):
        tab = ttk.Frame(self.nb)
        self.nb.add(tab, text="③ 合并 & 烧写")
        box = ttk.LabelFrame(tab, text="合并（固件 0x10000000 + 资产区 0x10120000）")
        box.pack(fill="x", padx=8, pady=6)
        ttk.Button(box, text="合并成单个 UF2", width=22,
                   command=lambda: self._task("合并", self._do_merge)).pack(side="left", padx=8, pady=6)
        ttk.Button(box, text="校验 UF2", width=14,
                   command=lambda: self._task("校验 UF2", self._do_verify)).pack(side="left", pady=6)
        self.lbl_uf2 = ttk.Label(box, text="", font=("Consolas", 10))
        self.lbl_uf2.pack(side="left", padx=12)

        up = ttk.LabelFrame(tab, text="烧写")
        up.pack(fill="x", padx=8, pady=6)
        ttk.Button(up, text="烧到 RPI-RP2 盘（BOOTSEL 拖盘）", width=32,
                   command=lambda: self._task("拖盘烧写", self._do_drive)).pack(side="left", padx=8, pady=6)
        ttk.Button(up, text="picotool load", width=16,
                   command=lambda: self._task("picotool 上传", self._do_picotool)).pack(side="left", pady=6)
        self.lbl_drv = ttk.Label(up, text="", foreground="#555")
        self.lbl_drv.pack(side="left", padx=12)

    def _build_tab_assets(self):
        tab = ttk.Frame(self.nb)
        self.nb.add(tab, text="④ 资产区")
        tb = ttk.LabelFrame(tab, text="rom/romregion.bin（各机型占地）")
        tb.pack(fill="both", expand=True, padx=8, pady=6)
        cols = ("name", "type", "count", "bytes", "offset", "end")
        self.tree_reg = ttk.Treeview(tb, columns=cols, show="headings", height=14)
        for k, txt, w in (("name", "资产", 190), ("type", "类型", 170), ("count", "数量", 70),
                          ("bytes", "字节", 90), ("offset", "偏移", 110), ("end", "结束", 110)):
            self.tree_reg.heading(k, text=txt)
            self.tree_reg.column(k, width=w, anchor="w" if k in ("name", "type") else "center")
        self.tree_reg.pack(side="left", fill="both", expand=True, padx=(8, 0), pady=6)
        s2 = ttk.Scrollbar(tb, orient="vertical", command=self.tree_reg.yview)
        s2.pack(side="left", fill="y", padx=(0, 8), pady=6)
        self.tree_reg.configure(yscrollcommand=s2.set)

        foot = ttk.Frame(tab)
        foot.pack(fill="x", padx=8, pady=6)
        ttk.Button(foot, text="刷新", command=self.refresh_assets, width=10).pack(side="left")
        self.lbl_reg = ttk.Label(foot, text="", font=("Consolas", 10))
        self.lbl_reg.pack(side="left", padx=12)

    # ---------------- 环境 / 刷新 ----------------
    def _pick_upstream(self):
        p = filedialog.askdirectory(title="选 galagino-max 目录")
        if p:
            self.var_up.set(p)

    def _pick_sdk(self):
        p = filedialog.askdirectory(title="选 pico-sdk 目录")
        if p:
            self.var_sdk.set(p)

    def _save_env(self):
        self.cfg["project"] = self.var_proj.get().strip()
        self.cfg["upstream"] = self.var_up.get().strip()
        self.cfg["sdk"] = self.var_sdk.get().strip()
        self.cfg["jobs"] = self.var_par.get()
        save_config(self.cfg)
        self._log("设置已保存")
        self.refresh_all()

    def refresh_all(self):
        self.refresh_jobs()
        self.refresh_assets()
        self._refresh_sum()

    def refresh_jobs(self):
        self.jobs = scan_jobs(self.cfg["upstream"])
        names = sorted(self.jobs)
        self.cb_job["values"] = names
        if names and not self.var_job.get():
            self.var_job.set(names[0])
        self._on_job()
        self._log("扫到 %d 个 job" % len(names))

    def _on_job(self):
        name = self.var_job.get()
        path = self.jobs.get(name, "")
        if not path:
            self.lbl_job.configure(text="")
            return
        reqs = required_of(path)
        reg = registration_state(name) if name else (False, False, False)
        self.lbl_job.configure(
            text="%s ｜ 必需文件 %d 个 ｜ 已注册：CMake %s / MACHINES %s / machines.h %s"
                 % (os.path.relpath(path, self.cfg["upstream"]), len(reqs),
                    "是" if reg[0] else "否", "是" if reg[1] else "否",
                    "是" if reg[2] else "否"))

    def refresh_assets(self):
        self.tree_reg.delete(*self.tree_reg.get_children())
        items = read_region()
        for a in items:
            off = a.get("offset", 0)
            self.tree_reg.insert("", "end", values=(
                a.get("name", ""), a.get("type", ""), a.get("count", ""),
                a.get("bytes", 0), "0x%06X" % off,
                "0x%06X" % (GAL_ROM_BASE + off + a.get("bytes", 0))))
        n = len(items)
        total = region_total()[1]
        cap = 16 * 1024 * 1024 - 0x120000
        self.lbl_reg.configure(
            text="%d 个资产 / %d B ｜ 0x%08X .. 0x%08X ｜ 占用 %.1f%% of %d B"
                 % (n, total, GAL_ROM_BASE, GAL_ROM_BASE + total,
                    100.0 * total / cap if cap else 0, cap))
        self._refresh_sum()

    def _refresh_sum(self):
        size = fw_size()
        n, total = region_total()
        self.lbl_sum.configure(
            text="固件 %s B / %d ｜ 资产 %d 项 / %d B ｜ 合计 %s B"
                 % (f"{size:,}" if size else "—", FW_LIMIT, n, total,
                    f"{size + total:,}" if size else "—"))
        self.lbl_fw.configure(
            text=("固件 %s B（上限 %s B）｜%s"
                  % (f"{size:,}", f"{FW_LIMIT:,}",
                     "OK" if size and size <= FW_LIMIT else ("缺产物" if not size else "超限"))))
        self.lbl_paths.configure(
            text="\n".join([FW_BIN, FW_UF2, DIST_UF2]))
        self.lbl_uf2.configure(text=self._uf2_text())
        drv = find_uf2_drive()
        self.lbl_drv.configure(text=("BOOTSEL 盘：%s" % drv) if drv else "BOOTSEL 盘：未检测到")

    def _uf2_text(self):
        if not os.path.isfile(DIST_UF2):
            return "还没有合并产物"
        try:
            i = verify_uf2(DIST_UF2)
            return ("%d B / %d 块 / 末地址 0x%08X%s"
                    % (i["bytes"], i["blocks"], i["end"],
                       "" if i["contiguous"] else "（块号不连续！）"))
        except Exception as e:
            return "校验失败：%s" % e

    # ---------------- 日志 ----------------
    def _log(self, msg):
        self.logq.put(str(msg))

    def _drain_log(self):
        try:
            while True:
                msg = self.logq.get_nowait()
                self.log.insert("end", msg)
                if int(self.log.index("end")) > 4000:
                    self.log.delete(0, 500)
                self.log.see("end")
        except queue.Empty:
            pass
        self.after(60, self._drain_log)

    def _copy_log(self):
        self.clipboard_clear()
        self.clipboard_append("\n".join(self.log.get(0, "end")))
        messagebox.showinfo("已复制", "日志已复制到剪贴板。")

    # ---------------- 任务调度 ----------------
    def set_status(self, text):
        self.lbl_status.configure(text=text)

    def _task(self, title, fn):
        if self.busy:
            messagebox.showinfo("正在忙", "上一件事还没结束。")
            return
        self.busy = True
        self.set_status(title + " …")
        self.btn_stop.configure(state="normal")
        self._log("=== %s ===" % title)

        def work():
            try:
                fn()
            except Exception as e:
                self._log("!! %s" % e)
            finally:
                self.busy = False
                self.holder[:] = []
                self._log("--- %s 结束 ---" % title)
                self.after(0, lambda: self.set_status(""))
                self.after(0, lambda: self.btn_stop.configure(state="disabled"))
                self.after(0, self._refresh_sum)
                self.after(0, self.refresh_assets)

        threading.Thread(target=work, daemon=True).start()

    def _stop(self):
        if self.holder:
            try:
                self.holder[0].kill()
                self._log("已发送 kill")
            except Exception as e:
                self._log("kill 失败：%s" % e)

    # ---------------- 动作：转换 ----------------
    def _pick_zip(self):
        p = filedialog.askopenfilename(
            title="选 ROM 包", filetypes=[("ROM 包", "*.zip *.7z"), ("所有文件", "*.*")])
        if p:
            self.var_zip.set(p)

    def _do_check(self):
        game = self.var_job.get()
        zpath = self.var_zip.get().strip()
        if not game or not self.jobs.get(game):
            raise RuntimeError("先选机型")
        if not os.path.isfile(zpath):
            raise RuntimeError("ROM 包不存在：%s" % zpath)
        reqs = required_of(self.jobs[game])
        if not reqs:
            raise RuntimeError("这个 job 里解析不出 (文件名, SHA-1) 表，直接跑转换看日志")
        self._log("job 声明 %d 个必需文件" % len(reqs))
        rows, n = check_archive(zpath, reqs)
        self.tree_req.delete(*self.tree_req.get_children())
        ok = 0
        for fn, sha, state, member in rows:
            tag = () if state == "OK" else ("bad",)
            self.tree_req.insert("", "end", values=(fn, sha, state, member), tags=tag)
            ok += state == "OK"
        self._log("包内 %d 个成员；命中 %d / %d" % (n, ok, len(reqs)))
        if ok != len(reqs):
            self._log("缺文件就别往下走：romconv 逐文件校验，bootleg/改版套会直接失败")

    def _do_integrate(self):
        game = self.var_job.get()
        if not game:
            raise RuntimeError("先选机型")
        log = self._log
        log("机型：%s" % game)
        step_convert(self.cfg, game, log, self.holder)        # 1) 转 C 头（会重写所有 logo）
        restore_all_plain(log)                                # 2) 立刻把别的 11 款救回来
        step_copy(self.cfg, game, log)                        # 3) 拷机器源
        step_plain(self.cfg, game, log, self.holder)          # 4) 压缩 -> 明文
        patch_logo(game, log)                                 # 5) Logo 去掉 constexpr
        ensure_registration(game, log)                        # 6) add_game + MACHINES
        snapshot_plain(game, log)                             # 7) 明文快照
        step_extract(self.cfg, log, self.holder)              # 8) 回灌 + 抽区
        log("集成完成。下一步：② 固件编译 → ③ 合并 & 烧写")

    # ---------------- 动作：编译 ----------------
    def _do_configure(self):
        self.cfg["jobs"] = self.var_par.get()
        save_config(self.cfg)
        step_configure(self.cfg, self._log, self.holder)

    def _do_build(self):
        self.cfg["jobs"] = self.var_par.get()
        save_config(self.cfg)
        step_build(self.cfg, self._log, self.holder)
        # 编完立刻合并：dist 里要留一份"固件 + 资产区"的 UF2 给烧写用。
        # 单固件 UF2 只写 0x10000000..0x10037xxx，板上资产区不动，而固件里的
        # 资产偏移表是编译期写死的——两者不是同一版就会崩，所以必须一起写。
        self._log("编译完成，接着合并资产区（生成的 UF2 已含 ROM，烧它即可，不用再单独上传 ROM）")
        step_merge(self.cfg, self._log, self.holder)

    # ---------------- 动作：合并 / 烧写 ----------------
    def _do_merge(self):
        if not os.path.isfile(FW_UF2):
            raise RuntimeError("还没有固件产物，先编译")
        step_merge(self.cfg, self._log, self.holder)

    def _do_verify(self):
        if not os.path.isfile(DIST_UF2):
            raise RuntimeError("还没有 dist/galagino_16m.uf2")
        i = verify_uf2(DIST_UF2)
        self._log("块数 %d ｜ numBlocks %s ｜ 末地址 0x%08X ｜ 连续 %s"
                  % (i["blocks"], i["num_blocks"], i["end"], i["contiguous"]))
        total = region_total()[1]
        want = GAL_ROM_BASE + total
        self._log("末地址应为 0x%08X（0x10120000 + 资产区 %d B）：%s"
                  % (want, total, "OK" if i["end"] == want else "不一致！"))

    def _do_drive(self):
        upload_drive(self._log)

    def _do_picotool(self):
        upload_picotool(self._log, self.cfg)


# =====================================================================
#  自检（不开窗口）
# =====================================================================
def selftest():
    cfg = load_config()
    print("项目：%s" % cfg["project"])
    print("上游：%s ｜ 存在=%s" % (cfg["upstream"], os.path.isdir(cfg["upstream"])))
    print("SDK ：%s ｜ 存在=%s" % (cfg["sdk"] or "(未设置)", os.path.isdir(cfg["sdk"]) if cfg["sdk"] else False))
    cmake, ninja, gcc = sdk_tools(cfg["sdk"])
    print("工具：cmake=%s\n      ninja=%s\n      gcc=%s"
          % (cmake or "(缺)", ninja or "(缺)", gcc or "(缺)"))

    jobs = scan_jobs(cfg["upstream"])
    print("\njob 数：%d" % len(jobs))
    for name in ("gberet", "xevious", "commando"):
        p = jobs.get(name)
        print("  %-10s %s  必需 %d 个" % (name, "OK" if p else "缺", len(required_of(p)) if p else 0))

    items = read_region()
    total = region_total()[1]
    print("\n资产区：%d 个资产 / %d B / 末地址 0x%08X"
          % (len(items), total, GAL_ROM_BASE + total))
    for a in items[-6:]:
        print("  %-22s 0x%06X  %d B" % (a["name"], a["offset"], a["bytes"]))

    size = fw_size()
    print("\n固件：%s B / 上限 %d" % (size or "—", FW_LIMIT))

    if os.path.isfile(DIST_UF2):
        i = verify_uf2(DIST_UF2)
        print("UF2 ：%d B / %d 块 / numBlocks %s / 末地址 0x%08X / 连续 %s"
              % (i["bytes"], i["blocks"], i["num_blocks"], i["end"], i["contiguous"]))
    else:
        print("UF2 ：缺")

    print("\nBOOTSEL 盘：%s" % (find_uf2_drive() or "未检测到"))
    print("picotool  ：%s" % (find_picotool() or "未找到"))
    reg = registration_state("gberet")
    print("gberet 注册：CMake %s / MACHINES %s / machines.h %s" % reg)
    print("\nSELFTEST_OK")
    return 0


def smoke(out_path):
    """真窗口自检：建窗口、量控件、写报告、销毁。"""
    app = App()
    app.update_idletasks()
    app.update()
    found = []

    def walk(w):
        for c in w.winfo_children():
            if isinstance(c, ttk.Button):
                found.append("%s  %dx%d  mapped=%d"
                             % (c.cget("text"), c.winfo_width(), c.winfo_height(),
                                c.winfo_ismapped()))
            walk(c)

    walk(app)
    lines = [
        "tk=%s" % tk.TkVersion,
        "window=%dx%d" % (app.winfo_width(), app.winfo_height()),
        "title=%s" % app.title(),
        "tabs=%s" % [app.nb.tab(i, "text") for i in app.nb.tabs()],
        "jobs=%d" % len(app.jobs),
        "req_rows=%d" % len(app.tree_req.get_children()),
        "asset_rows=%d" % len(app.tree_reg.get_children()),
        "buttons=%d" % len(found),
    ] + found + ["SMOKE_OK"]
    write_text(out_path, "\n".join(lines))
    app.destroy()
    return 0


def main():
    if "--selftest" in sys.argv:
        return selftest()
    if "--smoke" in sys.argv:
        i = sys.argv.index("--smoke")
        return smoke(sys.argv[i + 1])
    if sys.platform == "win32":
        try:
            import ctypes
            ctypes.windll.shcore.SetProcessDpiAwareness(1)
        except Exception:
            pass
    App().mainloop()
    return 0


if __name__ == "__main__":
    sys.exit(main() or 0)
