from .compiler import CompileError, compile_source, Compiler, CompiledModule
from .constants import ConstantPool
from .crypto import FORMAT_VERSION, MAGIC, encode_container
from .module import build_module
from .symbols import GlobalNames, LocalTable, SymbolError

__all__ = [
    "CompileError",
    "compile_source",
    "Compiler",
    "CompiledModule",
    "ConstantPool",
    "FORMAT_VERSION",
    "MAGIC",
    "encode_container",
    "build_module",
    "GlobalNames",
    "LocalTable",
    "SymbolError",
]

__version__ = "0.3.0"