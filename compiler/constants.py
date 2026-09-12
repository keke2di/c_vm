from __future__ import annotations

import struct
from typing import List, Tuple, Union

Const = Union[None, bool, int, float, str, bytes, Tuple["Const", ...]]

TAG_NONE = 0x00
TAG_FALSE = 0x01
TAG_TRUE = 0x02
TAG_INT64 = 0x03
TAG_FLOAT64 = 0x04
TAG_STR = 0x05
TAG_BYTES = 0x06
TAG_TUPLE = 0x07

MAX_STR_LEN = 1 << 20
MAX_BYTES_LEN = 1 << 20
MAX_TUPLE_LEN = 1 << 16
MAX_POOL_LEN = 1 << 16

class ConstantPoolError(ValueError):
    pass

class ConstantPool:
    def __init__(self) -> None:
        self._items: List[Const] = []
        self._index: dict[Tuple[int, bytes], int] = {}

    def __len__(self) -> int:
        return len(self._items)

    def items(self) -> List[Const]:
        return list(self._items)

    def add(self, value: Const) -> int:
        key = self._key(value)
        existing = self._index.get(key)
        if existing is not None:
            return existing
        if len(self._items) >= MAX_POOL_LEN:
            raise ConstantPoolError("constant pool overflow")
        idx = len(self._items)
        self._items.append(value)
        self._index[key] = idx
        return idx

    def _key(self, value: Const) -> Tuple[int, bytes]:
        return (self._type_id(value), _encode_value(value))

    @staticmethod
    def _type_id(value: Const) -> int:
        if value is None:
            return TAG_NONE
        if isinstance(value, bool):
            return TAG_TRUE if value else TAG_FALSE
        if isinstance(value, int):
            return TAG_INT64
        if isinstance(value, float):
            return TAG_FLOAT64
        if isinstance(value, str):
            return TAG_STR
        if isinstance(value, bytes):
            return TAG_BYTES
        if isinstance(value, tuple):
            return TAG_TUPLE
        raise ConstantPoolError(f"unsupported constant type: {type(value).__name__}")

    def serialize(self) -> bytes:
        out = bytearray()
        out += struct.pack("<I", len(self._items))
        for v in self._items:
            out += _encode_tagged(v)
        return bytes(out)

def _encode_tagged(value: Const) -> bytes:
    return bytes([ConstantPool._type_id(value)]) + _encode_value(value)

def _encode_value(value: Const) -> bytes:
    if value is None or isinstance(value, bool):
        return b""
    if isinstance(value, int):
        if not (-(1 << 63) <= value < (1 << 63)):
            raise ConstantPoolError("int constant out of int64 range")
        return struct.pack("<q", value)
    if isinstance(value, float):
        return struct.pack("<d", value)
    if isinstance(value, str):
        data = value.encode("utf-8")
        if len(data) > MAX_STR_LEN:
            raise ConstantPoolError("string constant too large")
        return struct.pack("<I", len(data)) + data
    if isinstance(value, bytes):
        if len(value) > MAX_BYTES_LEN:
            raise ConstantPoolError("bytes constant too large")
        return struct.pack("<I", len(value)) + value
    if isinstance(value, tuple):
        if len(value) > MAX_TUPLE_LEN:
            raise ConstantPoolError("tuple constant too large")
        payload = bytearray(struct.pack("<I", len(value)))
        for item in value:
            payload += _encode_tagged(item)
        return bytes(payload)
    raise ConstantPoolError(f"unsupported constant type: {type(value).__name__}")
