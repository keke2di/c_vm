import sys
import unicodedata
from pathlib import Path

MAX = 0x110000

FLAGS = [
    "ALPHA",
    "DECIMAL",
    "DIGIT",
    "NUMERIC",
    "PRINTABLE",
    "LOWER",
    "UPPER",
    "TITLE",
    "CASED",
    "IDSTART",
    "IDCONT",
]

MAX_EXPANSION = 4


def classify(cp):
    ch = chr(cp)
    flags = 0

    if ch.isalpha():
        flags |= 1 << FLAGS.index("ALPHA")
    if ch.isdecimal():
        flags |= 1 << FLAGS.index("DECIMAL")
    if ch.isdigit():
        flags |= 1 << FLAGS.index("DIGIT")
    if ch.isnumeric():
        flags |= 1 << FLAGS.index("NUMERIC")
    if ch.isprintable():
        flags |= 1 << FLAGS.index("PRINTABLE")
    if ch.islower():
        flags |= 1 << FLAGS.index("LOWER")
    if ch.isupper():
        flags |= 1 << FLAGS.index("UPPER")
    if ch.istitle():
        flags |= 1 << FLAGS.index("TITLE")
    if ch.lower() != ch or ch.upper() != ch:
        flags |= 1 << FLAGS.index("CASED")
    if ch.isidentifier():
        flags |= 1 << FLAGS.index("IDSTART")
    if ("a" + ch).isidentifier():
        flags |= 1 << FLAGS.index("IDCONT")

    decimal = int(ch) if ch.isdecimal() else 0
    mapped = (ch.lower(), ch.upper(), ch.title(), ch.casefold())

    return flags, decimal, mapped


def build_records():
    records = {}
    order = []
    cp_index = [0] * MAX
    specials = {}
    special_order = []

    for cp in range(MAX):
        flags, decimal, mapped = classify(cp)

        if max(len(m) for m in mapped) > 1:
            if mapped not in specials:
                specials[mapped] = len(special_order) + 1
                special_order.append(mapped)
            special = specials[mapped]
            deltas = (0, 0, 0, 0)
        else:
            special = 0
            deltas = tuple(ord(m) - cp for m in mapped)

        record = (flags, decimal, special) + deltas
        index = records.get(record)

        if index is None:
            index = len(order)
            records[record] = index
            order.append(record)

        cp_index[cp] = index

    for mapped in special_order:
        if max(len(m) for m in mapped) > MAX_EXPANSION:
            raise SystemExit(f"expansion longer than {MAX_EXPANSION}: {mapped!r}")

    return order, cp_index, special_order


def index_type(count):
    return "uint8_t" if count <= 256 else "uint16_t"


def index_size(count):
    return 1 if count <= 256 else 2


def split_stage(values, shift):
    size = 1 << shift
    blocks = {}
    parents = []

    for base in range(0, len(values), size):
        block = tuple(values[base:base + size])
        index = blocks.get(block)

        if index is None:
            index = len(blocks)
            blocks[block] = index

        parents.append(index)

    flat = []
    for block in blocks:
        flat.extend(block)

    return parents, flat, len(blocks)


def plan(cp_index, record_count):
    best = None
    record_bytes = index_size(record_count)

    for shift2 in range(3, 9):
        for shift3 in range(3, 9):
            mid, stage3, sub_count = split_stage(cp_index, shift3)
            stage1, stage2, mid_count = split_stage(mid, shift2)

            total = (
                len(stage1) * index_size(mid_count)
                + len(stage2) * index_size(sub_count)
                + len(stage3) * record_bytes
            )

            if best is None or total < best[0]:
                best = (total, shift2, shift3, stage1, stage2, stage3, mid_count, sub_count)

    return best


def emit_array(out, ctype, name, values, per_line):
    out.append(f"static const {ctype} {name}[] = {{")

    for start in range(0, len(values), per_line):
        chunk = values[start:start + per_line]
        out.append("    " + ",".join(str(v) for v in chunk) + ",")

    out.append("};")
    out.append("")


def main(argv):
    if len(argv) != 2:
        raise SystemExit("usage: gen_unicode.py <output header>")

    target = Path(argv[1])
    order, cp_index, special_order = build_records()
    total, shift2, shift3, stage1, stage2, stage3, mid_count, sub_count = plan(
        cp_index, len(order)
    )

    special_cps = []
    special_entries = []

    for mapped in special_order:
        offsets = []
        lengths = []
        for text in mapped:
            offsets.append(len(special_cps))
            lengths.append(len(text))
            special_cps.extend(ord(c) for c in text)
        special_entries.append((offsets, lengths))

    out = []
    out.append("#ifndef CVM_UNICODE_DATA_H")
    out.append("#define CVM_UNICODE_DATA_H")
    out.append("")
    out.append("#include <stdint.h>")
    out.append("")
    out.append(f'#define UNI_VERSION "{unicodedata.unidata_version}"')
    out.append(f"#define UNI_SHIFT2 {shift2}")
    out.append(f"#define UNI_SHIFT3 {shift3}")
    out.append("")

    for bit, name in enumerate(FLAGS):
        out.append(f"#define UNI_{name} {1 << bit}u")

    out.append("")
    out.append("typedef struct {")
    out.append("    uint16_t flags;")
    out.append("    uint8_t decimal;")
    out.append("    uint16_t special;")
    out.append("    int32_t lower;")
    out.append("    int32_t upper;")
    out.append("    int32_t title;")
    out.append("    int32_t casefold;")
    out.append("} UniRecord;")
    out.append("")
    out.append("typedef struct {")
    out.append("    uint16_t offset[4];")
    out.append("    uint8_t length[4];")
    out.append("} UniSpecial;")
    out.append("")

    out.append(f"static const UniRecord UNI_RECORDS[{len(order)}] = {{")
    for flags, decimal, special, lower, upper, title, casefold in order:
        out.append(
            f"    {{{flags}u,{decimal}u,{special}u,{lower},{upper},{title},{casefold}}},"
        )
    out.append("};")
    out.append("")

    out.append(f"static const UniSpecial UNI_SPECIALS[{len(special_entries) + 1}] = {{")
    out.append("    {{0,0,0,0},{0,0,0,0}},")
    for offsets, lengths in special_entries:
        packed_offsets = ",".join(str(v) for v in offsets)
        packed_lengths = ",".join(str(v) for v in lengths)
        out.append(f"    {{{{{packed_offsets}}},{{{packed_lengths}}}}},")
    out.append("};")
    out.append("")

    emit_array(out, "uint32_t", "UNI_SPECIAL_CPS", special_cps, 12)
    emit_array(out, index_type(mid_count), "UNI_STAGE1", stage1, 24)
    emit_array(out, index_type(sub_count), "UNI_STAGE2", stage2, 24)
    emit_array(out, index_type(len(order)), "UNI_STAGE3", stage3, 24)

    out.append("#endif")
    out.append("")

    target.write_text("\n".join(out), encoding="utf-8")

    special_bytes = len(special_entries) * 12 + len(special_cps) * 4
    record_bytes = len(order) * 24

    print(f"unicode {unicodedata.unidata_version}")
    print(f"records {len(order)} ({record_bytes} bytes)")
    print(f"specials {len(special_entries)} ({special_bytes} bytes)")
    print(f"shift2 {shift2} shift3 {shift3} mid {mid_count} sub {sub_count}")
    print(
        f"stage1 {len(stage1) * index_size(mid_count)} "
        f"stage2 {len(stage2) * index_size(sub_count)} "
        f"stage3 {len(stage3) * index_size(len(order))}"
    )
    print(f"table total {total + record_bytes + special_bytes} bytes")
    print(f"wrote {target}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
