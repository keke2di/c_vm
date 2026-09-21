from __future__ import annotations

from typing import List

from .container import build_module as _build_container
from .constants import ConstantPool
from .symbols import GlobalNames


def build_module(
    functions: List,
    constants: ConstantPool,
    names: GlobalNames,
    entry_function: str = "__main__",
    source_name: str = "<module>",
) -> bytes:
    """
    Build the plaintext module bytes.
    Wraps container.build_module with the entry function name.
    """
    return _build_container(functions, constants, names, entry_function, source_name)
