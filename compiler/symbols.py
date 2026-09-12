from __future__ import annotations

from dataclasses import dataclass, field

MAX_LOCALS = 256
MAX_GLOBALS = 65535
MAX_NAME_LEN = 255

class SymbolError(Exception):
    pass

@dataclass
class LocalTable:
    name: str
    _by_name: Dict[str, int] = field(default_factory=dict)
    _ordered: List[str] = field(default_factory=list)

    def declare(self, name: str) -> int:
        if name in self._by_name:
            return self._by_name[name]
        if len(self._ordered) >= MAX_LOCALS:
            raise SymbolError(f"function {self.name!r} exceeds {MAX_LOCALS} locals")
        if len(name.encode("utf-8")) > MAX_NAME_LEN:
            raise SymbolError(f"local name too long: {name!r}")
        idx = len(self._ordered)
        self._by_name[name] = idx
        self._ordered.append(name)
        return idx

    def index(self, name: str) -> int:
        if name not in self._by_name:
            raise SymbolError(f"undefined local {name!r} in {self.name!r}")
        return self._by_name[name]

    def has(self, name: str) -> bool:
        return name in self._by_name

    def count(self) -> int:
        return len(self._ordered)

    def names(self) -> List[str]:
        return list(self._ordered)


@dataclass
class GlobalTable:
    _by_name: Dict[str, int] = field(default_factory=dict)
    _ordered: List[str] = field(default_factory=list)

    def intern(self, name: str) -> int:
        if name in self._by_name:
            return self._by_name[name]
        if len(self._ordered) >= MAX_GLOBALS:
            raise SymbolError(f"global name pool exceeds {MAX_GLOBALS} entries")
        if len(name.encode("utf-8")) > MAX_NAME_LEN:
            raise SymbolError(f"global name too long: {name!r}")
        idx = len(self._ordered)
        self._by_name[name] = idx
        self._ordered.append(name)
        return idx

    def index(self, name: str) -> int:
        if name not in self._by_name:
            raise SymbolError(f"undefined global {name!r}")
        return self._by_name[name]

    def has(self, name: str) -> bool:
        return name in self._by_name

    def count(self) -> int:
        return len(self._ordered)

    def names(self) -> List[str]:
        return list(self._ordered)

GlobalNames = GlobalTable

def serialize_names(names: List[str]) -> bytes:
    import struct
    out = bytearray(struct.pack("<I", len(names)))
    for n in names:
        data = n.encode("utf-8")
        if len(data) > MAX_NAME_LEN:
            raise SymbolError(f"name too long: {n!r}")
        out += struct.pack("<B", len(data)) + data
    return bytes(out)
