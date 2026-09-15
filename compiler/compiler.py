from __future__ import annotations

import ast
import sys
from dataclasses import dataclass, field
from typing import Dict, List, Optional

from .constants import ConstantPool
from .emitter import Emitter, EmitterError
from .symbols import GlobalNames, LocalTable, SymbolError

REQUIRED_PYTHON = (3, 14)

class CompileError(Exception):
    pass

@dataclass
class CompiledFunction:
    name: str
    num_locals: int
    num_params: int
    code: bytes
    param_names: List[str] = field(default_factory=list)
    default_consts: List[int] = field(default_factory=list)

@dataclass
class CompiledModule:
    constants: ConstantPool
    names: GlobalNames
    functions: List[CompiledFunction] = field(default_factory=list)
    entry_index: int = 0

class Compiler:
    def __init__(self) -> None:
        self.constants = ConstantPool()
        self.names = GlobalNames()
        self.functions: List[CompiledFunction] = []
        self._func_index: Dict[str, int] = {}
        self._defined_functions: set[str] = set()
        self._rebound_names: set[str] = set()

    def compile_module(self, source: str, filename: str = "<module>") -> CompiledModule:
        if tuple(sys.version_info[:2]) != REQUIRED_PYTHON:
            raise CompileError(
                f"cVM requires Python {REQUIRED_PYTHON[0]}.{REQUIRED_PYTHON[1]}, "
                f"running {sys.version_info[0]}.{sys.version_info[1]}"
            )
        tree = ast.parse(source, filename=filename)
        self._defined_functions = {
            node.name for node in tree.body if isinstance(node, ast.FunctionDef)
        }
        self._rebound_names = {
            node.id
            for node in ast.walk(tree)
            if isinstance(node, ast.Name) and not isinstance(node.ctx, ast.Load)
        }
        self._rebound_names.update(
            arg.arg
            for node in ast.walk(tree)
            if isinstance(node, ast.arguments)
            for arg in node.posonlyargs + node.args + node.kwonlyargs
        )
        module_body: List[ast.stmt] = []
        for node in tree.body:
            if isinstance(node, ast.FunctionDef):
                self._compile_function(node)
            else:
                module_body.append(node)
        entry = ast.FunctionDef(
            name="__main__",
            args=ast.arguments(posonlyargs=[], args=[], vararg=None, kwonlyargs=[], kw_defaults=[], kwarg=None, defaults=[]),
            body=module_body or [ast.Pass()],
            decorator_list=[],
            returns=None,
            type_comment=None,
        )
        ast.fix_missing_locations(entry)
        self._compile_function(entry)
        module = CompiledModule(constants=self.constants, names=self.names, functions=self.functions)
        module.entry_index = self._func_index["__main__"]
        return module

    def _compile_function(self, node: ast.FunctionDef) -> None:
        if node.name in self._func_index:
            raise CompileError(f"duplicate function: {node.name}")
        args = node.args
        if node.decorator_list:
            raise CompileError("decorators are not supported yet")
        if getattr(node, "type_params", None):
            raise CompileError("type parameters are not supported")
        if args.vararg is not None:
            raise CompileError("*args parameters are not supported yet")
        if args.kwarg is not None:
            raise CompileError("**kwargs parameters are not supported yet")
        if args.kwonlyargs:
            raise CompileError("keyword-only parameters are not supported yet")
        if args.posonlyargs:
            raise CompileError("positional-only parameters are not supported yet")
        param_names = [arg.arg for arg in args.args]
        if len(set(param_names)) != len(param_names):
            raise CompileError(f"duplicate parameter name in function {node.name}")
        default_consts = [
            self.constants.add(_constant_default(default))
            for default in args.defaults
        ]
        locals_tbl = LocalTable(node.name)
        for param_name in param_names:
            locals_tbl.declare(param_name)
        emitter = Emitter(node.name)
        ctx = _FuncCtx(
            emitter=emitter,
            locals=locals_tbl,
            compiler=self,
            loop_stack=[],
            is_module=(node.name == "__main__"),
        )
        for stmt in node.body:
            _compile_stmt(ctx, stmt)
        emitter.emit("LOAD_CONST", self.constants.add(None))
        emitter.emit("RETURN_VALUE")
        code = emitter.finalize()
        idx = len(self.functions)
        self.functions.append(CompiledFunction(
            name=node.name,
            num_locals=locals_tbl.count(),
            num_params=len(param_names),
            code=code,
            param_names=param_names,
            default_consts=default_consts,
        ))
        self._func_index[node.name] = idx

@dataclass
class _FuncCtx:
    emitter: Emitter
    locals: LocalTable
    compiler: "Compiler"
    loop_stack: list
    is_module: bool = False
    iter_depth: int = 0

def _compile_stmt(ctx: _FuncCtx, node: ast.stmt) -> None:
    if isinstance(node, ast.Assign):
        targets = node.targets
        if len(targets) == 1:
            _assign_single(ctx, targets[0], node.value)
            return
        _compile_expr(ctx, node.value)
        for i, target in enumerate(targets):
            if i < len(targets) - 1:
                ctx.emitter.emit("DUP_TOP")
            _assign_to(ctx, target)
        return
    if isinstance(node, ast.AugAssign):
        op = _BINOPS.get(type(node.op))
        if op is None:
            raise CompileError(f"unsupported augmented assignment operator: {type(node.op).__name__}")
        target = node.target
        if isinstance(target, ast.Name):
            name = target.id
            if name in ctx.locals._by_name:
                ctx.emitter.emit("LOAD_FAST", ctx.locals.index(name))
            else:
                ctx.emitter.emit("LOAD_GLOBAL", ctx.compiler.names.intern(name))
            _compile_expr(ctx, node.value)
            ctx.emitter.emit(op)
            _store_name(ctx, name)
        elif isinstance(target, ast.Subscript):
            _compile_expr(ctx, target.value)
            _compile_expr(ctx, target.slice)
            ctx.emitter.emit("DUP_TOP_TWO")
            ctx.emitter.emit("GET_INDEX")
            _compile_expr(ctx, node.value)
            ctx.emitter.emit(op)
            ctx.emitter.emit("SET_INDEX")
        else:
            raise CompileError("augmented assignment target must be a name or subscript")
        return
    if isinstance(node, ast.Delete):
        for target in node.targets:
            _compile_delete(ctx, target)
        return
    if isinstance(node, ast.Expr):
        _compile_expr(ctx, node.value)
        ctx.emitter.emit("POP_TOP")
        return
    if isinstance(node, ast.Return):
        if ctx.is_module:
            raise CompileError("'return' outside function")
        if node.value is None:
            ctx.emitter.emit("LOAD_CONST", ctx.compiler.constants.add(None))
        else:
            _compile_expr(ctx, node.value)
        ctx.emitter.emit("RETURN_VALUE")
        return
    if isinstance(node, ast.If):
        else_label = ctx.emitter.new_label("if_else")
        end_label = ctx.emitter.new_label("if_end")
        _compile_expr(ctx, node.test)
        ctx.emitter.emit_jump("POP_JUMP_IF_FALSE", else_label)
        for s in node.body:
            _compile_stmt(ctx, s)
        ctx.emitter.emit_jump("JUMP_ABSOLUTE", end_label)
        ctx.emitter.label(else_label)
        for s in node.orelse:
            _compile_stmt(ctx, s)
        ctx.emitter.label(end_label)
        return

    if isinstance(node, ast.While):
        start_label = ctx.emitter.new_label("while_start")
        else_label = ctx.emitter.new_label("while_else")
        end_label = ctx.emitter.new_label("while_end")
        ctx.emitter.label(start_label)
        _compile_expr(ctx, node.test)
        ctx.emitter.emit_jump("POP_JUMP_IF_FALSE", else_label)
        ctx.loop_stack.append((start_label, end_label))
        for s in node.body:
            _compile_stmt(ctx, s)
        ctx.loop_stack.pop()
        ctx.emitter.emit_jump("JUMP_ABSOLUTE", start_label)
        ctx.emitter.label(else_label)
        for s in node.orelse:
            _compile_stmt(ctx, s)
        ctx.emitter.label(end_label)
        return

    if isinstance(node, ast.For):
        target = node.target
        if not isinstance(target, (ast.Name, ast.Tuple, ast.List)):
            raise CompileError("for loop target must be a name or a tuple/list of names")

        ctx.iter_depth += 1
        iter_idx = ctx.locals.declare(f"__iter_{ctx.iter_depth}")

        _compile_expr(ctx, node.iter)
        ctx.emitter.emit("GET_ITER")
        ctx.emitter.emit("STORE_FAST", iter_idx)

        start_label = ctx.emitter.new_label("for_start")
        else_label = ctx.emitter.new_label("for_else")
        end_label = ctx.emitter.new_label("for_end")

        ctx.emitter.label(start_label)

        ctx.emitter.emit("LOAD_FAST", iter_idx)
        ctx.emitter.emit_jump("FOR_ITER", else_label)

        _assign_to(ctx, target)

        ctx.loop_stack.append((start_label, end_label))

        for s in node.body:
            _compile_stmt(ctx, s)

        ctx.loop_stack.pop()

        ctx.emitter.emit_jump("JUMP_ABSOLUTE", start_label)

        ctx.emitter.label(else_label)

        for s in node.orelse:
            _compile_stmt(ctx, s)

        ctx.emitter.label(end_label)
        ctx.iter_depth -= 1

        return

    if isinstance(node, ast.Break):
        if not ctx.loop_stack:
            raise CompileError("break outside loop")
        ctx.emitter.emit_jump("JUMP_ABSOLUTE", ctx.loop_stack[-1][1])
        return
    if isinstance(node, ast.Continue):
        if not ctx.loop_stack:
            raise CompileError("continue outside loop")
        ctx.emitter.emit_jump("JUMP_ABSOLUTE", ctx.loop_stack[-1][0])
        return
    if isinstance(node, ast.Pass):
        ctx.emitter.emit("NOP")
        return
    raise CompileError(f"unsupported statement: {type(node).__name__}")

def _compile_expr(ctx: _FuncCtx, node: ast.expr) -> None:
    if isinstance(node, ast.Constant):
        ctx.emitter.emit("LOAD_CONST", ctx.compiler.constants.add(node.value))
        return
    if isinstance(node, ast.Name):
        if node.id in ctx.locals._by_name:
            ctx.emitter.emit("LOAD_FAST", ctx.locals.index(node.id))
        else:
            ctx.emitter.emit("LOAD_GLOBAL", ctx.compiler.names.intern(node.id))
        return
    if isinstance(node, ast.JoinedStr):
        _compile_joined_str(ctx, node)
        return
    if isinstance(node, ast.FormattedValue):
        _compile_formatted_value(ctx, node)
        return
    if isinstance(node, ast.BinOp):
        _compile_expr(ctx, node.left)
        _compile_expr(ctx, node.right)
        op = _BINOPS.get(type(node.op))
        if op is None:
            raise CompileError(f"unsupported binop: {type(node.op).__name__}")
        ctx.emitter.emit(op)
        return
    if isinstance(node, ast.Compare):
        count = len(node.ops)
        _compile_expr(ctx, node.left)
        if count == 1:
            _compile_expr(ctx, node.comparators[0])
            _emit_compare_link(ctx, node.ops[0])
            return
        fail_label = ctx.emitter.new_label("cmp_fail")
        end_label = ctx.emitter.new_label("cmp_end")
        for i in range(count):
            _compile_expr(ctx, node.comparators[i])
            if i < count - 1:
                ctx.emitter.emit("DUP_TOP")
                ctx.emitter.emit("ROT_THREE")
            _emit_compare_link(ctx, node.ops[i])
            if i < count - 1:
                ctx.emitter.emit("DUP_TOP")
                ctx.emitter.emit_jump("POP_JUMP_IF_FALSE", fail_label)
                ctx.emitter.emit("POP_TOP")
        ctx.emitter.emit_jump("JUMP_ABSOLUTE", end_label)
        ctx.emitter.label(fail_label)
        ctx.emitter.emit("ROT_TWO")
        ctx.emitter.emit("POP_TOP")
        ctx.emitter.label(end_label)
        return
    if isinstance(node, ast.UnaryOp):
        _compile_expr(ctx, node.operand)
        op = _UNOPS.get(type(node.op))
        if op is None:
            raise CompileError(f"unsupported unary: {type(node.op).__name__}")
        ctx.emitter.emit(op)
        return
    if isinstance(node, ast.BoolOp):
        is_and = isinstance(node.op, ast.And)
        values = node.values
        if len(values) < 2:
            _compile_expr(ctx, values[0])
            return
        end_label = ctx.emitter.new_label("bool_end")
        for i, val in enumerate(values[:-1]):
            _compile_expr(ctx, val)
            ctx.emitter.emit("DUP_TOP")
            if is_and:
                ctx.emitter.emit_jump("POP_JUMP_IF_FALSE", end_label)
            else:
                ctx.emitter.emit_jump("POP_JUMP_IF_TRUE", end_label)
            ctx.emitter.emit("POP_TOP")
        _compile_expr(ctx, values[-1])
        ctx.emitter.emit_jump("JUMP_ABSOLUTE", end_label)
        ctx.emitter.label(end_label)
        return
    if isinstance(node, ast.IfExp):
        else_label = ctx.emitter.new_label("ifexp_else")
        end_label = ctx.emitter.new_label("ifexp_end")
        _compile_expr(ctx, node.test)
        ctx.emitter.emit_jump("POP_JUMP_IF_FALSE", else_label)
        _compile_expr(ctx, node.body)
        ctx.emitter.emit_jump("JUMP_ABSOLUTE", end_label)
        ctx.emitter.label(else_label)
        _compile_expr(ctx, node.orelse)
        ctx.emitter.label(end_label)
        return
    if isinstance(node, ast.List):
        for elt in node.elts:
            _compile_expr(ctx, elt)
        ctx.emitter.emit("BUILD_LIST", len(node.elts))
        return
    if isinstance(node, ast.Tuple):
        for elt in node.elts:
            _compile_expr(ctx, elt)
        ctx.emitter.emit("BUILD_TUPLE", len(node.elts))
        return
    if isinstance(node, ast.Set):
        for elt in node.elts:
            _compile_expr(ctx, elt)
        ctx.emitter.emit("BUILD_SET", len(node.elts))
        return
    if isinstance(node, ast.Dict):
        if any(key is None for key in node.keys):
            raise CompileError("dict unpacking with ** is not supported yet")
        for key, value in zip(node.keys, node.values):
            _compile_expr(ctx, key)
            _compile_expr(ctx, value)
        ctx.emitter.emit("BUILD_MAP", len(node.keys))
        return
    if isinstance(node, (ast.ListComp, ast.SetComp, ast.DictComp)):
        _compile_comprehension(ctx, node)
        return

    if isinstance(node, ast.Subscript):
        if not isinstance(node.ctx, ast.Load):
            raise CompileError("Subscript in expression must be Load context")

        _compile_expr(ctx, node.value)

        if isinstance(node.slice, ast.Slice):
            if node.slice.lower is None:
                ctx.emitter.emit("LOAD_CONST", ctx.compiler.constants.add(None))
            else:
                _compile_expr(ctx, node.slice.lower)

            if node.slice.upper is None:
                ctx.emitter.emit("LOAD_CONST", ctx.compiler.constants.add(None))
            else:
                _compile_expr(ctx, node.slice.upper)

            if node.slice.step is None:
                ctx.emitter.emit("LOAD_CONST", ctx.compiler.constants.add(None))
            else:
                _compile_expr(ctx, node.slice.step)

            ctx.emitter.emit("GET_SLICE")
            return

        _compile_expr(ctx, node.slice)
        ctx.emitter.emit("GET_INDEX")
        return
    if isinstance(node, ast.Call):
        for arg in node.args:
            if isinstance(arg, ast.Starred):
                raise CompileError("argument unpacking with * is not supported yet")
        keyword_names = []
        for keyword in node.keywords:
            if keyword.arg is None:
                raise CompileError("argument unpacking with ** is not supported yet")
            if keyword.arg in keyword_names:
                raise CompileError(f"duplicate keyword argument: {keyword.arg}")
            keyword_names.append(keyword.arg)

        if isinstance(node.func, ast.Attribute):
            _compile_expr(ctx, node.func.value)
            ctx.emitter.emit("LOAD_CONST", ctx.compiler.constants.add(node.func.attr))
            for arg in node.args:
                _compile_expr(ctx, arg)
            if not keyword_names:
                ctx.emitter.emit("CALL_METHOD", len(node.args))
                return
            for keyword in node.keywords:
                _compile_expr(ctx, keyword.value)
            ctx.emitter.emit("LOAD_CONST", ctx.compiler.constants.add(tuple(keyword_names)))
            ctx.emitter.emit("CALL_METHOD_KW", len(node.args) + len(keyword_names))
            return
        if (
            isinstance(node.func, ast.Name)
            and node.func.id == "len"
            and "len" not in ctx.compiler._rebound_names
            and "len" not in ctx.compiler._defined_functions
        ):
            if len(node.args) != 1 or keyword_names:
                raise CompileError("len() takes exactly one argument")
            _compile_expr(ctx, node.args[0])
            ctx.emitter.emit("LEN")
            return
        _compile_expr(ctx, node.func)
        for arg in node.args:
            _compile_expr(ctx, arg)
        if not keyword_names:
            ctx.emitter.emit("CALL_FUNCTION", len(node.args))
            return
        for keyword in node.keywords:
            _compile_expr(ctx, keyword.value)
        ctx.emitter.emit("LOAD_CONST", ctx.compiler.constants.add(tuple(keyword_names)))
        ctx.emitter.emit("CALL_KW", len(node.args) + len(keyword_names))
        return
    raise CompileError(f"unsupported expression: {type(node).__name__}")

_COMPREHENSIONS = {
    ast.ListComp: ("List", "BUILD_LIST", "LIST_APPEND"),
    ast.SetComp: ("Set", "BUILD_SET", "SET_ADD"),
    ast.DictComp: ("Dict", "BUILD_MAP", "MAP_ADD"),
}

def _compile_comprehension(ctx: _FuncCtx, node: ast.expr) -> None:
    kind, build_op, add_op = _COMPREHENSIONS[type(node)]
    generators = node.generators
    for gen in generators:
        if gen.is_async:
            raise CompileError(f"async {kind.lower()} comprehensions are not supported")

    ctx.iter_depth += 1
    depth = ctx.iter_depth
    result_idx = ctx.locals.declare(f"__comp_result_{depth}")

    ctx.emitter.emit(build_op, 0)
    ctx.emitter.emit("STORE_FAST", result_idx)

    def emit_body():
        ctx.emitter.emit("LOAD_FAST", result_idx)
        if isinstance(node, ast.DictComp):
            _compile_expr(ctx, node.key)
            _compile_expr(ctx, node.value)
        else:
            _compile_expr(ctx, node.elt)
        ctx.emitter.emit(add_op)

    def emit_generator(i):
        if i == len(generators):
            emit_body()
            return
        gen = generators[i]
        iter_idx = ctx.locals.declare(f"__comp_iter_{depth}_{i}")
        _compile_expr(ctx, gen.iter)
        ctx.emitter.emit("GET_ITER")
        ctx.emitter.emit("STORE_FAST", iter_idx)

        start_label = ctx.emitter.new_label(f"comp_for_{depth}_{i}")
        end_label = ctx.emitter.new_label(f"comp_end_{depth}_{i}")
        ctx.emitter.label(start_label)
        ctx.emitter.emit("LOAD_FAST", iter_idx)
        ctx.emitter.emit_jump("FOR_ITER", end_label)
        _assign_to(ctx, gen.target)

        for cond in gen.ifs:
            _compile_expr(ctx, cond)
            ctx.emitter.emit_jump("POP_JUMP_IF_FALSE", start_label)

        emit_generator(i + 1)
        ctx.emitter.emit_jump("JUMP_ABSOLUTE", start_label)
        ctx.emitter.label(end_label)

    emit_generator(0)

    ctx.emitter.emit("LOAD_FAST", result_idx)
    ctx.iter_depth -= 1

def _constant_default(node: ast.expr):
    if isinstance(node, ast.Constant):
        return node.value
    if (
        isinstance(node, ast.UnaryOp)
        and isinstance(node.op, (ast.USub, ast.UAdd))
        and isinstance(node.operand, ast.Constant)
        and isinstance(node.operand.value, (int, float))
        and not isinstance(node.operand.value, bool)
    ):
        value = node.operand.value
        return -value if isinstance(node.op, ast.USub) else value
    if isinstance(node, ast.Tuple):
        return tuple(_constant_default(elt) for elt in node.elts)
    raise CompileError("default values must be constant expressions")

def _is_local_target(ctx: _FuncCtx, name: str) -> bool:
    return not ctx.is_module

def _store_name(ctx: _FuncCtx, name: str) -> None:
    if name in ctx.locals._by_name or _is_local_target(ctx, name):
        idx = ctx.locals.declare(name)
        ctx.emitter.emit("STORE_FAST", idx)
    else:
        idx = ctx.compiler.names.intern(name)
        ctx.emitter.emit("STORE_GLOBAL", idx)

def _assign_single(ctx: _FuncCtx, target, value_node) -> None:
    if isinstance(target, ast.Subscript):
        _compile_expr(ctx, target.value)
        _compile_expr(ctx, target.slice)
        _compile_expr(ctx, value_node)
        ctx.emitter.emit("SET_INDEX")
    else:
        _compile_expr(ctx, value_node)
        _assign_to(ctx, target)

def _assign_to(ctx: _FuncCtx, target) -> None:
    if isinstance(target, ast.Name):
        _store_name(ctx, target.id)
    elif isinstance(target, ast.Subscript):
        _compile_expr(ctx, target.value)
        _compile_expr(ctx, target.slice)
        ctx.emitter.emit("ROT_THREE")
        ctx.emitter.emit("ROT_THREE")
        ctx.emitter.emit("SET_INDEX")
    elif isinstance(target, (ast.Tuple, ast.List)):
        _unpack_targets(ctx, target.elts)
    elif isinstance(target, ast.Starred):
        raise CompileError("starred assignment target must be inside a list or tuple")
    else:
        raise CompileError(f"unsupported assignment target: {type(target).__name__}")

_FSTRING_CONVERSIONS = {115: 1, 114: 2, 97: 3}

def _compile_formatted_value(ctx: _FuncCtx, node) -> None:
    _compile_expr(ctx, node.value)
    if node.format_spec is None:
        ctx.emitter.emit("LOAD_CONST", ctx.compiler.constants.add(""))
    else:
        _compile_joined_str(ctx, node.format_spec)
    conv = _FSTRING_CONVERSIONS.get(node.conversion, 0)
    ctx.emitter.emit("FORMAT_VALUE", conv)

def _compile_joined_str(ctx: _FuncCtx, node) -> None:
    parts = 0
    for value in node.values:
        if isinstance(value, ast.Constant):
            if not isinstance(value.value, str):
                raise CompileError("unsupported f-string literal part")
            ctx.emitter.emit("LOAD_CONST", ctx.compiler.constants.add(value.value))
        elif isinstance(value, ast.FormattedValue):
            _compile_formatted_value(ctx, value)
        else:
            raise CompileError(f"unsupported f-string part: {type(value).__name__}")
        parts += 1

    if parts == 0:
        ctx.emitter.emit("LOAD_CONST", ctx.compiler.constants.add(""))
    elif parts > 1:
        ctx.emitter.emit("BUILD_STRING", parts)

def _compile_delete(ctx: _FuncCtx, target) -> None:
    if isinstance(target, ast.Name):
        name = target.id
        if name in ctx.locals._by_name or _is_local_target(ctx, name):
            idx = ctx.locals.declare(name)
            ctx.emitter.emit("DELETE_FAST", idx)
        else:
            idx = ctx.compiler.names.intern(name)
            ctx.emitter.emit("DELETE_GLOBAL", idx)
    elif isinstance(target, ast.Subscript):
        _compile_expr(ctx, target.value)
        _compile_expr(ctx, target.slice)
        ctx.emitter.emit("DELETE_SUBSCR")
    else:
        raise CompileError(f"unsupported del target: {type(target).__name__}")

def _unpack_targets(ctx: _FuncCtx, elts) -> None:
    stars = [i for i, e in enumerate(elts) if isinstance(e, ast.Starred)]
    if len(stars) > 1:
        raise CompileError("multiple starred targets in assignment")
    if stars:
        before = stars[0]
        after = len(elts) - before - 1
        ctx.emitter.emit("UNPACK_EX", (after << 16) | before)
    else:
        ctx.emitter.emit("UNPACK_SEQUENCE", len(elts))
    for elt in elts:
        if isinstance(elt, ast.Starred):
            _assign_to(ctx, elt.value)
        else:
            _assign_to(ctx, elt)

_BINOPS = {
    ast.Add: "BINARY_ADD",
    ast.Sub: "BINARY_SUB",
    ast.Mult: "BINARY_MUL",
    ast.Div: "BINARY_DIV",
    ast.FloorDiv: "BINARY_FLOORDIV",
    ast.Mod: "BINARY_MOD",
    ast.Pow: "BINARY_POW",
    ast.BitAnd: "BINARY_AND",
    ast.BitOr: "BINARY_OR",
    ast.BitXor: "BINARY_XOR",
    ast.LShift: "BINARY_LSHIFT",
    ast.RShift: "BINARY_RSHIFT",
}

_CMPOPS = {
    ast.Eq: "COMPARE_EQ",
    ast.NotEq: "COMPARE_NE",
    ast.Lt: "COMPARE_LT",
    ast.LtE: "COMPARE_LE",
    ast.Gt: "COMPARE_GT",
    ast.GtE: "COMPARE_GE",
}

_UNOPS = {
    ast.USub: "UNARY_NEG",
    ast.UAdd: "UNARY_POS",
    ast.Invert: "UNARY_INVERT",
    ast.Not: "UNARY_NOT",
}

def _emit_compare_link(ctx, op_node):
    if isinstance(op_node, ast.In):
        ctx.emitter.emit("CONTAINS")
    elif isinstance(op_node, ast.NotIn):
        ctx.emitter.emit("CONTAINS")
        ctx.emitter.emit("UNARY_NOT")
    elif isinstance(op_node, ast.Is):
        ctx.emitter.emit("COMPARE_IS")
    elif isinstance(op_node, ast.IsNot):
        ctx.emitter.emit("COMPARE_IS_NOT")
    else:
        op = _CMPOPS.get(type(op_node))
        if op is None:
            raise CompileError(f"unsupported compare: {type(op_node).__name__}")
        ctx.emitter.emit(op)

def compile_source(source: str, filename: str = "<module>") -> CompiledModule:
    compiler = Compiler()
    return compiler.compile_module(source, filename)
