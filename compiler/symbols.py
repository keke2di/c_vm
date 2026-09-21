from __future__ import annotations

import ast
from dataclasses import dataclass, field

MAX_LOCALS = 256
MAX_GLOBALS = 65535
MAX_NAME_LEN = 255

class SymbolError(Exception):
    pass

@dataclass
class Scope:
    name: str
    kind: str = "function"
    parent: Optional["Scope"] = None
    params: List[str] = field(default_factory=list)
    bindings: List[str] = field(default_factory=list)
    referenced: List[str] = field(default_factory=list)
    _referenced_seen: set = field(default_factory=set)
    globals_declared: List[str] = field(default_factory=list)
    nonlocals_declared: List[str] = field(default_factory=list)
    children: List["Scope"] = field(default_factory=list)
    locals_list: List[str] = field(default_factory=list)
    local_index: Dict[str, int] = field(default_factory=dict)
    cell_names: List[str] = field(default_factory=list)
    cell_index: Dict[str, int] = field(default_factory=dict)
    frees: List[str] = field(default_factory=list)
    free_index: Dict[str, int] = field(default_factory=dict)
    free_source: Dict[str, int] = field(default_factory=dict)

    @property
    def _by_name(self) -> Dict[str, int]:
        return self.local_index

    def bind(self, name: str) -> None:
        if name not in self.bindings:
            self.bindings.append(name)

    def reference(self, name: str) -> None:
        if name not in self._referenced_seen:
            self._referenced_seen.add(name)
            self.referenced.append(name)

    def declare(self, name: str) -> int:
        if name in self.local_index:
            return self.local_index[name]
        if len(self.locals_list) >= MAX_LOCALS:
            raise SymbolError(f"function {self.name!r} exceeds {MAX_LOCALS} locals")
        if len(name.encode("utf-8")) > MAX_NAME_LEN:
            raise SymbolError(f"local name too long: {name!r}")
        idx = len(self.locals_list)
        self.local_index[name] = idx
        self.locals_list.append(name)
        return idx

    def index(self, name: str) -> int:
        if name not in self.local_index:
            raise SymbolError(f"undefined local {name!r} in {self.name!r}")
        return self.local_index[name]

    def has(self, name: str) -> bool:
        return name in self.local_index

    def count(self) -> int:
        return len(self.locals_list)

    def names(self) -> List[str]:
        return list(self.locals_list)

    def resolve(self, name: str) -> Tuple[str, int]:
        if name in self.globals_declared:
            return ("global", 0)
        if self.kind == "module":
            if name in self.local_index:
                return ("local", self.local_index[name])
            return ("global", 0)
        if name in self.local_index:
            if name in self.cell_index:
                return ("cell", self.cell_index[name])
            return ("local", self.local_index[name])
        if name in self.free_index:
            return ("free", self.free_index[name])
        return ("global", 0)


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

class ScopeAnalyzer:
    def __init__(self) -> None:
        self.node_scopes: Dict[int, Scope] = {}

    def analyze_module(self, tree) -> Scope:
        root = Scope(name="<module>", kind="module")
        self._collect_body(root, tree.body)
        self._resolve(root)
        return root

    def scope_for(self, node) -> Scope:
        scope = self.node_scopes.get(id(node))
        if scope is None:
            raise SymbolError(f"no scope recorded for {type(node).__name__}")
        return scope

    def _collect_body(self, scope: Scope, body) -> None:
        for stmt in body:
            self._collect_stmt(scope, stmt)

    def _collect_stmt(self, scope: Scope, node) -> None:
        if isinstance(node, ast.FunctionDef):
            scope.bind(node.name)
            child = self._function_scope(scope, node)
            self._collect_body(child, node.body)
            return
        if isinstance(node, ast.Global):
            for name in node.names:
                if name not in scope.globals_declared:
                    scope.globals_declared.append(name)
            return
        if isinstance(node, ast.Nonlocal):
            for name in node.names:
                if name not in scope.nonlocals_declared:
                    scope.nonlocals_declared.append(name)
            return
        if isinstance(node, ast.Delete):
            for target in node.targets:
                self._collect_delete_target(scope, target)
            return
        for child in ast.iter_child_nodes(node):
            if isinstance(child, ast.stmt):
                self._collect_stmt(scope, child)
            elif isinstance(child, ast.expr):
                if isinstance(child, ast.arguments):
                    continue
                self._collect_expr(scope, child)
            else:
                self._collect_misc(scope, child)

    def _collect_misc(self, scope: Scope, node) -> None:
        if isinstance(node, ast.arguments):
            self._collect_arguments(scope, node)
            return
        if isinstance(node, ast.ExceptHandler):
            if node.name:
                scope.bind(node.name)
            if node.type is not None:
                self._collect_expr(scope, node.type)
            self._collect_body(scope, node.body)
            return
        for child in ast.iter_child_nodes(node):
            if isinstance(child, ast.stmt):
                self._collect_stmt(scope, child)
            elif isinstance(child, ast.expr):
                if isinstance(child, ast.arguments):
                    continue
                self._collect_expr(scope, child)
            else:
                self._collect_misc(scope, child)

    def _collect_arguments(self, scope: Scope, args: ast.arguments) -> None:
        for default in list(args.defaults):
            self._collect_expr(scope, default)
        for default in args.kw_defaults:
            if default is not None:
                self._collect_expr(scope, default)

    def _collect_delete_target(self, scope: Scope, target) -> None:
        if isinstance(target, ast.Name):
            scope.bind(target.id)
        elif isinstance(target, (ast.Tuple, ast.List)):
            for elt in target.elts:
                self._collect_delete_target(scope, elt)
        elif isinstance(target, ast.Starred):
            self._collect_delete_target(scope, target.value)
        elif isinstance(target, ast.Subscript):
            self._collect_expr(scope, target.value)
            if isinstance(target.slice, ast.Slice):
                for part in (target.slice.lower, target.slice.upper, target.slice.step):
                    if part is not None:
                        self._collect_expr(scope, part)
            else:
                self._collect_expr(scope, target.slice)

    def _collect_expr(self, scope: Scope, node) -> None:
        if node is None:
            return
        if isinstance(node, ast.Name):
            if isinstance(node.ctx, ast.Load):
                scope.reference(node.id)
            else:
                scope.bind(node.id)
            return
        if isinstance(node, ast.Lambda):
            child = self._function_scope(scope, node)
            self._collect_expr(child, node.body)
            return
        if isinstance(node, (ast.ListComp, ast.SetComp, ast.DictComp, ast.GeneratorExp)):
            child = Scope(
                name=f"<{type(node).__name__.lower()}>",
                kind="function",
                parent=scope,
                params=[".0"],
            )
            self.node_scopes[id(node)] = child
            scope.children.append(child)
            generators = node.generators
            self._collect_expr(scope, generators[0].iter)
            for index, generator in enumerate(generators):
                if index > 0:
                    self._collect_expr(child, generator.iter)
                self._collect_expr(child, generator.target)
                for condition in generator.ifs:
                    self._collect_expr(child, condition)
            if isinstance(node, ast.DictComp):
                self._collect_expr(child, node.key)
                self._collect_expr(child, node.value)
            else:
                self._collect_expr(child, node.elt)
            return
        if isinstance(node, ast.Starred):
            self._collect_expr(scope, node.value)
            return
        if isinstance(node, ast.arguments):
            self._collect_arguments(scope, node)
            return
        if isinstance(node, ast.Subscript) and isinstance(node.slice, ast.Slice):
            self._collect_expr(scope, node.value)
            for part in (node.slice.lower, node.slice.upper, node.slice.step):
                if part is not None:
                    self._collect_expr(scope, part)
            return
        for child in ast.iter_child_nodes(node):
            if isinstance(child, ast.expr):
                self._collect_expr(scope, child)
            elif isinstance(child, ast.stmt):
                self._collect_stmt(scope, child)
            else:
                self._collect_misc(scope, child)

    def _function_scope(self, parent: Scope, node) -> Scope:
        args = node.args
        name = node.name if isinstance(node, ast.FunctionDef) else "<lambda>"
        scope = Scope(name=name, kind="function", parent=parent)
        self.node_scopes[id(node)] = scope
        parent.children.append(scope)
        for arg in args.posonlyargs + args.args + args.kwonlyargs:
            scope.params.append(arg.arg)
        if args.vararg is not None:
            scope.params.append(args.vararg.arg)
        if args.kwarg is not None:
            scope.params.append(args.kwarg.arg)
        for decorator in getattr(node, "decorator_list", []):
            self._collect_expr(parent, decorator)
        self._collect_arguments(parent, args)
        return scope

    def _resolve(self, root: Scope) -> None:
        order: List[Scope] = []

        def walk(scope: Scope) -> None:
            order.append(scope)
            for child in scope.children:
                walk(child)

        walk(root)

        for scope in order:
            if scope.kind == "module":
                continue
            names: List[str] = []
            declared = set(scope.globals_declared) | set(scope.nonlocals_declared)
            for name in list(scope.params) + list(scope.bindings):
                if name in declared or name in names:
                    continue
                names.append(name)
            scope.locals_list = names
            scope.local_index = {name: index for index, name in enumerate(names)}

        for scope in reversed(order):
            if scope.kind == "module":
                continue
            for name in scope.nonlocals_declared:
                if self._find_binding(scope, name) is None:
                    raise SymbolError(f"no binding for nonlocal {name!r} found")
            for name in scope.referenced:
                if name in scope.globals_declared:
                    continue
                if name in scope.local_index:
                    continue
                owner = self._find_binding(scope, name)
                if owner is None:
                    continue
                if name not in scope.frees:
                    scope.frees.append(name)
                if name not in owner.cell_names:
                    owner.cell_names.append(name)
                self._propagate(scope, owner, name)

        for scope in order:
            scope.cell_names = [n for n in scope.locals_list if n in set(scope.cell_names)]
            scope.cell_index = {name: index for index, name in enumerate(scope.cell_names)}
            base = len(scope.cell_names)
            for index, name in enumerate(scope.frees):
                scope.free_index[name] = base + index

        for scope in order:
            for name in scope.frees:
                owner = self._find_binding(scope, name)
                if owner is None or name not in owner.cell_index:
                    raise SymbolError(f"unresolved free variable {name!r} in {scope.name!r}")
                scope.free_source[name] = owner.cell_index[name]

    def _propagate(self, scope: Scope, owner: Scope, name: str) -> None:
        current = scope.parent
        while current is not None and current is not owner:
            current.reference(name)
            current = current.parent

    def _find_binding(self, scope: Optional[Scope], name: str) -> Optional[Scope]:
        current = scope
        while current is not None and current.kind != "module":
            if name in current.local_index:
                return current
            current = current.parent
        return None

def serialize_names(names: List[str]) -> bytes:
    import struct
    out = bytearray(struct.pack("<I", len(names)))
    for n in names:
        data = n.encode("utf-8")
        if len(data) > MAX_NAME_LEN:
            raise SymbolError(f"name too long: {n!r}")
        out += struct.pack("<B", len(data)) + data
    return bytes(out)
