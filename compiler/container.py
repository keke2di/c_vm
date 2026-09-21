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
    source_name: str = "<module>",
) -> bytes:
    entry_idx = None

    for i, fn in enumerate(functions):
        names.intern(fn.name)
        for param_name in fn.param_names + fn.kwonly_names + fn.free_names:
            names.intern(param_name)

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
    out += _section(0x04, source_name.encode("utf-8"))

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

        if len(fn.param_names) != fn.num_params:
            raise ContainerError(
                f"function {fn.name!r} parameter names do not match parameter count"
            )

        if fn.num_defaults > fn.num_params:
            raise ContainerError(
                f"function {fn.name!r} has more defaults than params"
            )

        if len(fn.kwonly_names) != len(fn.kwonly_flags):
            raise ContainerError(
                f"function {fn.name!r} keyword-only table mismatch"
            )

        out += struct.pack("<I", name_idx)
        out += struct.pack("<H", fn.num_locals)
        out += struct.pack("<H", fn.num_params)
        out += struct.pack("<H", fn.posonly_count)
        out += struct.pack("<H", fn.num_defaults)
        out += struct.pack("<H", len(fn.kwonly_names))
        out += struct.pack("<H", fn.vararg_slot)
        out += struct.pack("<H", fn.kwarg_slot)
        out += struct.pack("<H", len(fn.cell_slots))
        out += struct.pack("<H", len(fn.free_names))

        for param_name in fn.param_names:
            out += struct.pack("<I", names.index(param_name))

        for name in fn.kwonly_names:
            out += struct.pack("<I", names.index(name))

        for slot in fn.kwonly_slots:
            out += struct.pack("<H", slot)

        for flag in fn.kwonly_flags:
            out += struct.pack("<B", flag)

        for slot in fn.cell_slots:
            out += struct.pack("<H", slot)

        for name in fn.free_names:
            out += struct.pack("<I", names.index(name))

        out += struct.pack("<I", len(fn.line_table))
        for offset, line in fn.line_table:
            out += struct.pack("<II", offset, line)

        out += struct.pack("<I", len(fn.code))
        out += fn.code

    return bytes(out)
