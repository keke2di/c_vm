from __future__ import annotations

import ast
import sys
from dataclasses import dataclass, field
from typing import Dict, List, Optional

from .constants import ConstantPool
from .emitter import Emitter, EmitterError
from .symbols import GlobalNames, Scope, ScopeAnalyzer, SymbolError

REQUIRED_PYTHON = (3, 14)

class CompileError(Exception):
    pass

@dataclass
class CompiledFunction:
    name: str
    num_locals: int
    code: bytes
    param_names: List[str] = field(default_factory=list)
    posonly_count: int = 0
    num_defaults: int = 0
    kwonly_names: List[str] = field(default_factory=list)
    kwonly_slots: List[int] = field(default_factory=list)
    kwonly_flags: List[int] = field(default_factory=list)
    vararg_slot: int = 0xFFFF
    kwarg_slot: int = 0xFFFF
    cell_slots: List[int] = field(default_factory=list)
    free_names: List[str] = field(default_factory=list)
    line_table: List[tuple] = field(default_factory=list)

    @property
    def num_params(self) -> int:
        return len(self.param_names)

@dataclass
class CompiledModule:
    constants: ConstantPool
    names: GlobalNames
    functions: List[CompiledFunction] = field(default_factory=list)
    entry_index: int = 0
    source_name: str = "<module>"

class Compiler:
    def __init__(self) -> None:
        self.constants = ConstantPool()
        self.names = GlobalNames()
        self.functions: List[Optional[CompiledFunction]] = []
        self._func_index: Dict[int, int] = {}
        self._defined_functions: set[str] = set()
        self._rebound_names: set[str] = set()
        self.analyzer = ScopeAnalyzer()
        self.module_scope: Optional[Scope] = None
        self.entry_index = 0
        self.source_name = "<module>"

    def compile_module(self, source: str, filename: str = "<module>") -> CompiledModule:
        if tuple(sys.version_info[:2]) != REQUIRED_PYTHON:
            raise CompileError(
                f"cVM requires Python {REQUIRED_PYTHON[0]}.{REQUIRED_PYTHON[1]}, "
                f"running {sys.version_info[0]}.{sys.version_info[1]}"
            )
        tree = ast.parse(source, filename=filename)
        self.source_name = filename
        self._defined_functions = {
            node.name for node in ast.walk(tree) if isinstance(node, ast.FunctionDef)
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

        self.analyzer = ScopeAnalyzer()
        self.module_scope = self.analyzer.analyze_module(tree)

        nested: List = []

        def collect(node) -> None:
            for child in ast.iter_child_nodes(node):
                if isinstance(child, (ast.FunctionDef, ast.Lambda, ast.ListComp, ast.SetComp, ast.DictComp)):
                    nested.append(child)
                collect(child)

        collect(tree)

        for node in nested:
            self._func_index[id(node)] = len(self.functions)
            self.functions.append(None)

        self.entry_index = len(self.functions)
        self.functions.append(None)

        for node in nested:
            self.functions[self._func_index[id(node)]] = self._compile_function_node(node)

        entry_scope = self.module_scope
        emitter = Emitter("__main__")
        ctx = _FuncCtx(
            emitter=emitter,
            locals=entry_scope,
            compiler=self,
            loop_stack=[],
            is_module=True,
        )
        for stmt in tree.body:
            _compile_stmt(ctx, stmt)
        emitter.emit("LOAD_CONST", self.constants.add(None))
        emitter.emit("RETURN_VALUE")

        self.functions[self.entry_index] = CompiledFunction(
            name="__main__",
            num_locals=entry_scope.count(),
            code=emitter.finalize(),
            line_table=list(emitter.line_table),
        )

        module = CompiledModule(
            constants=self.constants, names=self.names, functions=self.functions
        )
        module.source_name = self.source_name
        module.entry_index = self.entry_index
        return module

    def _compile_function_node(self, node) -> CompiledFunction:
        if isinstance(node, (ast.ListComp, ast.SetComp, ast.DictComp)):
            return self._compile_comprehension_node(node)
        return self._compile_body_function(node)

    def _compile_body_function(self, node) -> CompiledFunction:
        args = node.args
        if getattr(node, "type_params", None):
            raise CompileError("type parameters are not supported")
        is_lambda = isinstance(node, ast.Lambda)
        name = "<lambda>" if is_lambda else node.name
        scope = self.analyzer.scope_for(node)

        all_params = [arg.arg for arg in args.posonlyargs + args.args + args.kwonlyargs]
        if args.vararg is not None:
            all_params.append(args.vararg.arg)
        if args.kwarg is not None:
            all_params.append(args.kwarg.arg)
        if len(set(all_params)) != len(all_params):
            raise CompileError(f"duplicate parameter name in function {name}")

        param_names = [arg.arg for arg in args.posonlyargs + args.args]
        kwonly_names = [arg.arg for arg in args.kwonlyargs]

        emitter = Emitter(name)
        ctx = _FuncCtx(
            emitter=emitter,
            locals=scope,
            compiler=self,
            loop_stack=[],
            is_module=False,
        )
        if is_lambda:
            _compile_expr(ctx, node.body)
            emitter.emit("RETURN_VALUE")
        else:
            for stmt in node.body:
                _compile_stmt(ctx, stmt)
            emitter.emit("LOAD_CONST", self.constants.add(None))
            emitter.emit("RETURN_VALUE")

        def slot_of(param_name: str) -> int:
            if param_name not in scope.local_index:
                raise CompileError(f"parameter {param_name!r} has no slot")
            return scope.local_index[param_name]

        return CompiledFunction(
            name=name,
            num_locals=scope.count(),
            code=emitter.finalize(),
            line_table=list(emitter.line_table),
            param_names=param_names,
            posonly_count=len(args.posonlyargs),
            num_defaults=len(args.defaults),
            kwonly_names=kwonly_names,
            kwonly_slots=[slot_of(name) for name in kwonly_names],
            kwonly_flags=[0 if default is None else 1 for default in args.kw_defaults],
            vararg_slot=0xFFFF if args.vararg is None else slot_of(args.vararg.arg),
            kwarg_slot=0xFFFF if args.kwarg is None else slot_of(args.kwarg.arg),
            cell_slots=[slot_of(name) for name in scope.cell_names],
            free_names=list(scope.frees),
        )

    def _compile_comprehension_node(self, node) -> CompiledFunction:
        scope = self.analyzer.scope_for(node)
        emitter = Emitter("<comp>")
        ctx = _FuncCtx(
            emitter=emitter,
            locals=scope,
            compiler=self,
            loop_stack=[],
            is_module=False,
        )
        _compile_comprehension_body(ctx, node)
        return CompiledFunction(
            name="<comp>",
            num_locals=scope.count(),
            code=emitter.finalize(),
            line_table=list(emitter.line_table),
            param_names=[".0"],
            cell_slots=[scope.local_index[name] for name in scope.cell_names],
            free_names=list(scope.frees),
        )

@dataclass
class _FuncCtx:
    emitter: Emitter
    locals: Scope
    compiler: "Compiler"
    loop_stack: list
    is_module: bool = False
    iter_depth: int = 0
    temp_seq: int = 0
    handler_depth: int = 0
    except_depth: int = 0
    finally_stack: list = field(default_factory=list)

def _emit_exit_cleanup(ctx: _FuncCtx, handler_depth: int, except_depth: int) -> None:
    for _ in range(ctx.except_depth - except_depth):
        ctx.emitter.emit("EXCEPT_CLEAR")

    for _ in range(ctx.handler_depth - handler_depth):
        ctx.emitter.emit("POP_HANDLER")

def _emit_active_finally(ctx: _FuncCtx, final_depth: int) -> None:
    if len(ctx.finally_stack) <= final_depth:
        return

    pending = list(ctx.finally_stack[final_depth:])
    outer = list(ctx.finally_stack[:final_depth])

    del ctx.finally_stack[:]
    ctx.finally_stack.extend(outer)

    try:
        for body in reversed(pending):
            for stmt in body:
                _compile_stmt(ctx, stmt)
    finally:
        del ctx.finally_stack[:]
        ctx.finally_stack.extend(outer + pending)

def _emit_load_name(ctx: _FuncCtx, name: str) -> None:
    kind, slot = ctx.locals.resolve(name)

    if kind == "local":
        ctx.emitter.emit("LOAD_FAST", slot)
    elif kind in ("cell", "free"):
        ctx.emitter.emit("LOAD_DEREF", slot)
    else:
        ctx.emitter.emit("LOAD_GLOBAL", ctx.compiler.names.intern(name))

def _emit_store_name(ctx: _FuncCtx, name: str) -> None:
    kind, slot = ctx.locals.resolve(name)

    if kind == "local":
        ctx.emitter.emit("STORE_FAST", slot)
    elif kind in ("cell", "free"):
        ctx.emitter.emit("STORE_DEREF", slot)
    else:
        ctx.emitter.emit("STORE_GLOBAL", ctx.compiler.names.intern(name))

def _emit_delete_name(ctx: _FuncCtx, name: str) -> None:
    kind, slot = ctx.locals.resolve(name)

    if kind == "local":
        ctx.emitter.emit("DELETE_FAST", slot)
    elif kind in ("cell", "free"):
        ctx.emitter.emit("DELETE_DEREF", slot)
    else:
        ctx.emitter.emit("DELETE_GLOBAL", ctx.compiler.names.intern(name))

def _compile_stmt(ctx: _FuncCtx, node: ast.stmt) -> None:
    line = getattr(node, "lineno", None)
    if line:
        ctx.emitter.current_line = line

    if isinstance(node, ast.FunctionDef):
        _compile_function_def(ctx, node)
        return
    if isinstance(node, (ast.Global, ast.Nonlocal)):
        return
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
            _emit_load_name(ctx, name)
            _compile_expr(ctx, node.value)
            ctx.emitter.emit(op)
            _emit_store_name(ctx, name)
        elif isinstance(target, ast.Subscript):
            if isinstance(target.slice, ast.Slice):
                _compile_slice_augassign(ctx, op, target, node.value)
                return
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
        _emit_active_finally(ctx, 0)
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

    if isinstance(node, ast.Try):
        _compile_try(ctx, node)
        return

    if isinstance(node, ast.Raise):
        if node.cause is not None:
            raise CompileError("'raise ... from ...' is not supported yet")
        if node.exc is None:
            ctx.emitter.emit("RAISE_VARARGS", 0)
            return
        _compile_expr(ctx, node.exc)
        ctx.emitter.emit("RAISE_VARARGS", 1)
        return

    if isinstance(node, ast.Assert):
        end_label = ctx.emitter.new_label("assert_end")
        _compile_expr(ctx, node.test)
        ctx.emitter.emit_jump("POP_JUMP_IF_TRUE", end_label)

        ctx.emitter.emit("LOAD_GLOBAL", ctx.compiler.names.intern("AssertionError"))

        if node.msg is not None:
            _compile_expr(ctx, node.msg)
            ctx.emitter.emit("CALL_FUNCTION", 1)
        else:
            ctx.emitter.emit("CALL_FUNCTION", 0)

        ctx.emitter.emit("RAISE_VARARGS", 1)
        ctx.emitter.label(end_label)
        return

    if isinstance(node, ast.While):
        start_label = ctx.emitter.new_label("while_start")
        else_label = ctx.emitter.new_label("while_else")
        end_label = ctx.emitter.new_label("while_end")
        ctx.emitter.label(start_label)
        _compile_expr(ctx, node.test)
        ctx.emitter.emit_jump("POP_JUMP_IF_FALSE", else_label)
        ctx.loop_stack.append(
            (start_label, end_label, ctx.handler_depth, ctx.except_depth, len(ctx.finally_stack))
        )
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

        ctx.loop_stack.append(
            (start_label, end_label, ctx.handler_depth, ctx.except_depth, len(ctx.finally_stack))
        )

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
        start, end, handlers, excepts, final_depth = ctx.loop_stack[-1]
        _emit_active_finally(ctx, final_depth)
        _emit_exit_cleanup(ctx, handlers, excepts)
        ctx.emitter.emit_jump("JUMP_ABSOLUTE", end)
        return
    if isinstance(node, ast.Continue):
        if not ctx.loop_stack:
            raise CompileError("continue outside loop")
        start, end, handlers, excepts, final_depth = ctx.loop_stack[-1]
        _emit_active_finally(ctx, final_depth)
        _emit_exit_cleanup(ctx, handlers, excepts)
        ctx.emitter.emit_jump("JUMP_ABSOLUTE", start)
        return
    if isinstance(node, ast.Pass):
        ctx.emitter.emit("NOP")
        return
    raise CompileError(f"unsupported statement: {type(node).__name__}")

def _compile_expr(ctx: _FuncCtx, node: ast.expr) -> None:
    line = getattr(node, "lineno", None)
    if line:
        ctx.emitter.current_line = line

    if isinstance(node, ast.Constant):
        ctx.emitter.emit("LOAD_CONST", ctx.compiler.constants.add(node.value))
        return
    if isinstance(node, ast.Name):
        _emit_load_name(ctx, node.id)
        return
    if isinstance(node, ast.Lambda):
        _emit_function_value(ctx, node)
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
        has_star = any(isinstance(arg, ast.Starred) for arg in node.args)
        has_dstar = any(keyword.arg is None for keyword in node.keywords)

        if has_star or has_dstar:
            _compile_call_ex(ctx, node, has_star, has_dstar)
            return

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

def _compile_try(ctx: _FuncCtx, node: ast.Try) -> None:
    handlers = node.handlers
    has_finally = len(node.finalbody) > 0
    has_handlers = len(handlers) > 0

    end_label = ctx.emitter.new_label("try_end")
    else_label = ctx.emitter.new_label("try_else")
    normal_label = ctx.emitter.new_label("try_normal")

    handler_labels = [ctx.emitter.new_label(f"except_{i}") for i in range(len(handlers))]
    finally_label = ctx.emitter.new_label("try_finally") if has_finally else None

    if has_finally:
        ctx.handler_depth += 1
        ctx.emitter.emit_jump("SETUP_HANDLER", finally_label)

    if has_handlers:
        ctx.handler_depth += 1
        ctx.emitter.emit_jump("SETUP_HANDLER", handler_labels[0])

    if has_finally:
        ctx.finally_stack.append(node.finalbody)

    for stmt in node.body:
        _compile_stmt(ctx, stmt)

    if has_handlers:
        ctx.emitter.emit("POP_HANDLER")
        ctx.handler_depth -= 1
        ctx.emitter.emit_jump("JUMP_ABSOLUTE", else_label)

        for index, handler in enumerate(handlers):
            ctx.emitter.label(handler_labels[index])
            skip_label = None

            if handler.type is not None:
                skip_label = ctx.emitter.new_label(f"except_next_{index}")
                ctx.emitter.emit("DUP_TOP")
                _compile_expr(ctx, handler.type)
                ctx.emitter.emit("EXCEPT_MATCH")
                ctx.emitter.emit_jump("POP_JUMP_IF_FALSE", skip_label)

            if handler.name:
                ctx.emitter.emit("DUP_TOP")
                _emit_store_name(ctx, handler.name)

            ctx.except_depth += 1
            for stmt in handler.body:
                _compile_stmt(ctx, stmt)
            ctx.except_depth -= 1

            ctx.emitter.emit("EXCEPT_CLEAR")
            ctx.emitter.emit_jump("JUMP_ABSOLUTE", normal_label)

            if skip_label is not None:
                ctx.emitter.label(skip_label)

        ctx.emitter.emit("RERAISE")

    ctx.emitter.label(else_label)

    for stmt in node.orelse:
        _compile_stmt(ctx, stmt)

    if has_finally:
        ctx.finally_stack.pop()

    ctx.emitter.label(normal_label)

    if has_finally:
        ctx.emitter.emit("POP_HANDLER")
        ctx.handler_depth -= 1

        for stmt in node.finalbody:
            _compile_stmt(ctx, stmt)

        ctx.emitter.emit_jump("JUMP_ABSOLUTE", end_label)
        ctx.emitter.label(finally_label)

        for stmt in node.finalbody:
            _compile_stmt(ctx, stmt)

        ctx.emitter.emit("RERAISE")

    ctx.emitter.label(end_label)

def _compile_closure(ctx: _FuncCtx, node) -> None:
    child = ctx.compiler.analyzer.scope_for(node)

    for name in child.frees:
        kind, slot = ctx.locals.resolve(name)
        if kind not in ("cell", "free"):
            raise CompileError(f"closure over non-cell {name!r}")
        ctx.emitter.emit("LOAD_CLOSURE", slot)

    ctx.emitter.emit("BUILD_TUPLE", len(child.frees))

def _emit_function_value(ctx: _FuncCtx, node) -> None:
    index = ctx.compiler._func_index[id(node)]
    args = node.args

    for default in args.defaults:
        _compile_expr(ctx, default)
    ctx.emitter.emit("BUILD_TUPLE", len(args.defaults))

    provided = 0
    for arg, default in zip(args.kwonlyargs, args.kw_defaults):
        if default is None:
            continue
        ctx.emitter.emit("LOAD_CONST", ctx.compiler.constants.add(arg.arg))
        _compile_expr(ctx, default)
        provided += 1
    ctx.emitter.emit("BUILD_MAP", provided)

    _compile_closure(ctx, node)
    ctx.emitter.emit("MAKE_FUNCTION", index)

def _compile_function_def(ctx: _FuncCtx, node) -> None:
    decorators = []

    for decorator in node.decorator_list:
        _compile_expr(ctx, decorator)
        ctx.temp_seq += 1
        slot = ctx.locals.declare(f"__decorator_{ctx.temp_seq}")
        ctx.emitter.emit("STORE_FAST", slot)
        decorators.append(slot)

    _emit_function_value(ctx, node)

    for slot in reversed(decorators):
        ctx.emitter.emit("LOAD_FAST", slot)
        ctx.emitter.emit("ROT_TWO")
        ctx.emitter.emit("CALL_FUNCTION", 1)

    _emit_store_name(ctx, node.name)

def _compile_comprehension_body(ctx: _FuncCtx, node) -> None:
    kind, build_op, add_op = _COMPREHENSIONS[type(node)]
    generators = node.generators
    result_idx = ctx.locals.declare("__comp_result")

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
        end_label = ctx.emitter.new_label(f"comp_end_{i}")

        if i > 0:
            iter_idx = ctx.locals.declare(f"__comp_iter_{i}")
            _compile_expr(ctx, gen.iter)
            ctx.emitter.emit("GET_ITER")
            ctx.emitter.emit("STORE_FAST", iter_idx)

        start_label = ctx.emitter.new_label(f"comp_for_{i}")
        ctx.emitter.label(start_label)

        if i == 0:
            ctx.emitter.emit("LOAD_FAST", 0)
        else:
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
    ctx.emitter.emit("RETURN_VALUE")

def _compile_comprehension(ctx: _FuncCtx, node) -> None:
    index = ctx.compiler._func_index[id(node)]
    generator = node.generators[0]

    ctx.temp_seq += 1
    iter_idx = ctx.locals.declare(f"__comp_iter_{ctx.temp_seq}")
    _compile_expr(ctx, generator.iter)
    ctx.emitter.emit("GET_ITER")
    ctx.emitter.emit("STORE_FAST", iter_idx)

    ctx.emitter.emit("BUILD_TUPLE", 0)
    ctx.emitter.emit("BUILD_MAP", 0)
    _compile_closure(ctx, node)
    ctx.emitter.emit("MAKE_FUNCTION", index)

    ctx.emitter.emit("LOAD_FAST", iter_idx)
    ctx.emitter.emit("CALL_FUNCTION", 1)

def _compile_call_ex(ctx: _FuncCtx, node, has_star, has_dstar) -> None:
    if isinstance(node.func, ast.Attribute):
        raise CompileError("argument unpacking in method calls is not supported yet")

    named = []
    for keyword in node.keywords:
        if keyword.arg is not None:
            if keyword.arg in named:
                raise CompileError(f"duplicate keyword argument: {keyword.arg}")
            named.append(keyword.arg)

    ctx.temp_seq += 1
    tag = ctx.temp_seq
    args_idx = ctx.locals.declare(f"__call_args_{tag}")
    kwargs_idx = ctx.locals.declare(f"__call_kwargs_{tag}")

    ctx.emitter.emit("BUILD_LIST", 0)
    ctx.emitter.emit("STORE_FAST", args_idx)

    for arg in node.args:
        ctx.emitter.emit("LOAD_FAST", args_idx)
        if isinstance(arg, ast.Starred):
            _compile_expr(ctx, arg.value)
            ctx.emitter.emit("LIST_EXTEND")
        else:
            _compile_expr(ctx, arg)
            ctx.emitter.emit("LIST_APPEND")

    ctx.emitter.emit("BUILD_MAP", 0)
    ctx.emitter.emit("STORE_FAST", kwargs_idx)

    for keyword in node.keywords:
        ctx.emitter.emit("LOAD_FAST", kwargs_idx)
        if keyword.arg is None:
            _compile_expr(ctx, keyword.value)
            ctx.emitter.emit("DICT_MERGE")
        elif has_dstar:
            ctx.emitter.emit("LOAD_CONST", ctx.compiler.constants.add(keyword.arg))
            _compile_expr(ctx, keyword.value)
            ctx.emitter.emit("BUILD_MAP", 1)
            ctx.emitter.emit("DICT_MERGE")
        else:
            ctx.emitter.emit("LOAD_CONST", ctx.compiler.constants.add(keyword.arg))
            _compile_expr(ctx, keyword.value)
            ctx.emitter.emit("MAP_ADD")

    _compile_expr(ctx, node.func)
    ctx.emitter.emit("LOAD_FAST", args_idx)
    ctx.emitter.emit("LOAD_FAST", kwargs_idx)
    ctx.emitter.emit("CALL_EX")

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

def _assign_single(ctx: _FuncCtx, target, value_node) -> None:
    if isinstance(target, ast.Subscript):
        if isinstance(target.slice, ast.Slice):
            _compile_expr(ctx, value_node)
            _emit_slice_target(ctx, target)
            ctx.emitter.emit("STORE_SLICE")
            return
        _compile_expr(ctx, target.value)
        _compile_expr(ctx, target.slice)
        _compile_expr(ctx, value_node)
        ctx.emitter.emit("SET_INDEX")
    else:
        _compile_expr(ctx, value_node)
        _assign_to(ctx, target)

def _emit_slice_parts(ctx: _FuncCtx, node) -> None:
    for part in (node.lower, node.upper, node.step):
        if part is None:
            ctx.emitter.emit("LOAD_CONST", ctx.compiler.constants.add(None))
        else:
            _compile_expr(ctx, part)

def _emit_slice_target(ctx: _FuncCtx, target) -> None:
    _compile_expr(ctx, target.value)
    _emit_slice_parts(ctx, target.slice)

def _store_slice_part(ctx: _FuncCtx, node, idx: int) -> None:
    if node is None:
        ctx.emitter.emit("LOAD_CONST", ctx.compiler.constants.add(None))
    else:
        _compile_expr(ctx, node)
    ctx.emitter.emit("STORE_FAST", idx)

def _compile_slice_augassign(ctx: _FuncCtx, op: str, target, value_node) -> None:
    ctx.temp_seq += 1
    tag = ctx.temp_seq

    obj_idx = ctx.locals.declare(f"__slice_obj_{tag}")
    start_idx = ctx.locals.declare(f"__slice_start_{tag}")
    stop_idx = ctx.locals.declare(f"__slice_stop_{tag}")
    step_idx = ctx.locals.declare(f"__slice_step_{tag}")

    _compile_expr(ctx, target.value)
    ctx.emitter.emit("STORE_FAST", obj_idx)
    _store_slice_part(ctx, target.slice.lower, start_idx)
    _store_slice_part(ctx, target.slice.upper, stop_idx)
    _store_slice_part(ctx, target.slice.step, step_idx)

    for idx in (obj_idx, start_idx, stop_idx, step_idx):
        ctx.emitter.emit("LOAD_FAST", idx)
    ctx.emitter.emit("GET_SLICE")

    _compile_expr(ctx, value_node)
    ctx.emitter.emit(op)

    for idx in (obj_idx, start_idx, stop_idx, step_idx):
        ctx.emitter.emit("LOAD_FAST", idx)
    ctx.emitter.emit("STORE_SLICE")

def _assign_to(ctx: _FuncCtx, target) -> None:
    if isinstance(target, ast.Name):
        _emit_store_name(ctx, target.id)
    elif isinstance(target, ast.Subscript):
        if isinstance(target.slice, ast.Slice):
            _emit_slice_target(ctx, target)
            ctx.emitter.emit("STORE_SLICE")
            return
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
        _emit_delete_name(ctx, target.id)
    elif isinstance(target, ast.Subscript):
        if isinstance(target.slice, ast.Slice):
            _emit_slice_target(ctx, target)
            ctx.emitter.emit("DELETE_SLICE")
            return
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
