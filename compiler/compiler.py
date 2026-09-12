from __future__ import annotations

import ast
from dataclasses import dataclass, field
from typing import Dict, List, Optional

from .constants import ConstantPool
from .emitter import Emitter, EmitterError
from .symbols import GlobalNames, LocalTable, SymbolError

class CompileError(Exception):
    pass

@dataclass
class CompiledFunction:
    name: str
    num_locals: int
    num_params: int
    code: bytes

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

    def compile_module(self, source: str, filename: str = "<module>") -> CompiledModule:
        tree = ast.parse(source, filename=filename)
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
        locals_tbl = LocalTable(node.name)
        for arg in node.args.args:
            locals_tbl.declare(arg.arg)
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
            num_params=len(node.args.args),
            code=code,
        ))
        self._func_index[node.name] = idx

@dataclass
class _FuncCtx:
    emitter: Emitter
    locals: LocalTable
    compiler: "Compiler"
    loop_stack: list
    is_module: bool = False

def _compile_stmt(ctx: _FuncCtx, node: ast.stmt) -> None:
    if isinstance(node, ast.Assign):
        if len(node.targets) != 1:
            raise CompileError("only single assignment supported")
        target = node.targets[0]
        if isinstance(target, ast.Name):
            _compile_expr(ctx, node.value)
            name = target.id
            if name in ctx.locals._by_name or _is_local_target(ctx, name):
                idx = ctx.locals.declare(name)
                ctx.emitter.emit("STORE_FAST", idx)
            else:
                idx = ctx.compiler.names.intern(name)
                ctx.emitter.emit("STORE_GLOBAL", idx)
        elif isinstance(target, ast.Subscript):
            _compile_expr(ctx, target.value)
            _compile_expr(ctx, target.slice)
            _compile_expr(ctx, node.value)
            ctx.emitter.emit("SET_INDEX")
        else:
            raise CompileError(f"unsupported assignment target: {type(target).__name__}")
        return
    if isinstance(node, ast.AugAssign):
        if not isinstance(node.target, ast.Name):
            raise CompileError("augmented assignment only supported for simple names")
        target = node.target.id
        if target in ctx.locals._by_name:
            ctx.emitter.emit("LOAD_FAST", ctx.locals.index(target))
        else:
            ctx.emitter.emit("LOAD_GLOBAL", ctx.compiler.names.intern(target))
        _compile_expr(ctx, node.value)
        op = _BINOPS.get(type(node.op))
        if op is None:
            raise CompileError(f"unsupported augmented assignment operator: {type(node.op).__name__}")
        ctx.emitter.emit(op)
        if target in ctx.locals._by_name:
            ctx.emitter.emit("STORE_FAST", ctx.locals.index(target))
        else:
            ctx.emitter.emit("STORE_GLOBAL", ctx.compiler.names.intern(target))
        return
    if isinstance(node, ast.Expr):
        _compile_expr(ctx, node.value)
        ctx.emitter.emit("POP_TOP")
        return
    if isinstance(node, ast.Return):
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
        end_label = ctx.emitter.new_label("while_end")
        ctx.emitter.label(start_label)
        _compile_expr(ctx, node.test)
        ctx.emitter.emit_jump("POP_JUMP_IF_FALSE", end_label)
        ctx.loop_stack.append((start_label, end_label))
        for s in node.body:
            _compile_stmt(ctx, s)
        ctx.loop_stack.pop()
        ctx.emitter.emit_jump("JUMP_ABSOLUTE", start_label)
        ctx.emitter.label(end_label)
        return

    if isinstance(node, ast.For):
        is_range = (isinstance(node.iter, ast.Call) and
                    isinstance(node.iter.func, ast.Name) and
                    node.iter.func.id == 'range')

        if is_range:
            args = node.iter.args
            if not 1 <= len(args) <= 3:
                raise CompileError("range() expects 1-3 arguments")

            if not all(
                isinstance(arg, ast.Constant)
                and isinstance(arg.value, int)
                and not isinstance(arg.value, bool)
                for arg in args
            ):
                raise CompileError("range() currently requires integer constants")

            if len(args) == 1:
                start_value, stop_value, step_value = 0, args[0].value, 1
            elif len(args) == 2:
                start_value, stop_value, step_value = args[0].value, args[1].value, 1
            else:
                start_value, stop_value, step_value = (
                    args[0].value,
                    args[1].value,
                    args[2].value,
                )

            if step_value == 0:
                raise CompileError("range() step cannot be zero")

            target = node.target
            if not isinstance(target, ast.Name):
                raise CompileError("for loop target must be a simple name")

            if step_value < 0:
                raise CompileError("negative range() steps are not supported")

            target_idx = (
                ctx.locals.index(target.id)
                if target.id in ctx.locals._by_name
                else ctx.locals.declare(target.id)
            )

            ctx.emitter.emit(
                "LOAD_CONST",
                ctx.compiler.constants.add(start_value)
            )
            ctx.emitter.emit("STORE_FAST", target_idx)

            start_label = ctx.emitter.new_label("for_start")
            continue_label = ctx.emitter.new_label("for_continue")
            end_label = ctx.emitter.new_label("for_end")

            ctx.emitter.label(start_label)

            ctx.emitter.emit("LOAD_FAST", target_idx)
            ctx.emitter.emit(
                "LOAD_CONST",
                ctx.compiler.constants.add(stop_value)
            )
            ctx.emitter.emit("COMPARE_GE")
            ctx.emitter.emit_jump("POP_JUMP_IF_TRUE", end_label)

            ctx.loop_stack.append((continue_label, end_label))

            for s in node.body:
                _compile_stmt(ctx, s)

            ctx.loop_stack.pop()

            ctx.emitter.label(continue_label)

            ctx.emitter.emit("LOAD_FAST", target_idx)
            ctx.emitter.emit(
                "LOAD_CONST",
                ctx.compiler.constants.add(step_value)
            )
            ctx.emitter.emit("BINARY_ADD")
            ctx.emitter.emit("STORE_FAST", target_idx)

            ctx.emitter.emit_jump("JUMP_ABSOLUTE", start_label)
            ctx.emitter.label(end_label)

            return

        else:
            target = node.target
            if not isinstance(target, ast.Name):
                raise CompileError("for loop target must be a simple name")

            temp_name = "__iter_temp"
            temp_idx = ctx.locals.declare(temp_name)

            _compile_expr(ctx, node.iter)
            ctx.emitter.emit("STORE_FAST", temp_idx)

            ctx.emitter.emit("LOAD_FAST", temp_idx)
            ctx.emitter.emit("LEN")

            temp_len_name = "__len_temp"
            len_idx = ctx.locals.declare(temp_len_name)
            ctx.emitter.emit("STORE_FAST", len_idx)

            idx_name = "__index_temp"
            idx_idx = ctx.locals.declare(idx_name)

            ctx.emitter.emit(
                "LOAD_CONST",
                ctx.compiler.constants.add(0)
            )
            ctx.emitter.emit("STORE_FAST", idx_idx)

            start_label = ctx.emitter.new_label("for_start")
            continue_label = ctx.emitter.new_label("for_continue")
            end_label = ctx.emitter.new_label("for_end")

            ctx.emitter.label(start_label)

            ctx.emitter.emit("LOAD_FAST", idx_idx)
            ctx.emitter.emit("LOAD_FAST", len_idx)
            ctx.emitter.emit("COMPARE_GE")
            ctx.emitter.emit_jump("POP_JUMP_IF_TRUE", end_label)

            ctx.emitter.emit("LOAD_FAST", temp_idx)
            ctx.emitter.emit("LOAD_FAST", idx_idx)
            ctx.emitter.emit("GET_ITER_ITEM")

            target_idx = (
                ctx.locals.index(target.id)
                if target.id in ctx.locals._by_name
                else ctx.locals.declare(target.id)
            )
            ctx.emitter.emit("STORE_FAST", target_idx)

            ctx.loop_stack.append((continue_label, end_label))

            for s in node.body:
                _compile_stmt(ctx, s)

            ctx.loop_stack.pop()

            ctx.emitter.label(continue_label)

            ctx.emitter.emit("LOAD_FAST", idx_idx)
            ctx.emitter.emit(
                "LOAD_CONST",
                ctx.compiler.constants.add(1)
            )
            ctx.emitter.emit("BINARY_ADD")
            ctx.emitter.emit("STORE_FAST", idx_idx)

            ctx.emitter.emit_jump("JUMP_ABSOLUTE", start_label)
            ctx.emitter.label(end_label)

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
    if isinstance(node, ast.BinOp):
        _compile_expr(ctx, node.left)
        _compile_expr(ctx, node.right)
        op = _BINOPS.get(type(node.op))
        if op is None:
            raise CompileError(f"unsupported binop: {type(node.op).__name__}")
        ctx.emitter.emit(op)
        return
    if isinstance(node, ast.Compare):
        if len(node.ops) != 1 or len(node.comparators) != 1:
            raise CompileError("chained comparisons not supported")
        first_op = node.ops[0]
        if isinstance(first_op, ast.In):
            _compile_expr(ctx, node.left)
            _compile_expr(ctx, node.comparators[0])
            ctx.emitter.emit("CONTAINS")
            return
        elif isinstance(first_op, ast.NotIn):
            _compile_expr(ctx, node.left)
            _compile_expr(ctx, node.comparators[0])
            ctx.emitter.emit("CONTAINS")
            ctx.emitter.emit("UNARY_NOT")
            return
        _compile_expr(ctx, node.left)
        _compile_expr(ctx, node.comparators[0])
        op = _CMPOPS.get(type(first_op))
        if op is None:
            raise CompileError(f"unsupported compare: {type(first_op).__name__}")
        ctx.emitter.emit(op)
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
        for key, value in zip(node.keys, node.values):
            _compile_expr(ctx, key)
            _compile_expr(ctx, value)
        ctx.emitter.emit("BUILD_MAP", len(node.keys))
        return
    if isinstance(node, ast.ListComp):
        if len(node.generators) != 1:
            raise CompileError("Only single-generator list comprehensions are supported")
        gen = node.generators[0]
        if gen.ifs:
            raise CompileError("List comprehensions with if conditions are not supported yet")

        target = gen.target
        if not isinstance(target, ast.Name):
            raise CompileError("List comprehension target must be a simple name")

        temp_list_name = "__list_comp"
        temp_list_idx = ctx.locals.declare(temp_list_name)
        ctx.emitter.emit("BUILD_LIST", 0)
        ctx.emitter.emit("STORE_FAST", temp_list_idx)

        iter_temp_name = "__iter_comp_temp"
        iter_temp_idx = ctx.locals.declare(iter_temp_name)
        _compile_expr(ctx, gen.iter)
        ctx.emitter.emit("STORE_FAST", iter_temp_idx)

        ctx.emitter.emit("LOAD_FAST", iter_temp_idx)
        ctx.emitter.emit("LEN")
        len_temp_name = "__len_comp_temp"
        len_temp_idx = ctx.locals.declare(len_temp_name)
        ctx.emitter.emit("STORE_FAST", len_temp_idx)

        idx_temp_name = "__idx_comp_temp"
        idx_temp_idx = ctx.locals.declare(idx_temp_name)
        ctx.emitter.emit("LOAD_CONST", ctx.compiler.constants.add(0))
        ctx.emitter.emit("STORE_FAST", idx_temp_idx)

        start_label = ctx.emitter.new_label("comp_start")
        end_label = ctx.emitter.new_label("comp_end")
        ctx.emitter.label(start_label)

        ctx.emitter.emit("LOAD_FAST", idx_temp_idx)
        ctx.emitter.emit("LOAD_FAST", len_temp_idx)
        ctx.emitter.emit("COMPARE_GE")
        ctx.emitter.emit_jump("POP_JUMP_IF_TRUE", end_label)

        ctx.emitter.emit("LOAD_FAST", iter_temp_idx)
        ctx.emitter.emit("LOAD_FAST", idx_temp_idx)
        ctx.emitter.emit("GET_ITER_ITEM")
        target_idx = ctx.locals.index(target.id) if target.id in ctx.locals._by_name else ctx.locals.declare(target.id)
        ctx.emitter.emit("STORE_FAST", target_idx)

        ctx.emitter.emit("LOAD_FAST", temp_list_idx)
        _compile_expr(ctx, node.elt)
        ctx.emitter.emit("LIST_APPEND")

        ctx.emitter.emit("LOAD_FAST", idx_temp_idx)
        ctx.emitter.emit("LOAD_CONST", ctx.compiler.constants.add(1))
        ctx.emitter.emit("BINARY_ADD")
        ctx.emitter.emit("STORE_FAST", idx_temp_idx)

        ctx.emitter.emit_jump("JUMP_ABSOLUTE", start_label)
        ctx.emitter.label(end_label)

        ctx.emitter.emit("LOAD_FAST", temp_list_idx)
        return

    if isinstance(node, ast.SetComp):
        if len(node.generators) != 1:
            raise CompileError("Only single-generator set comprehensions are supported")
        gen = node.generators[0]
        if gen.ifs:
            raise CompileError("Set comprehensions with if conditions are not supported yet")

        target = gen.target
        if not isinstance(target, ast.Name):
            raise CompileError("Set comprehension target must be a simple name")

        temp_set_name = "__set_comp"
        temp_set_idx = ctx.locals.declare(temp_set_name)
        ctx.emitter.emit("BUILD_SET", 0)
        ctx.emitter.emit("STORE_FAST", temp_set_idx)

        iter_temp_name = "__iter_comp_temp"
        iter_temp_idx = ctx.locals.declare(iter_temp_name)
        _compile_expr(ctx, gen.iter)
        ctx.emitter.emit("STORE_FAST", iter_temp_idx)

        ctx.emitter.emit("LOAD_FAST", iter_temp_idx)
        ctx.emitter.emit("LEN")
        len_temp_name = "__len_comp_temp"
        len_temp_idx = ctx.locals.declare(len_temp_name)
        ctx.emitter.emit("STORE_FAST", len_temp_idx)

        idx_temp_name = "__idx_comp_temp"
        idx_temp_idx = ctx.locals.declare(idx_temp_name)
        ctx.emitter.emit("LOAD_CONST", ctx.compiler.constants.add(0))
        ctx.emitter.emit("STORE_FAST", idx_temp_idx)

        start_label = ctx.emitter.new_label("comp_start")
        end_label = ctx.emitter.new_label("comp_end")
        ctx.emitter.label(start_label)

        ctx.emitter.emit("LOAD_FAST", idx_temp_idx)
        ctx.emitter.emit("LOAD_FAST", len_temp_idx)
        ctx.emitter.emit("COMPARE_GE")
        ctx.emitter.emit_jump("POP_JUMP_IF_TRUE", end_label)

        ctx.emitter.emit("LOAD_FAST", iter_temp_idx)
        ctx.emitter.emit("LOAD_FAST", idx_temp_idx)
        ctx.emitter.emit("GET_INDEX")
        target_idx = ctx.locals.index(target.id) if target.id in ctx.locals._by_name else ctx.locals.declare(target.id)
        ctx.emitter.emit("STORE_FAST", target_idx)

        ctx.emitter.emit("LOAD_FAST", temp_set_idx)
        _compile_expr(ctx, node.elt)
        ctx.emitter.emit("SET_ADD")

        ctx.emitter.emit("LOAD_FAST", idx_temp_idx)
        ctx.emitter.emit("LOAD_CONST", ctx.compiler.constants.add(1))
        ctx.emitter.emit("BINARY_ADD")
        ctx.emitter.emit("STORE_FAST", idx_temp_idx)

        ctx.emitter.emit_jump("JUMP_ABSOLUTE", start_label)
        ctx.emitter.label(end_label)

        ctx.emitter.emit("LOAD_FAST", temp_set_idx)
        return

    if isinstance(node, ast.DictComp):
        if len(node.generators) != 1:
            raise CompileError("Only single-generator dict comprehensions are supported")
        gen = node.generators[0]
        if gen.ifs:
            raise CompileError("Dict comprehensions with if conditions are not supported yet")

        target = gen.target
        if not isinstance(target, ast.Name):
            raise CompileError("Dict comprehension target must be a simple name")

        temp_dict_name = "__dict_comp"
        temp_dict_idx = ctx.locals.declare(temp_dict_name)
        ctx.emitter.emit("BUILD_MAP", 0)
        ctx.emitter.emit("STORE_FAST", temp_dict_idx)

        iter_temp_name = "__iter_comp_temp"
        iter_temp_idx = ctx.locals.declare(iter_temp_name)
        _compile_expr(ctx, gen.iter)
        ctx.emitter.emit("STORE_FAST", iter_temp_idx)

        ctx.emitter.emit("LOAD_FAST", iter_temp_idx)
        ctx.emitter.emit("LEN")
        len_temp_name = "__len_comp_temp"
        len_temp_idx = ctx.locals.declare(len_temp_name)
        ctx.emitter.emit("STORE_FAST", len_temp_idx)

        idx_temp_name = "__idx_comp_temp"
        idx_temp_idx = ctx.locals.declare(idx_temp_name)
        ctx.emitter.emit("LOAD_CONST", ctx.compiler.constants.add(0))
        ctx.emitter.emit("STORE_FAST", idx_temp_idx)

        start_label = ctx.emitter.new_label("comp_start")
        end_label = ctx.emitter.new_label("comp_end")
        ctx.emitter.label(start_label)

        ctx.emitter.emit("LOAD_FAST", idx_temp_idx)
        ctx.emitter.emit("LOAD_FAST", len_temp_idx)
        ctx.emitter.emit("COMPARE_GE")
        ctx.emitter.emit_jump("POP_JUMP_IF_TRUE", end_label)

        ctx.emitter.emit("LOAD_FAST", iter_temp_idx)
        ctx.emitter.emit("LOAD_FAST", idx_temp_idx)
        ctx.emitter.emit("GET_INDEX")
        target_idx = ctx.locals.index(target.id) if target.id in ctx.locals._by_name else ctx.locals.declare(target.id)
        ctx.emitter.emit("STORE_FAST", target_idx)

        ctx.emitter.emit("LOAD_FAST", temp_dict_idx)
        _compile_expr(ctx, node.key)
        _compile_expr(ctx, node.value)
        ctx.emitter.emit("MAP_ADD")

        ctx.emitter.emit("LOAD_FAST", idx_temp_idx)
        ctx.emitter.emit("LOAD_CONST", ctx.compiler.constants.add(1))
        ctx.emitter.emit("BINARY_ADD")
        ctx.emitter.emit("STORE_FAST", idx_temp_idx)

        ctx.emitter.emit_jump("JUMP_ABSOLUTE", start_label)
        ctx.emitter.label(end_label)

        ctx.emitter.emit("LOAD_FAST", temp_dict_idx)
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
        if isinstance(node.func, ast.Attribute):
            _compile_expr(ctx, node.func.value)
            method_name = node.func.attr
            method_idx = ctx.compiler.constants.add(method_name)
            ctx.emitter.emit("LOAD_CONST", method_idx)
            for arg in node.args:
                _compile_expr(ctx, arg)
            ctx.emitter.emit("CALL_METHOD", len(node.args))
            return
        if not isinstance(node.func, ast.Name):
            raise CompileError("only simple function or method calls supported")
        func_name = node.func.id
        if func_name == "len":
            if len(node.args) != 1:
                raise CompileError("len() takes exactly one argument")
            _compile_expr(ctx, node.args[0])
            ctx.emitter.emit("LEN")
            return
        for arg in node.args:
            _compile_expr(ctx, arg)
        name_idx = ctx.compiler.names.intern(func_name)
        ctx.emitter.emit("LOAD_CONST", ctx.compiler.constants.add(name_idx))
        ctx.emitter.emit("CALL_FUNCTION", len(node.args))
        return
    raise CompileError(f"unsupported expression: {type(node).__name__}")

def _is_local_target(ctx: _FuncCtx, name: str) -> bool:
    return not ctx.is_module

_BINOPS = {
    ast.Add: "BINARY_ADD",
    ast.Sub: "BINARY_SUB",
    ast.Mult: "BINARY_MUL",
    ast.Div: "BINARY_DIV",
    ast.FloorDiv: "BINARY_FLOORDIV",
    ast.Mod: "BINARY_MOD",
    ast.Pow: "BINARY_POW",
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

def compile_source(source: str, filename: str = "<module>") -> CompiledModule:
    compiler = Compiler()
    return compiler.compile_module(source, filename)
