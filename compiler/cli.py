from __future__ import annotations

import argparse
import sys
from pathlib import Path

from .compiler import CompileError, compile_source
from .constants import ConstantPoolError
from .container import ContainerError
from .crypto import encode_container
from .emitter import EmitterError
from .module import build_module
from .symbols import SymbolError


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        prog="cvmc",
        description="Compile Python source to a CVM2 .cvm module",
    )
    parser.add_argument("input", help="Python source file to compile")
    parser.add_argument(
        "-o",
        "--output",
        help="Output .cvm path (default: <input>.cvm)",
    )
    args = parser.parse_args(argv)

    src_path = Path(args.input)
    if not src_path.is_file():
        print(f"error: no such file: {src_path}", file=sys.stderr)
        return 2

    out_path = Path(args.output) if args.output else src_path.with_suffix(".cvm")
    out_path.parent.mkdir(parents=True, exist_ok=True)

    try:
        source = src_path.read_text(encoding="utf-8")
        compiled = compile_source(source, str(src_path))
        plain = build_module(
            compiled.functions,
            compiled.constants,
            compiled.names,
            entry_function="__main__",
            source_name=compiled.source_name,
        )
        container = encode_container(plain, pepper_id=1, flags=0)
    except (
        SyntaxError,
        CompileError,
        EmitterError,
        SymbolError,
        ContainerError,
        ConstantPoolError,
    ) as e:
        print(f"compile error: {e}", file=sys.stderr)
        return 1

    out_path.write_bytes(container)
    print(f"compiled {src_path} -> {out_path} ({len(container)} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
