from __future__ import annotations

CANONICAL_OPCODES = {
    "NOP":              0x00,
    "LOAD_CONST":       0x01,
    "LOAD_FAST":        0x02,
    "STORE_FAST":       0x03,
    "LOAD_GLOBAL":      0x04,
    "STORE_GLOBAL":     0x05,
    "POP_TOP":          0x06,
    "DUP_TOP":          0x07,
    "ROT_TWO":          0x08,

    "BINARY_ADD":       0x10,
    "BINARY_SUB":       0x11,
    "BINARY_MUL":       0x12,
    "BINARY_DIV":       0x13,
    "BINARY_MOD":       0x14,
    "BINARY_POW":       0x15,
    "BINARY_FLOORDIV":  0x16,
    "UNARY_NEG":        0x17,
    "UNARY_NOT":        0x18,
    "UNARY_POS":        0x19,
    "UNARY_INVERT":     0x1A,

    "COMPARE_EQ":       0x20,
    "COMPARE_NE":       0x21,
    "COMPARE_LT":       0x22,
    "COMPARE_LE":       0x23,
    "COMPARE_GT":       0x24,
    "COMPARE_GE":       0x25,
    "CONTAINS":         0x26,

    "JUMP":             0x30,
    "JUMP_IF_FALSE":    0x31,
    "JUMP_IF_TRUE":     0x32,

    "CALL":             0x40,
    "RETURN":           0x41,
    "CALL_KW":          0x42,

    "BUILD_LIST":       0x50,
    "BUILD_TUPLE":      0x51,
    "BUILD_MAP":        0x52,
    "BUILD_SET":        0x57,
    "GET_INDEX":        0x53,
    "SET_INDEX":        0x54,
    "GET_ITER_ITEM":    0x5B,
    "GET_SLICE":        0x5A,
    "LIST_APPEND":      0x55,
    "SET_ADD":          0x58,
    "MAP_ADD":          0x59,
    "LEN":              0x56,

    "PRINT":            0x60,
    "CALL_BUILTIN":     0x61,
    "CALL_METHOD":      0x62,

    "HALT":             0xFF,
}

ALIASES = {
    "RETURN_VALUE":       0x41,
    "POP_JUMP_IF_FALSE":  0x31,
    "POP_JUMP_IF_TRUE":   0x32,
    "JUMP_ABSOLUTE":      0x30,
    "CALL_FUNCTION":      0x40,
}

OPCODES = {**CANONICAL_OPCODES, **ALIASES}

OPERAND_WIDTHS = {
    "LOAD_CONST":       4,
    "LOAD_FAST":        4,
    "STORE_FAST":       4,
    "LOAD_GLOBAL":      4,
    "STORE_GLOBAL":     4,
    "JUMP":             4,
    "JUMP_IF_FALSE":    4,
    "JUMP_IF_TRUE":     4,
    "CALL":             4,
    "CALL_KW":          4,
    "CALL_BUILTIN":     4,
    "BUILD_LIST":       4,
    "BUILD_TUPLE":      4,
    "BUILD_MAP":        4,
    "BUILD_SET":        4,
    "PRINT":            4,
    "CALL_METHOD":      4,
    "POP_JUMP_IF_FALSE":4,
    "POP_JUMP_IF_TRUE": 4,
    "JUMP_ABSOLUTE":    4,
    "CALL_FUNCTION":    4,
}

OPCODE_WIDTH_BY_CODE = {
    code: OPERAND_WIDTHS.get(name, 0)
    for name, code in OPCODES.items()
}

def operand_width_for_code(op: int) -> int:
    return OPCODE_WIDTH_BY_CODE.get(op, 0)

OPERAND_WIDTH = operand_width_for_code

OPCODE_TABLE_VERSION = 3

_globals = globals()
for _name, _value in OPCODES.items():
    _globals["OP_" + _name] = _value

NAME_BY_CODE = {v: k for k, v in CANONICAL_OPCODES.items()}

def operand_width(name: str) -> int:
    return OPERAND_WIDTHS.get(name, 0)

def instruction_size(name: str) -> int:
    return 1 + operand_width(name)

def to_c_header() -> str:
    lines = [
        "#ifndef CVM_OPCODES_H",
        "#define CVM_OPCODES_H",
        "",
        f"#define CVM_OPCODE_TABLE_VERSION {OPCODE_TABLE_VERSION}",
        "",
        "typedef enum {",
    ]
    for name, code in CANONICAL_OPCODES.items():
        lines.append(f"    OP_{name} = 0x{code:02X},")
    lines += [
        "} cvm_opcode_t;",
        "",
        "static inline int cvm_operand_width(cvm_opcode_t op) {",
        "    switch (op) {",
    ]
    for name, code in CANONICAL_OPCODES.items():
        width = OPERAND_WIDTHS.get(name, 0)
        if width:
            lines.append(f"        case OP_{name}: return {width};")
    lines += [
        "        default: return 0;",
        "    }",
        "}",
        "",
        "#endif",
        "",
    ]
    return "\n".join(lines)

if __name__ == "__main__":
    import sys, pathlib
    out = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else pathlib.Path("vm_c/opcodes.h")
    out.write_text(to_c_header())
    print(f"wrote {out}")
