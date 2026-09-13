import sys
from pathlib import Path

from compiler.compiler import Compiler
from compiler.opcodes import NAME_BY_CODE, OPERAND_WIDTH


def main(argv):
    if len(argv) != 2:
        print("usage: python debug_disasm.py <source.py>")
        return 2

    source_path = Path(argv[1])
    source = source_path.read_text(encoding="utf-8")
    module = Compiler().compile_module(source, str(source_path))

    for fn in module.functions:
        print(f"\n=== {fn.name}({', '.join(fn.param_names)}) ===")
        code = fn.code
        ip = 0
        while ip < len(code):
            op = code[ip]
            name = NAME_BY_CODE.get(op, f"UNKNOWN({op:#x})")
            width = OPERAND_WIDTH(op)
            operand = ""
            if width:
                operand = f" {int.from_bytes(code[ip + 1:ip + 1 + width], 'little')}"
            print(f"{ip:03}: {name}{operand}")
            ip += 1 + width

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
