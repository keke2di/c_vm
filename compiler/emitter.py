from __future__ import annotations

import struct
from dataclasses import dataclass, field
from typing import Dict, List, Optional

from .opcodes import OPCODES, OPERAND_WIDTH

MAX_CODE_LEN = 1 << 20

class EmitterError(Exception):
    pass

@dataclass
class _Fixup:
    offset: int
    label: str
    width: int

@dataclass
class Emitter:
    name: str
    code: bytearray = field(default_factory=bytearray)
    labels: Dict[str, int] = field(default_factory=dict)
    fixups: List[_Fixup] = field(default_factory=list)
    label_counters: Dict[str, int] = field(default_factory=dict)

    def pos(self) -> int:
        return len(self.code)

    def label(self, name: str) -> None:
        if name in self.labels:
            raise EmitterError(f"duplicate label {name!r} in {self.name}")
        self.labels[name] = len(self.code)

    def new_label(self, hint: str = "L") -> str:
        i = self.label_counters.get(hint, 0)

        while True:
            candidate = f"{hint}_{i}"
            i += 1

            if candidate not in self.labels and not any(
                f.label == candidate for f in self.fixups
            ):
                self.label_counters[hint] = i
                return candidate

    def emit(self, mnemonic: str, operand: Optional[int] = None) -> None:
        if mnemonic not in OPCODES:
            raise EmitterError(f"unknown opcode {mnemonic!r}")
        op = OPCODES[mnemonic]
        width = OPERAND_WIDTH(op)
        self.code.append(op)
        if width == 0:
            if operand is not None:
                raise EmitterError(f"{mnemonic} takes no operand")
            return
        if operand is None:
            raise EmitterError(f"{mnemonic} requires an operand")
        self._emit_uint(operand, width)

    def emit_jump(self, mnemonic: str, label: str) -> None:
        if mnemonic not in OPCODES:
            raise EmitterError(f"unknown opcode {mnemonic!r}")
        op = OPCODES[mnemonic]
        width = OPERAND_WIDTH(op)
        if width not in (2, 4):
            raise EmitterError(f"{mnemonic} is not a jump opcode")
        self.code.append(op)
        self.fixups.append(_Fixup(offset=len(self.code), label=label, width=width))
        self.code.extend(b"\x00" * width)

    def _emit_uint(self, value: int, width: int) -> None:
        if value < 0:
            raise EmitterError(f"operand must be unsigned, got {value}")
        if width == 1:
            if value > 0xFF:
                raise EmitterError("operand overflow (u8)")
            self.code.append(value)
        elif width == 2:
            if value > 0xFFFF:
                raise EmitterError("operand overflow (u16)")
            self.code += struct.pack("<H", value)
        elif width == 4:
            if value > 0xFFFFFFFF:
                raise EmitterError("operand overflow (u32)")
            self.code += struct.pack("<I", value)
        else:
            raise EmitterError(f"unsupported operand width {width}")

    def finalize(self) -> bytes:
        for fx in self.fixups:
            if fx.label not in self.labels:
                raise EmitterError(f"unresolved label {fx.label!r} in {self.name}")
            target = self.labels[fx.label]
            if target > len(self.code):
                raise EmitterError(f"label {fx.label!r} points past end of code")
            if fx.width == 2:
                if target > 0xFFFF:
                    raise EmitterError("jump target exceeds u16")
                struct.pack_into("<H", self.code, fx.offset, target)
            elif fx.width == 4:
                if target > 0xFFFFFFFF:
                    raise EmitterError("jump target exceeds u32")
                struct.pack_into("<I", self.code, fx.offset, target)
        if len(self.code) > MAX_CODE_LEN:
            raise EmitterError(f"function {self.name!r} exceeds max code length")
        return bytes(self.code)
