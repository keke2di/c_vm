from __future__ import annotations

import struct

from .constants import ConstantPool
from .symbols import GlobalNames, serialize_names

MAX_FUNCTIONS = 4096
MAX_SECTION_LEN = 1 << 24


class ContainerError(Exception):
    pass


def build_module(
    functions: list,
    constants: ConstantPool,
    names: GlobalNames,
    entry_function: str,
) -> bytes:
    entry_idx = None

    for i, fn in enumerate(functions):
        names.intern(fn.name)

        if fn.name == entry_function:
            entry_idx = i

    if entry_idx is None:
        raise ContainerError(f"entry function {entry_function!r} not found")

    const_data = constants.serialize()
    name_data = serialize_names(names.names())
    func_data = _serialize_functions(functions, names)

    out = bytearray()
    out += struct.pack("<I", entry_idx)
    out += _section(0x01, const_data)
    out += _section(0x02, name_data)
    out += _section(0x03, func_data)

    return bytes(out)


def _section(tag: int, payload: bytes) -> bytes:
    if len(payload) > MAX_SECTION_LEN:
        raise ContainerError(f"section {tag:#x} exceeds max length")

    return struct.pack("<BI", tag, len(payload)) + payload


def _serialize_functions(functions: list, names: GlobalNames) -> bytes:
    if len(functions) > MAX_FUNCTIONS:
        raise ContainerError("too many functions")

    out = bytearray()
    out += struct.pack("<I", len(functions))

    for fn in functions:
        name_idx = names.intern(fn.name)

        if fn.num_locals > 0xFFFF or fn.num_params > 0xFFFF:
            raise ContainerError(
                f"function {fn.name!r} exceeds local/param limits"
            )

        if fn.num_params > fn.num_locals:
            raise ContainerError(
                f"function {fn.name!r} has more params than locals"
            )

        if len(fn.code) > MAX_SECTION_LEN:
            raise ContainerError(
                f"function {fn.name!r} code too large"
            )

        out += struct.pack("<I", name_idx)
        out += struct.pack("<H", fn.num_locals)
        out += struct.pack("<H", fn.num_params)
        out += struct.pack("<I", len(fn.code))
        out += fn.code

    return bytes(out)