from compiler.compiler import Compiler
from pathlib import Path
from compiler.opcodes import NAME_BY_CODE, OPERAND_WIDTH

source = Path("examples/test_nested_function_calls.py").read_text()
module = Compiler().compile_module(source)

for fn in module.functions:
    print(f"\n=== {fn.name} ===")
    code = fn.code
    ip = 0
    while ip < len(code):
        op = code[ip]
        name = NAME_BY_CODE.get(op, f"UNKNOWN({op:#x})")
        width = OPERAND_WIDTH(op)
        operand = ""
        if width:
            operand = f" {int.from_bytes(code[ip+1:ip+1+width], 'little')}"
        print(f"{ip:03}: {name}{operand}")
        ip += 1 + width
