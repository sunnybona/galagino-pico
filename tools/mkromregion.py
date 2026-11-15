"""资产搬到片内 flash 的固定偏移区（16 MB 板，0x10120000 起），XIP 直读。

两步：
  1. `extract`：把 src/machines/{dkong,galaga} 里已经解压好的 `*_data[]`
     明文数组抽出来，按 4 字节对齐排成一块连续区域
     `rom/romregion.bin`，同时把对应头文件改写为"地址 + 偏移"的指针形式：
         static constexpr FlashAsset<unsigned char, PLAIN>
             dkong_rom_cpu1_blob = { (const unsigned char *)(GAL_ROM_BASE + 0x0000), 16384 };
     这样固件里不再有数据副本，只有指针。
  2. `merge`：把固件 UF2 与 romregion.bin 合并成一个可以直接拖进
     BOOTSEL 盘的单文件，ROM 区落在 GAL_ROM_BASE。

注意：`extract` 是单向的（数组被指针替换后就没了），所以 romregion.bin
就是此后资产的真身；要重新生成得回到 romconv + tools/plain_assets.py。
"""
import argparse
import json
import pathlib
import re
import struct
import sys

GAL_ROM_BASE = 0x10120000
MACHINES = ("dkong", "galaga", "1942", "pacman", "yiear", "contra", "gng", "xevious", "sxevious", "1943", "commando","gberet")

BASE_WIDTH = {
    "unsigned char": 1, "uint8_t": 1,
    "unsigned short": 2, "uint16_t": 2,
    "uint32_t": 4, "int": 4,
}

# 前面那行注释可有可无：tools/plain_assets.py 的输出带
# `// <name>: N bytes, ...`，而 romconv 自己生成的（1942）带的是
# `// arena-asset <name> ...`（PLAIN 为 1 时直接就是明文数组）。
ARRAY = re.compile(
    r"(?:// (?P<name0>\w+): (?P<rawlen>\d+) bytes[^\n]*\n)?"
    r"alignas\(4\) static const (?P<base>(?:unsigned )?\w+) (?P<name2>\w+)_data"
    r"(?P<dimsuf>\[[^=]*\]) = \{\n(?P<body>.*?)\n\};\n"
    r"static constexpr FlashAsset<(?P<type>[^,]+), PLAIN> (?P<blob>\w+)_blob = "
    r"\{ (?P<dims>\w+)_data, (?P<count>\d+) \};",
    re.S,
)


def parse_dims(suffix: str):
    return [int(d) for d in re.findall(r"\[(\d+)\]", suffix)]


def pointer_decl(base: str, dimsuf: str) -> str:
    """`_data[][256][4]` -> `const unsigned short (*)[256][4]`；标量 `_data[]` -> `const unsigned char *`

    声明里的第一组 `[]` 是"外层维度由初值推出"，指针类型里要去掉。
    """
    dims = dimsuf.strip()
    if dims.startswith("[]"):
        dims = dims[2:]
    if not dims:
        return f"(const {base} *)"
    return f"(const {base} (*){dims})"


def extract(src: pathlib.Path, out_bin: pathlib.Path, out_json: pathlib.Path) -> int:
    entries = []
    for machine in MACHINES:
        for path in sorted((src / machine).glob("*.h")):
            text = path.read_text(encoding="utf-8", newline="")
            for m in ARRAY.finditer(text):
                base = m.group("base")
                width = BASE_WIDTH[base]
                values = [int(x, 16) if x.lower().startswith("0x") else int(x)
                          for x in re.findall(r"0x[0-9A-Fa-f]+|\d+", m.group("body"))]
                data = b"".join(v.to_bytes(width, "little") for v in values)
                entries.append({
                    "file": path,
                    "match": m,
                    "name": m.group("name2"),
                    "base": base,
                    "dims": m.group("dimsuf"),
                    "type": m.group("type"),
                    "count": int(m.group("count")),
                    "data": data,
                })

    if not entries:
        print("没有可抽取的明文数组（可能已经抽取过了）")
        return 1

    region = bytearray()
    for e in entries:
        # 4 字节对齐，和 alignas(4) 一致
        pad = (-len(region)) % 4
        region += b"\x00" * pad
        e["offset"] = len(region)
        region += e["data"]
        print(f"  {e['name']:<22} off=0x{e['offset']:06X} {len(e['data']):>6} B")

    out_bin.parent.mkdir(parents=True, exist_ok=True)
    out_bin.write_bytes(bytes(region))
    out_json.write_text(
        json.dumps([{"name": e["name"], "file": e["file"].name, "type": e["type"],
                     "count": e["count"], "offset": e["offset"], "bytes": len(e["data"])}
                    for e in entries], indent=2),
        encoding="utf-8")

    # 改写头文件：数组 -> 指针
    by_file = {}
    for e in entries:
        by_file.setdefault(e["file"], []).append(e)

    for path, items in by_file.items():
        text = path.read_text(encoding="utf-8", newline="")
        for e in items:  # ARRAY 的匹配位置在原文本里，逐个替换
            old = e["match"].group(0)
            decl = pointer_decl(e["base"], e["dims"])
            new = (
                f"// {e['name']}: {len(e['data'])} bytes，住在 ROM 区 "
                f"0x{GAL_ROM_BASE + e['offset']:08X}（{e['offset']:#06x}），XIP 直读\n"
                f"// 注意：整数 -> 指针的转换不是常量表达式，所以这里是 static const\n"
                f"// （启动时初始化），不是 constexpr。\n"
                f"static const FlashAsset<{e['type']}, PLAIN> {e['name']}_blob = "
                f"{{ {decl}(GAL_ROM_BASE + {e['offset']:#06x}u), {e['count']} }};"
            )
            text = text.replace(old, new, 1)
        path.write_text(text, encoding="utf-8", newline="")

    print(f"\n区域合计 {len(region)} B -> {out_bin}")
    return 0


def uf2_blocks(addr: int, payload: bytes, start_no: int = 0, total: int = 0):
    """256 字节 payload 的标准 RP2040 UF2 块。"""
    MAGIC0, MAGIC1, MAGIC_END = 0x0A324655, 0x9E5D5157, 0x0AB16F30
    FAMILY = 0xE48BFF56  # rp2040
    blocks = []
    total = total or (len(payload) + 255) // 256
    for i, off in enumerate(range(0, len(payload), 256)):
        chunk = payload[off:off + 256]
        body = chunk + b"\x00" * (476 - len(chunk))
        blocks.append(struct.pack("<8I", MAGIC0, MAGIC1, 0x00002000,
                                  addr + off, len(chunk), start_no + i, total, FAMILY)
                      + body + struct.pack("<I", MAGIC_END))
    return blocks


def merge(fw_uf2: pathlib.Path, region: pathlib.Path, out: pathlib.Path) -> int:
    fw = fw_uf2.read_bytes()
    if len(fw) % 512:
        print("固件 UF2 不是 512 的整数倍")
        return 1
    region_len = region.stat().st_size
    fw_blocks = len(fw) // 512
    region_blocks = (region_len + 255) // 256
    print(f"固件 {fw_blocks} 块 / ROM 区 {region_len} B = {region_blocks} 块")

    # UF2 的 numBlocks 要覆盖全部块，逐块改写（字段位置：blockNo@20, numBlocks@24）
    fixed = bytearray()
    for i in range(fw_blocks):
        blk = bytearray(fw[i * 512:(i + 1) * 512])
        blk[24:28] = struct.pack("<I", fw_blocks + region_blocks)
        fixed += blk
    merged = bytes(fixed) + b"".join(uf2_blocks(GAL_ROM_BASE, region.read_bytes(),
                                               fw_blocks, fw_blocks + region_blocks))
    out.write_bytes(merged)
    print(f"合并后 {out} ({len(merged)} B)")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)

    e = sub.add_parser("extract", help="抽数据成 romregion.bin 并把头改成指针")
    e.add_argument("--root", default=str(pathlib.Path(__file__).resolve().parent.parent))

    m = sub.add_parser("merge", help="固件 UF2 + ROM 区 -> 单个 UF2")
    m.add_argument("--root", default=str(pathlib.Path(__file__).resolve().parent.parent))
    m.add_argument("--fw", default=None)
    m.add_argument("--out", default=None)

    a = ap.parse_args()
    root = pathlib.Path(a.root)
    rom_bin = root / "rom" / "romregion.bin"
    rom_json = root / "rom" / "romregion.json"

    if a.cmd == "extract":
        return extract(root / "src" / "machines", rom_bin, rom_json)

    fw = pathlib.Path(a.fw) if a.fw else root / "build-diag" / "galagino_arcade.uf2"
    out = pathlib.Path(a.out) if a.out else root / "dist" / "galagino_16m.uf2"
    if not rom_bin.exists():
        print(f"缺 {rom_bin}，先跑 extract")
        return 1
    return merge(fw, rom_bin, out)


if __name__ == "__main__":
    sys.exit(main())
