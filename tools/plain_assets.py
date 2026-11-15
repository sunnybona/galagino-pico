"""把 romconv 生成的压缩资产头就地改写成 flash 直读（PLAIN）形式。

背景：romconv 默认用 `emit_compressed`，头文件里只有一个
`static const unsigned char <name>_packed[]`（LZMA1 原始流，前面 1 字节是
props）和一个 `FlashAsset<T, COMPRESSED>` 描述符。设备上第一次访问时解包
进 Arena，这需要 ~57 KB RAM（RP2040 上刚好撞墙）。

本工具做纯本地转换：
  1. 解出 LZMA 原始字节（参数与 romconv/romconv/emit/lzma.py 一致：
     props 字节 + raw LZMA1，lc=lp=0，pb 从 props 里读，带 end marker）
  2. 改写成 `alignas(4) static const <T> <name>_data[][dims] = {...}`
     + `FlashAsset<T, PLAIN>`，数据留在 flash，XIP 直读，零拷贝、零 Arena
  3. 由 `[][dims]` 自动推出外层维度 = count

用法：
    python tools/plain_assets.py            # 转换 dkong + galaga 的头
    python tools/plain_assets.py --check    # 只校验，不写文件
"""
import argparse
import lzma
import pathlib
import re
import sys

MACHINES = ("dkong", "galaga", "1942", "pacman", "yiear", "contra", "gng", "xevious", "sxevious", "1943", "commando","gberet")

PACKED_ARRAY = re.compile(
    r"// (\w+): (\d+) -> (\d+) bytes[^\n]*\n"
    r"static const unsigned char (\w+)_packed\[\] = \{\n(?P<body>.*?)\n\};\n"
    r"static constexpr FlashAsset<(?P<type>[^,]+), COMPRESSED> (\w+)_blob = \{.*?\};\n",
    re.S,
)

BASE_TYPES = {
    "unsigned char": 1,
    "uint8_t": 1,
    "unsigned short": 2,
    "uint16_t": 2,
    "uint32_t": 4,
    "int": 4,
}


def parse_type(t: str):
    """'unsigned short[256][4]' -> ('unsigned short', '[256][4]', [256, 4])"""
    t = t.strip()
    for base in sorted(BASE_TYPES, key=len, reverse=True):
        if t.startswith(base):
            rest = t[len(base):]
            dims = [int(d) for d in re.findall(r"\[\s*(\d+)\s*\]", rest)]
            return base, rest, dims
    raise ValueError(f"unknown asset type: {t}")


def unpack_props(packed: bytes, raw_len: int) -> bytes:
    props = packed[0]
    lc, rest = props % 9, props // 9
    lp, pb = rest % 5, rest // 5
    filters = [{"id": lzma.FILTER_LZMA1, "dict_size": 1 << 22,
                "lc": lc, "lp": lp, "pb": pb}]
    raw = lzma.decompress(packed[1:], format=lzma.FORMAT_RAW, filters=filters)
    if len(raw) != raw_len:
        raise ValueError(f"raw length {len(raw)} != {raw_len}")
    return raw


def fmt_value(v: int, base: str) -> str:
    return f"0x{v:X}" if base != "unsigned char" else str(v)


def element_text(raw: bytes, base: str, dims) -> str:
    width = BASE_TYPES[base]
    count = max(1, 1)
    for d in dims:
        count *= d
    vals = []
    for i in range(count):
        chunk = raw[i * width:(i + 1) * width]
        vals.append(fmt_value(int.from_bytes(chunk, "little"), base))
    if not dims:
        return vals[0]
    if len(dims) == 1:
        return "{ " + ", ".join(vals) + " }"
    return "{ " + ", ".join(vals) + " }"


def convert(path: pathlib.Path, check_only: bool) -> bool:
    text = path.read_text(encoding="utf-8", newline="")
    changed = False

    def repl(m: re.Match) -> str:
        nonlocal changed
        name = m.group(4)
        raw_len = int(m.group(2))
        packed_len = int(m.group(3))
        type_str = m.group("type")
        # count: PLAIN descriptor's second field, taken from the COMPRESSED one
        blob_text = m.group(0)
        count = int(re.search(r"_blob = \{\s*\w+_packed,\s*sizeof\(\w+_packed\),\s*(\d+)", blob_text).group(1))

        values = [int(x, 16) if x.lower().startswith("0x") else int(x)
                  for x in re.findall(r"0x[0-9A-Fa-f]+|\d+", m.group("body"))]
        packed = bytes(v & 0xFF for v in values)
        if len(packed) != packed_len:
            raise ValueError(f"{name}: packed {len(packed)} != {packed_len}")

        base, dims_suffix, dims = parse_type(type_str)
        raw = unpack_props(packed, raw_len)
        elem_bytes = BASE_TYPES[base]
        for d in dims:
            elem_bytes *= d
        if elem_bytes * count != raw_len:
            raise ValueError(f"{name}: {count} x {elem_bytes} != {raw_len}")

        rows = []
        for i in range(count):
            chunk = raw[i * elem_bytes:(i + 1) * elem_bytes]
            rows.append("  " + element_text(chunk, base, dims) + ",")
        array_dims = "[]" + dims_suffix
        out = (
            f"// {name}: {raw_len} bytes, 直接放在 flash 里用 XIP 读（零拷贝、不占 Arena）\n"
            f"alignas(4) static const {base} {name}_data{array_dims} = {{\n"
            + "\n".join(rows) + "\n"
            f"}};\n"
            f"static constexpr FlashAsset<{type_str}, PLAIN> {name}_blob = {{ {name}_data, {count} }};\n"
        )
        changed = True
        print(f"  {name}: {packed_len} -> {raw_len} bytes ({count} x {elem_bytes})")
        return out

    new_text = PACKED_ARRAY.sub(repl, text)
    if not changed:
        print(f"  (skip) {path.name}: 没有压缩资产")
        return False
    if not check_only:
        path.write_text(new_text, encoding="utf-8", newline="")
    return True


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true", help="只转换到内存，不写回")
    ap.add_argument("--only", default="", help="只转换文件名匹配该 glob 的头（如 1942_logo.h）")
    args = ap.parse_args()

    src = pathlib.Path(__file__).resolve().parent.parent / "src" / "machines"
    total = 0
    for machine in MACHINES:
        print(f"[{machine}]")
        for path in sorted((src / machine).glob(args.only or "*.h")):
            if convert(path, args.check):
                total += 1
    print(f"\n{'校验' if args.check else '改写'}完成: {total} 个文件")
    return 0


if __name__ == "__main__":
    sys.exit(main())
