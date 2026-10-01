import ast
import inspect
import sys
import enum
import struct
import builtins
from typing import List, Dict, Any, Tuple, Set, Union, Callable, Optional


# return whether an object is a specialisation of the given generic,
# e.g. _is_generic(Dict, Dict[str, int]) == True
def _is_generic(generic, obj):
    if hasattr(obj, "__origin__") and hasattr(generic, "__origin__"):
        if generic.__origin__ == obj.__origin__:
            return True

    if hasattr(obj, "__origin__"):
        return obj.__origin__ == generic

    return False


def _unwrap_optional(outertype):
    if _is_generic(Optional, outertype):
        return getattr(outertype, '__args__')[0]

    if _is_generic(Union, outertype):
        non_none = [
            a
            for a in getattr(outertype, '__args__')
            if a is not None and a is not type(None)
        ]
        if len(non_none) == 1:
            return getattr(outertype, '__args__')[0]
    return outertype

# return true for AST nodes that need their own scope - this is module level, then
# classes and functions which may be nested inside each other
def _is_scope_node(node):
    return (
        isinstance(node, ast.Module)
        or isinstance(node, ast.ClassDef)
        or isinstance(node, ast.FunctionDef)
        or isinstance(node, ast.AsyncFunctionDef)
    )


def _expr_to_str(node) -> str:
    if isinstance(node, ast.Name):
        return node.id
    if isinstance(node, ast.alias):
        return f"{node.name} as {node.asname}"
    if isinstance(node, ast.Attribute):
        return _expr_to_str(node.value) + "." + node.attr
    if isinstance(node, ast.NamedExpr):
        return f"({_expr_to_str(node.target)} := {_expr_to_str(node.value)})"
    if isinstance(node, ast.Constant):
        return str(node.value)
    if isinstance(node, ast.Subscript):
        return f"{_expr_to_str(node.value)}[{_expr_to_str(node.slice)}]"
    if isinstance(node, ast.Slice):
        ret = f"{_expr_to_str(node.lower)}:{_expr_to_str(node.upper)}"
        if node.step is not None:
            ret += _expr_to_str(node.step)
        return ret
    if isinstance(node, ast.Tuple):
        ret = ",".join([_expr_to_str(x) for x in node.elts])
        return f"({ret})"
    if isinstance(node, ast.List):
        ret = ",".join([_expr_to_str(x) for x in node.elts])
        return f"[{ret}]"

    # legacy types before consolidation into constant
    if sys.version_info < (3, 8):
        if isinstance(node, ast.Num):
            return str(node.value)
        if isinstance(node, ast.Str):
            return str(node.value)
        if isinstance(node, ast.Bytes):
            return str(node.value)
        if isinstance(node, ast.NameConstant):
            return str(node.value)

    return "..."


def _lookup_attrpath(x: Any, path: str) -> Any:
    paths = path.split(".")
    while x is not None and len(paths) > 0:
        x = getattr(x, paths[0], None)
        paths.pop(0)
    return x


# get all the return statements immediately inside a function without going into nested
# functions
def _get_return_statements(node: ast.AST, root: bool = True):
    if _is_scope_node(node) and not root:
        return []

    if isinstance(node, ast.Return):
        return [node]

    ret = []

    for recurse in ["body", "orelse", "finalbody"]:
        if hasattr(node, recurse):
            for n in getattr(node, recurse):
                if not _is_scope_node(n):
                    ret += _get_return_statements(n, False)

    return ret


# Python 3.8+ is expected to have start and end lines.
# Before that, end was missing so we assume all statements are single
# line (or the last child).
def _get_linerange(node: ast.AST):
    if not hasattr(node, "lineno"):
        return (-1, -1)

    lineno = getattr(node, "lineno")

    if hasattr(node, "end_lineno"):
        return (lineno, getattr(node, "end_lineno"))

    end_lineno = lineno
    for recurse in ["body", "orelse", "finalbody"]:
        if hasattr(node, recurse) and len(getattr(node, recurse)) > 0:
            end_lineno = max(end_lineno, _get_linerange(getattr(node, recurse)[-1])[1])

    return (lineno, end_lineno)


# Python 3.8+ is expected to have start and end columns.
# Before that, end was missing so we assume all statements are
# extremely long
def _get_colrange(node):
    if not hasattr(node, "col_offset"):
        return (-1, -1)

    return (node.col_offset, getattr(node, "end_col_offset", 9999))


# true if this is a `self.foo` type attribute lookup, so we know
# to look up in the parent scope which is how we handle self
def _is_self_lookup(parsed: ast.AST):
    return (
        isinstance(parsed, ast.Attribute)
        and isinstance(parsed.value, ast.Name)
        and parsed.value.id == "self"
    )


# comment out all lines starting from a given point,
# to try and make things compile. Stops when it hits
# an indent that looks like the end of the statement
def _commentlines(text, first_comment_line):
    lines = text.splitlines()

    if first_comment_line >= len(lines):
        return text

    indent = len(lines[first_comment_line]) - len(lines[first_comment_line].lstrip())
    lines[first_comment_line] = (" " * indent) + "pass #" + lines[first_comment_line]
    for i in range(first_comment_line + 1, len(lines)):
        if lines[i].strip() == "":
            continue
        if lines[i].startswith(" " * (indent + 1)):
            lines[i] = (" " * (indent + 1)) + "pass #" + lines[i]
            continue
        break

    return "\n".join(lines)


# remove any common prefix of whitespace in all lines in the string
def _remove_space_prefix(string: str, maxlines: int = 0) -> str:
    if string is None:
        return ""
    lines = [l for l in string.splitlines() if l != ""]
    prefix = -1
    for l in lines:
        if l.strip() == "":
            continue
        p = len(l) - len(l.lstrip())
        if prefix == -1 or p < prefix:
            prefix = p
    if prefix == -1:
        prefix = 0
    if maxlines > 0:
        if len(lines) > maxlines:
            lines = lines[:maxlines]
            lines.append((prefix * " ") + "...")
    return "\n".join([l[prefix:] for l in lines if l != ""])


# replace the contents of all strings with 'x' so that
# they don't affect any bracket/brace/etc parsing.
# respect escaping
def _nopstrings(string: str) -> str:
    # split to list so it's mutable
    text = list(string)

    insingle = indouble = False
    i = 0
    while i < len(text):
        if text[i] == "'":
            # if we see a ' in a double-quoted string, it's just a character
            if indouble:
                text[i] = "x"
            else:
                insingle = not insingle

            i += 1
        elif text[i] == '"':
            if insingle:
                text[i] = "x"
            else:
                indouble = not indouble
            i += 1
        elif not insingle and not indouble:
            # if we're not in a string, ignore the char
            i += 1
        else:
            # in a string of some kind

            # if not an escape character just nop it.
            if text[i] != "\\":
                text[i] = "x"
                i += 1
            else:
                # escaping something. First nop the \\ char
                text[i] = "x"
                i += 1

                # octal character
                if text[i].isdigit():
                    # convert current char to x
                    text[i] = "x"
                    i += 1
                    # and up to 3 digits
                    end = i + 3
                    while text[i] in "0123457" and i < end:
                        text[i] = "x"
                        i += 1
                # hex character
                elif text[i] == "x":
                    i += 1  # already is an x!
                    for x in range(2):  # exactly two hex digits
                        text[i] = "x"
                        i += 1
                elif text[i] == "u":
                    text[i] = "x"
                    i += 1
                    for x in range(4):  # exactly four hex digits
                        text[i] = "x"
                        i += 1
                elif text[i] == "U":
                    text[i] = "x"
                    i += 1
                    for x in range(8):  # exactly eight hex digits
                        text[i] = "x"
                        i += 1
                elif text[i] == "N":
                    # named unicode literal
                    text[i] = "x"
                    i += 1

                    # remove the {
                    text[i] = "x"
                    i += 1

                    # find the next }
                    end = text.index("}", i)
                    while i <= end:
                        text[i] = "x"
                        i += 1
                else:
                    # just nop the next char
                    text[i] = "x"
                    i += 1

    # rejoin into string
    return "".join(text)


# this function does a bulk of the work of taking a work-in-progress expression
# and figuring out which sub-expression is relevant for auto-completion or such.
#
# the expression is expected to be truncated such that the end of the expression
# string is the point of interest. So if a cursor is part-way through a line the
# rest of the line should not be included.
#
# For example in this case:
#
#   function_call(param1, param2, function_call_3(x,
#
# the sub-expression we care about is `function_call_3(x, ` as the outer function
# call is not relevant.
#
# similarly in this case:
#
#   outer_list[other_value.blah
#
# we want to find other_value.blah
#
# One different case is with function calls:
#
#  function_call(param1, [incomplete_list_comp for x in other, blah.foo
#
# In this case what we care about is blah.foo
#
# Note this function *does not* try to determine if we're part way through
# typing a function call and which argument we're on. That is handled separately
# since it cares less about subexressions and more about counting parameters
def _get_trailing_expr(expr: str) -> str:
    pure_expr = _nopstrings(expr)

    # search backwards to the first unbalanced [], {} or ()
    i = len(pure_expr) - 1
    depths = [0, 0, 0]
    depth_toks = {
        "(": (0, -1),
        ")": (0, 1),
        "[": (1, -1),
        "]": (1, 1),
        "{": (2, -1),
        "}": (2, 1),
    }
    while i > 0:
        # if this doesn't affect paren matching we can't stop due to unbalanced
        # nesting
        if pure_expr[i] not in depth_toks:
            # if it's a token that we expect to delimit the end of an expression,
            # stop now if we're not nested
            if pure_expr[i] in "=,:;{}+-/*<>&|^%@~\"'" and all(
                [x == 0 for x in depths]
            ):
                i += 1
                return pure_expr[i:].strip()
            # if this is whitespace and the previous token wasn't a . or ,
            # then stop here too
            elif (
                pure_expr[i].isspace()
                and depths == [0, 0, 0]
                and i > 1
                and pure_expr[i - 1] not in ".,"
                and not pure_expr[i - 1].isspace()
            ):
                i += 1
                return pure_expr[i:].strip()

            i -= 1
        else:
            # update nesting
            d, x = depth_toks[pure_expr[i]]
            depths[d] += x

            i -= 1

            # if we hit an unbalanced nesting, stop here
            if any([x < 0 for x in depths]):
                # start from the [ char, not what would have been the next char
                i += 1
                # don't include the unbalanced brace/paren
                i += 1

                return pure_expr[i:].strip()

    # if we didn't find anything, the whole expression is the one we care about
    return pure_expr.strip()


# for a function-call like expression which is allowed to not be fully
# syntactically correct but should be 'minimal' ie. similar to what
# the above _get_trailing_expr returns.
# this returns the function call, and the index of the last argument
def _get_func_arg(call: str) -> Tuple[str, int]:
    call = _nopstrings(call)

    # this function only handles call-like strings, that must have a trailing )
    if call[-1] != ")":
        return ("", -1)

    depth = 0
    arg_count = 0
    i = len(call) - 1
    while i > 0:
        if call[i] == ")":
            depth += 1
        elif call[i] == "(":
            depth -= 1
            # found the balanced paren, can stop here
            if depth == 0:
                return (call[0:i], arg_count)
        else:
            if depth == 1 and call[i] == ",":
                arg_count += 1

            # skip lists
            if call[i] == "]":
                i -= 1
                inner_depth = 1
                while inner_depth > 0:
                    if call[i] == "]":
                        inner_depth += 1
                    elif call[i] == "[":
                        inner_depth -= 1
                        if inner_depth == 0:
                            break
                    i -= 1

            # skip dicts
            if call[i] == "}":
                i -= 1
                inner_depth = 1
                while inner_depth > 0:
                    if call[i] == "}":
                        inner_depth += 1
                    elif call[i] == "{":
                        inner_depth -= 1
                    i -= 1

        i -= 1

    # if we failed, return nothing
    return ("", -1)


class _target_in_gen(ast.AST):
    target: ast.AST
    gen: ast.comprehension

    def __init__(self, t: ast.AST, g: ast.comprehension):
        self.target = t
        self.gen = g


# a given instance of an identifier and its type
class Ident:
    # first line this ident is valid
    line: int = -1
    # for assignments the identifier is only valid on the first line
    # before a certain column.
    # mostly relevant for overwriting assignments e.g. foo = foo.bar
    # so the idents for LHS foo and RHS foo can be differentiated
    col: int = 9999
    # the type or type hint
    type_obj: Optional[Any] = None
    # for functions without return annotations, this will be set to the AST node
    # for lazy evaluation to obtain a guessed return type.
    # We do it this way as we normally process in declaration order but
    lazy_node: Optional[ast.AST] = None
    # Any user documentation from a string following the definition
    user_doc: str = ""


# a scope - either a module, class or function
class Scope:
    # the name for debugging
    name: str
    # the parent scope, for searches upwards for identifiers
    parent: "Optional[Scope]" = None
    # the parsed node
    parsed: ast.AST
    # the type, only relevant for classes
    type_obj: Optional[Any] = None
    # known identifiers in this scope
    identifiers: Dict[str, List[Ident]]
    # for non-modules, the ident of this scope
    ident: Optional[Ident] = None
    # whether this scope is a class or not (for finding `self`)
    is_class: bool = False

    # the last identifier added
    _last_ident_added: str = ""

    def __init__(self):
        self.identifiers = {}

    def set_ident(self, name: str, ident: Ident):
        if name not in self.identifiers:
            self.identifiers[name] = []
        self.identifiers[name] += [ident]
        self._last_ident_added = name

    def set_ident_docs(self, docs: str):
        if self._last_ident_added == "":
            self.type_obj.user_doc = docs
        else:
            self.identifiers[self._last_ident_added][-1].user_doc = docs

    # look up the version of an identifier on a given line, in our parents,
    # or in the builtins
    def get_ident(self, name: str, line: int, col: int):
        ret = None
        if name in self.identifiers:
            for i in self.identifiers[name]:
                # only consider identifiers that are valid for the line & col
                # we're searching for
                if (
                    i.line < line
                    or (i.line == line and (col < i.col or col == -1))
                    or line == -1
                ):
                    # if we don't have a match, or this match is more recent, use it
                    if ret is None or ret.line < i.line:
                        ret = i

        # if we don't have a record of it, or we're at a statement before
        # the first assignment, search the parent at our declaration line
        if ret is None:
            if self.parent is not None:
                return self.parent.get_ident(
                    name, self.ident.line if self.ident is not None else line, col
                )

            if name in dir(builtins):
                return getattr(builtins, name)

            return None

        return ret

    def full_name(self):
        if self.parent is not None:
            return f"{self.parent.full_name()}::{self.name}"
        return self.name

    def __repr__(self):
        return f"<Scope '{self.full_name()}'>"


class UserClass:
    # name of the class
    name: str = ""
    # Any user documentation from a string immediately as the first member
    user_doc: str = ""
    # the class's Ident
    ident: Ident
    # the class's Scope
    scope: Scope

    def __init__(self, n: str, i: Ident, s: Scope):
        self.name = n
        self.ident = i
        self.scope = s

    # this is hack, we want to look callable so that instances of UserClass
    # can be treated as types in generics like List[], but nothing should
    # actually call us :)
    def __call__(self):
        raise RuntimeError("we shouldn't call this!")


class UserFunc:
    # name of the function, if it's a member function
    name: str = ""
    # the callable type, for introspection
    callable_type: Any = Callable[..., Any]
    # Any user documentation from a string as the first definition
    user_doc: str = ""

    def __init__(self, n: str, t: Any):
        self.name = n
        self.callable_type = t


# Main class, reflects a given source text (if it can) and allows
# lookups of the types of expressions as well as auto-completion
# of partial expressions
class PyReflector:
    # a lookup of modules to alias, e.g. to a stubs module that has better docs
    # and type hints
    alias_modules: Dict[str, Any] = {}

    def __init__(self, text: str, starting_globals: Dict[str, Any], debug_types: bool):
        # the parsed module, or None if parsing completely failed
        self.module: Optional[ast.Module]

        # for tests - a lookup to retrieve the actual typevar since they can't be compared by name
        self.user_types: Dict[str, UserClass] = {}

        # the text that was actually parsed, including any truncation/commenting needed
        # to get it to compile
        self.parsed_text: str

        # an error if parsing completely failed
        self.parse_error: Optional[str]

        # the starting set of globals to consider the module populated with
        self.starting_globals = starting_globals.copy()
        if self.starting_globals is None:
            self.starting_globals = globals().copy()

        # apply module aliases
        for k in self.starting_globals.keys():
            if (
                k in PyReflector.alias_modules
                and self.starting_globals[k] == sys.modules[k]
            ):
                self.starting_globals[k] = PyReflector.alias_modules[k]

        # whether or not type-processing should be debugged. Instead of falling back
        # to `typing.Any` for unknown types, instead a string bounded by "@@" is returned.
        # Mostly for internal use
        self.debug_types = debug_types

        # the current scope for each line
        self.scopes: List[Scope] = []

        # a pending FIFO which we process classes and functions in. This is used
        # so that if we want to infer a type we can steal it early and process functions
        # out of normal declaration order. Can't resolve mutually-recursive functions
        # that require type guessing but improves many common situations where a class
        # method calls another that's declared later and doesn't have proper type
        # annotations
        self.pending: List[Tuple[Optional[Scope], ast.AST]] = []

        # the last ident processed in _get_type, used for user function completion. A bit
        # of a hack to avoid needing a full parallel _get_type equivalent for looking
        # up identifiers, or making _get_type do both
        self._last_ident: Optional[Ident] = None

        # try to parse the text
        self._parse_text(text)

        # This checks module against None internally to help type checkers
        self._process_module()

    def _parse_text(self, text: str):
        # try a simple parse. If there are no syntax errors this will
        # succeed.
        try:
            self.module = ast.parse(text)
            self.parsed_text = text
            self.parse_error = None
            return
        except SyntaxError as err:
            if err.lineno is None:
                raise err
            first_comment_line = err.lineno - 1

        # when encountering an error, comment everything
        # from the error line to the next line with same or
        # less indent (excluding blank lines) and try again

        mod = _commentlines(text, first_comment_line)

        try:
            self.module = ast.parse(mod)
            self.parsed_text = mod
            self.parse_error = None
            return
        except SyntaxError as err2:
            if err2.lineno is None:
                raise err2

            # if the error has moved to a later line, that suggests the
            # original error was reported from some previous line, so we
            # should try from an earlier point
            # if not, we can't recover this
            if err2.lineno <= first_comment_line + 1:
                self.module = None
                self.parsed_text = mod
                self.parse_error = "Error remained after comments"

        # first see where we can truncate to and successfully parse
        # (up to 10 lines of non-blank lines truncated, to limit scope)

        lines = text.splitlines()

        trunc_lines = lines[0 : first_comment_line + 1]
        removed = 0
        while len(trunc_lines) > 0:
            if trunc_lines[-1].strip() == "":
                del trunc_lines[-1]
                continue

            del trunc_lines[-1]
            removed += 1

            if len(trunc_lines) == 0:
                break

            last_line = trunc_lines[-1].rstrip()
            if last_line != "" and last_line[-1] == ":":
                trunc_lines[-1] += " pass"

            try:
                parsed = ast.parse("\n".join(trunc_lines))
                text = "\n".join(lines)
                break
            except Exception:
                if removed >= 10:
                    self.module = None
                    self.parsed_text = "\n".join(trunc_lines)
                    self.parse_error = "Couldn't backtrack"

        # now we know that trunc_lines parses,
        # retry commenting starting from there
        mod = _commentlines(text, len(trunc_lines))

        try:
            self.module = ast.parse(mod)
            self.parsed_text = mod
            self.parse_error = None
            return
        except Exception:
            self.module = None
            self.parsed_text = mod
            self.parse_error = "Error remained after comments"

    def valid(self):
        return self.module is not None

    # get the source string for a given line
    def get_line_source(self, line: int):
        return self.parsed_text.splitlines()[line - 1]

    def _type_failure(self, err: str):
        if self.debug_types:
            return "@@" + err.replace("@", "") + "@@"
        return Any

    def _add_extras_for_gen(
        self,
        scope: Scope,
        tmp_types: Optional[Dict[str, Any]],
        generators: List[ast.comprehension],
    ) -> Dict[str, Any]:
        if tmp_types is None:
            extras = {}
        else:
            extras = tmp_types.copy()

        for g in generators:
            iter: Any = self._get_type(scope, g.iter, tmp_types)

            if _is_generic(List, iter):
                iter_type = iter.__args__[0]
            elif _is_generic(Tuple, iter):
                if len(set(iter.__args__)) == 1:
                    iter_type = iter.__args__[0]
                else:
                    raise TypeError(f"Ambiguous tuple list comp on {iter}")
            else:
                raise TypeError(f"Unhandled iter list comp on {iter}")

            if isinstance(g.target, ast.Name):
                extras[g.target.id] = iter_type
            elif (
                isinstance(g.target, ast.Tuple)
                and _is_generic(Tuple, iter_type)
                and len(g.target.elts) == len(iter_type.__args__)
            ):
                for i, e in enumerate(g.target.elts):
                    if not isinstance(e, ast.Name):
                        raise TypeError(f"Failed unpacking list comp {i} on {e}")
                    extras[e.id] = iter_type.__args__[i]

        return extras

    def _handle_aliases(self, in_type: Any) -> Any:
        out_type = in_type

        # see if this type looks like it's in one of the modules we're aliasing from
        # and look up the desired type. This is generally expected to have better
        # type annotations

        for mod in PyReflector.alias_modules.keys():
            if mod not in sys.modules:
                continue

            if hasattr(in_type, "__qualname__"):
                attrpath = getattr(in_type, "__qualname__")

                x = _lookup_attrpath(sys.modules[mod], getattr(in_type, "__qualname__"))

                if x == in_type:
                    return _lookup_attrpath(
                        PyReflector.alias_modules[mod], getattr(in_type, "__qualname__")
                    )

            elif hasattr(in_type, "__class__"):
                cl = getattr(in_type, "__class__")

                names = [
                    x
                    for x in dir(sys.modules[mod])
                    if getattr(sys.modules[mod], x) == cl
                ]

                if len(names) == 1:
                    cl_name = names[0]

                    return getattr(PyReflector.alias_modules[mod], cl_name)

        return out_type

    def _get_type(
        self,
        scope: Scope,
        parsed: Optional[ast.AST],
        tmp_types: Optional[Dict[str, Any]] = None,
    ) -> Any:
        self._last_ident = None

        # simple protection and helps the type checker, silently drop None
        if parsed is None:
            return None

        # for things that are names, get the ident and look up its instance.
        # first check tmp_types for things like temporary objects inside list comprehensions
        # that we don't create proper identifiers for
        name = ""
        line, col = -1, -1
        if isinstance(parsed, ast.Name):
            name = parsed.id
            line, col = parsed.lineno, parsed.col_offset
        if isinstance(parsed, ast.arg):
            name = parsed.arg
            line, col = parsed.lineno, -1
        if isinstance(parsed, ast.alias):
            name = parsed.name
            if parsed.asname is not None:
                name = parsed.asname
            line = getattr(parsed, "lineno", -1)
            col = getattr(parsed, "col_offset", -1)

        if name != "":
            if tmp_types is not None and name in tmp_types:
                return tmp_types[name]
            ident = scope.get_ident(name, line, col)
            if ident is None:
                return self._type_failure(f"Unknown name {name}")
            if isinstance(ident, Ident):
                # if this is a function call that has a lazy_node, and it's in
                # our pending list (ie. not already on the current stack somewhere
                # due to mutual recursion) process it now so we can get a better
                # type object from inferring its return type
                if ident.lazy_node is not None:
                    for i, p in enumerate(self.pending):
                        if p[1] == ident.lazy_node:
                            self._process_pending(i)
                            break

                self._last_ident = ident
                return self._handle_aliases(ident.type_obj)
            return ident

        if isinstance(parsed, _target_in_gen):
            try:
                extras = self._add_extras_for_gen(scope, tmp_types, [parsed.gen])
            except TypeError as err:
                return self._type_failure(str(err))

            return self._get_type(scope, parsed.target, extras)

        if isinstance(parsed, ast.Attribute):
            # detect single-level self.foo and look up in parent if it exists
            # these identifiers are stored in the parent scope so they're available
            # to all self members
            if _is_self_lookup(parsed) and scope.parent is not None:
                self_lookup = scope.get_ident(
                    "self", parsed.value.lineno, parsed.value.col_offset
                )
                if self_lookup is not None:
                    # find the parent class, it could be multiple steps up if this is
                    # a nested function
                    parent_scope = scope.parent
                    while not parent_scope.is_class and parent_scope.parent is not None:
                        parent_scope = parent_scope.parent

                    ret = parent_scope.get_ident(parsed.attr, -1, -1)

                    if ret is None:
                        return ret

                    # if this is a function call that has a lazy_node, and it's in
                    # our pending list (ie. not already on the current stack somewhere
                    # due to mutual recursion) process it now so we can get a better
                    # type object from inferring its return type
                    if ret.lazy_node is not None:
                        for i, p in enumerate(self.pending):
                            if p[1] == ret.lazy_node:
                                self._process_pending(i)
                                break

                    self._last_ident = ret
                    return self._handle_aliases(ret.type_obj)

            # get the type of the base object that we're looking up
            base = self._get_type(scope, parsed.value, tmp_types)
            if isinstance(base, str):
                return self._type_failure(f"{base}, looking up {parsed.attr}")

            if base is None:
                if self.debug_types:
                    return self._type_failure(
                        f"Unexpected None in base {parsed.value} for access {parsed.attr}"
                    )
                return Any

            base = _unwrap_optional(base)

            if not hasattr(base, parsed.attr):
                # if the base is a typevar, it's a user defined type let's
                # see if this is a member we know about
                if isinstance(base, UserClass):
                    base_ident = scope.get_ident(
                        base.name, parsed.value.lineno, parsed.value.col_offset
                    )
                    if base_ident is not None:
                        base_scope = self.scopes[base_ident.line]

                        # if this scope is in our pending list then process it now
                        # so we can get a complete type object
                        for i, p in enumerate(self.pending):
                            if p[0] == base_scope:
                                self._process_pending(i)
                                break

                        attr_ident = base_scope.get_ident(
                            parsed.attr, parsed.value.lineno, parsed.value.col_offset
                        )

                        if attr_ident is not None:
                            self._last_ident = attr_ident
                            return self._handle_aliases(attr_ident.type_obj)
                return self._type_failure(
                    f"Attribute {parsed.attr} not found in {parsed.value}"
                )

            ret = getattr(base, parsed.attr)

            # if we got to a getset descriptor, try to see if we can reverse lookup our aliases to find the documented type
            if inspect.isgetsetdescriptor(ret):
                if hasattr(ret, "__objclass__") and hasattr(ret, "__name__"):
                    obj = ret.__objclass__

                    for k in PyReflector.alias_modules.keys():
                        for x in dir(sys.modules[k]):
                            if getattr(sys.modules[k], x) == obj:
                                doc_obj = getattr(PyReflector.alias_modules[k], x)
                                ret = getattr(doc_obj, ret.__name__)

            # if this is a property return the type that calling the property getter would return
            if isinstance(ret, property) and ret.fget is not None:
                try:
                    ret = inspect.signature(ret.fget).return_annotation

                    if isinstance(ret, str) and hasattr(base, "__module__"):
                        mod: str = getattr(base, "__module__")
                        if mod in sys.modules and hasattr(sys.modules[mod], ret):
                            ret = getattr(sys.modules[mod], ret)

                    ret = self._handle_aliases(ret)
                except:
                    return self._type_failure(
                        f"Failed to inspect property {parsed.attr}"
                    )

            # if this is a plain object return its type, if it's some kind of callable
            # then return the object directly as it is a type
            if (
                ret is not None
                and not inspect.isclass(ret)
                and not inspect.isfunction(ret)
                and not inspect.isbuiltin(ret)
                and not inspect.ismethod(ret)
                and not inspect.ismethoddescriptor(ret)
            ):
                if not hasattr(ret, "__origin__") or not inspect.isclass(
                    getattr(ret, "__origin__")
                ):
                    ret = type(ret)

            return ret

        if isinstance(parsed, ast.Expr):
            return self._get_type(scope, parsed.value, tmp_types)

        if isinstance(parsed, ast.Call):
            func = self._get_type(scope, parsed.func, tmp_types)

            # special case a bunch of builtins that don't have proper type
            # annotations. Not strictly needed as we don't expect to do anything,
            # but useful for debugging
            # if we could process typeshed stubs we could do away with this, but
            # those don't parse and need a dedicated separate parser
            if func is len:
                return int
            if func is range:
                return List[int]
            if func is max or func is min or func is reversed or func is sorted:
                return self._get_type(scope, parsed.args[0], tmp_types)
            if func is any or func is all or func is isinstance or func is issubclass:
                return bool
            if func is dir:
                return dict
            if func is cast:
                return self._get_type(scope, parsed.args[0], tmp_types)

            if func is enumerate:
                seq_type = self._get_type(scope, parsed.args[0], tmp_types)
                inner_type = Any
                if _is_generic(List, seq_type):
                    inner_type = seq_type.__args__[0]
                return Tuple[int, inner_type]

            if func is next:
                seq_type = self._get_type(scope, parsed.args[0], tmp_types)
                inner_type = Any
                if _is_generic(List, seq_type):
                    inner_type = seq_type.__args__[0]
                return inner_type

            if func is str.format:
                return str
            if func is list.index:
                return int
            if func is struct.unpack or func is struct.unpack_from:
                return Tuple[Any, ...]

            if isinstance(func, UserFunc):
                func = func.callable_type

            if _is_generic(Callable, func):
                ret = func.__args__[-1]
                if ret == type(None):
                    ret = None

                return ret

            if func is list:
                seq_type = self._get_type(scope, parsed.args[0], tmp_types)
                if _is_generic(List, seq_type):
                    return seq_type

            if type(func) is type or type(func) is UserClass:
                return func

            if func is None:
                return self._type_failure(
                    f"Invalid None looking up callable {parsed.func}"
                )

            # if it doesn't fall into the above cases, try to use inspect to get it from the signature
            try:
                sig = inspect.signature(func)
                ret = sig.return_annotation
                # assume this is a builtin with missing docs
                if ret == inspect.Signature.empty:
                    return Any
                if isinstance(ret, str) and hasattr(func, "__module__"):
                    mod: str = getattr(func, "__module__")
                    if mod in sys.modules and hasattr(sys.modules[mod], ret):
                        ret = getattr(sys.modules[mod], ret)
                return self._handle_aliases(ret)
            except:
                return self._type_failure(f"Failed to inspect signature of {func}")

        # unpack the elements in a tuple to generate the type for it
        if isinstance(parsed, ast.Tuple):
            params = tuple([self._get_type(scope, e, tmp_types) for e in parsed.elts])
            # if we have type failures enabled, returned types can be error strings
            if any([isinstance(x, str) for x in params]):
                return self._type_failure(f"Failed tuple[{params}]")
            return Tuple[params]

        if isinstance(parsed, ast.Dict):
            if len(parsed.keys) == 0:
                return Dict[Any, Any]
            keytypes = list(
                set([self._get_type(scope, e, tmp_types) for e in parsed.keys])
            )
            valtypes = list(
                set([self._get_type(scope, e, tmp_types) for e in parsed.values])
            )
            keytype, valtype = Any, Any
            if len(keytypes) == 1:
                keytype = keytypes[0]
            if len(valtypes) == 1:
                valtype = valtypes[0]
            return Dict[keytype, valtype]

        if isinstance(parsed, ast.List):
            if len(parsed.elts) == 0:
                return List[Any]

            x = self._get_type(scope, parsed.elts[0], tmp_types)
            if isinstance(x, str):
                return self._type_failure(f"Failed list[{x}]")

            return List[x]

        # subscripts could either be generic type declarations or indices into a
        # sequence type. We only handle standard generics
        if isinstance(parsed, ast.Subscript):
            coll_type = self._get_type(scope, parsed.value, tmp_types)

            slice = parsed.slice
            if sys.version_info < (3, 9):
                if isinstance(slice, ast.Index):
                    slice = slice.value

            # handle type annotations which directly subscript these generics in the AST
            # e.g. foo: List[int] = blah()
            if coll_type is List:
                return List[self._get_type(scope, slice, tmp_types)]
            if coll_type is Optional:
                return Optional[self._get_type(scope, slice, tmp_types)]
            if coll_type is Tuple and isinstance(slice, ast.Tuple):
                inners = tuple(
                    [self._get_type(scope, x, tmp_types) for x in slice.elts]
                )
                return Tuple[inners]
            if coll_type is Dict and isinstance(slice, ast.Tuple):
                key = self._get_type(scope, slice.elts[0], tmp_types)
                value = self._get_type(scope, slice.elts[1], tmp_types)
                return Dict[key, value]
            if coll_type is Callable and isinstance(slice, ast.Tuple):
                params = slice.elts[0]
                ret = self._get_type(scope, slice.elts[1], tmp_types)
                if isinstance(params, ast.List):
                    return Callable[
                        [self._get_type(scope, x, tmp_types) for x in params.elts], ret
                    ]
                else:
                    return Callable[..., ret]

            # handle an object which is a given standard type, e.g. `foo[5]` when `foo` is a List[int]
            c: Any = coll_type
            if _is_generic(List, c):
                # list slices return the same type
                if isinstance(slice, ast.Slice) and slice.upper is not None:
                    return c
                if not hasattr(c, "__args__") or c.__args__ is None:
                    return Any
                return c.__args__[0]
            if _is_generic(Optional, c):
                if not hasattr(c, "__args__") or c.__args__ is None:
                    return Any
                return c.__args__[0]
            if _is_generic(Dict, c):
                if not hasattr(c, "__args__") or c.__args__ is None:
                    return Any
                return c.__args__[1]
            if _is_generic(Tuple, c):
                # tuple slices return the same type
                if isinstance(slice, ast.Slice) and slice.upper is not None:
                    return c
                if hasattr(c, "__args__") and len(set(c.__args__)) == 1:
                    return c.__args__[0]
                if isinstance(slice, ast.Constant) and isinstance(slice.value, int):
                    i = slice.value
                    if hasattr(c, "__args__") and i < len(set(c.__args__)):
                        return c.__args__[i]

                # if the tuple isn't identically typed, we stop typing
                return self._type_failure(f"Ambiguous Tuple subscript {c}")

            # any other subscript, we don't attempt to generate type hints for
            return self._type_failure(f"Unknown subscripted type {c}")

        if isinstance(parsed, ast.Lambda):
            return Callable[..., Any]

        if isinstance(parsed, ast.FunctionDef):
            ret = None
            if parsed.returns is not None:
                ret = self._get_type(scope, parsed.returns, tmp_types)
            else:
                funcscope = self.scopes[parsed.lineno].ident
                if funcscope is not None:
                    ret = funcscope.type_obj.callable_type.__args__[-1]

            userfunc = UserFunc(parsed.name, Callable[..., Any])

            # don't generate args for 'complex' functions
            args = parsed.args
            if (
                args.kwarg is not None
                or args.vararg is not None
                or len(args.kwonlyargs) > 0
            ):
                userfunc.callable_type = Callable[(..., ret)]
                return userfunc

            if "posonlyargs" in args._fields and len(args.posonlyargs) > 0:
                userfunc.callable_type = Callable[(..., ret)]
                return userfunc

            # Callable isn't designed for methods, drop the self argument
            first = 0
            if isinstance(scope.parsed, ast.ClassDef) and args.args[0].arg == "self":
                first = 1

            arg_list = []
            for i in range(first, len(args.args)):
                annot = args.args[i].annotation
                if annot is None:
                    arg_list += [Any]
                else:
                    arg_list += [self._get_type(scope, annot, tmp_types)]

            userfunc.callable_type = Callable[
                (
                    [a for a in arg_list],
                    ret,
                )
            ]

            return userfunc

        if isinstance(parsed, ast.Constant):
            if parsed.value is None:
                return None
            return type(parsed.value)

        # for if expressions, assume that the types won't vary between each
        # branch and return the 'main' branch
        if isinstance(parsed, ast.IfExp):
            return self._get_type(scope, parsed.body, tmp_types)

        # for list comprehensions / generators we generate a tmp type for the iterator value
        if isinstance(parsed, ast.ListComp) or isinstance(parsed, ast.GeneratorExp):
            try:
                extras = self._add_extras_for_gen(scope, tmp_types, parsed.generators)
            except TypeError as err:
                return self._type_failure(str(err))

            inner = self._get_type(scope, parsed.elt, extras)
            if isinstance(inner, str):
                return self._type_failure(f"Failed Listcomp {inner}")
            return List[inner]

        # assume simple ops can be mostly type modeled as if they always return
        # the LHS type. Not true for int * float or int * str but close enough
        if isinstance(parsed, ast.BinOp):
            return self._get_type(scope, parsed.left, tmp_types)
        if isinstance(parsed, ast.UnaryOp):
            return self._get_type(scope, parsed.operand, tmp_types)
        # similarly, comparisons/bools don't consider overloads and just assume bool return
        if isinstance(parsed, ast.Compare) or isinstance(parsed, ast.BoolOp):
            return bool

        if sys.version_info >= (3, 14):
            if isinstance(parsed, ast.JoinedStr) or isinstance(parsed, ast.TemplateStr):
                return str

        # legacy types before consolidation into constant
        if sys.version_info < (3, 8):
            if isinstance(parsed, ast.Num):
                return type(parsed.n)
            if isinstance(parsed, ast.Str):
                return str
            if isinstance(parsed, ast.Bytes):
                return bytes
            if isinstance(parsed, ast.NameConstant):
                if parsed.value is None:
                    return None
                return type(parsed.value)

        return self._type_failure(f"General Type-lookup failure {parsed}")

    # we can try to guess function return values by looking at the types of
    # the return statements. If they are all the same (ignoring possible none)
    # then use that. If they're different we give up as we don't handle union/
    # varied types
    def _guess_return_value(self, scope: Scope, node: ast.AST):
        ret_types = [
            self._get_type(scope, r.value) for r in _get_return_statements(node)
        ]
        ret_types = list(set([r for r in ret_types if r is not None]))

        if len(ret_types) == 1 and not isinstance(ret_types[0], str):
            return ret_types[0]
        return type(None)

    # for a single statement, process the identifiers it creates and recurse
    # as needed (but not into new scopes)
    def _process_stmt(self, parent: Optional[Scope], parsed: ast.AST):
        if parent is None:
            raise ValueError("Expected parent for non-module")

        # for functions and classes we register their type as an identifier and update
        # scopes, but push them onto the pending list and continue processing.
        if isinstance(parsed, ast.ClassDef):
            classscope = Scope()
            id = Ident()

            classscope.name = f"class {parsed.name}"
            classscope.parent = parent
            classscope.parsed = parsed
            classscope.type_obj = UserClass(parsed.name, id, classscope)  # type: ignore
            self.user_types[parsed.name] = classscope.type_obj
            classscope.is_class = True

            r = _get_linerange(parsed)

            for line in range(r[0], r[1] + 1):
                self.scopes[line] = classscope

            id.line = parsed.lineno
            id.type_obj = classscope.type_obj
            classscope.ident = id
            parent.set_ident(parsed.name, id)

            self.pending.append((classscope, parsed))

            return
        elif isinstance(parsed, ast.FunctionDef) or isinstance(
            parsed, ast.AsyncFunctionDef
        ):
            funcscope = Scope()
            funcscope.name = f"function {parsed.name}"
            funcscope.parent = parent
            funcscope.parsed = parsed
            funcscope.is_class = False

            r = _get_linerange(parsed)

            for line in range(r[0], r[1] + 1):
                self.scopes[line] = funcscope

            id = Ident()
            id.line = parsed.lineno
            id.type_obj = self._get_type(parent, parsed)
            # if there's no return annotation, we'll try to guess it
            # later when this function gets processed
            if parsed.returns is None:
                id.lazy_node = parsed
            else:
                # if we know the return type already and this is a @property
                # then pretend it is just a member of that type, not a function
                if any(
                    [
                        isinstance(a, ast.Name) and a.id == "property"
                        for a in parsed.decorator_list
                    ]
                ):
                    id.type_obj = self._get_type(parent, parsed.returns)

            prop_setter = False

            # if this one is a property setter, @self.setter, then
            # don't add it as an ident
            if any(
                [
                    isinstance(a, ast.Attribute)
                    and a.attr == "setter"
                    and isinstance(a.value, ast.Name)
                    and a.value.id == parsed.name
                    for a in parsed.decorator_list
                ]
            ):
                prop_setter = True

            funcscope.ident = id
            if not prop_setter:
                parent.set_ident(parsed.name, id)

            args = parsed.args
            arg_list = []
            if "posonlyargs" in args._fields:
                arg_list += args.posonlyargs
            arg_list += args.args

            # resize up the defaults array to the right size, defaults are 'trailing'
            # ie. if there are fewer defaults than arguments, the first ones (starting
            # from position-only arguments) have defaults omitted
            defaults = [None] * (len(arg_list) - len(args.defaults)) + args.defaults

            # len(kwonlyargs) == len(kw_defaults) because keyword defaults can come in
            # any order
            arg_list += args.kwonlyargs
            defaults += args.kw_defaults

            for i, a in enumerate(arg_list):
                id = Ident()
                id.line = parsed.lineno
                default_val = defaults[i]
                if a.annotation is not None:
                    id.type_obj = self._get_type(parent, a.annotation)
                elif default_val is not None:
                    id.type_obj = self._get_type(parent, default_val)
                elif (
                    a in parsed.args.args
                    and parsed.args.args.index(a) == 0
                    and a.arg == "self"
                    and parent.parent is not None
                ):
                    id.type_obj = parent.type_obj
                else:
                    if self.debug_types:
                        id.type_obj = self._type_failure(
                            f"Unknown parameter type {a.arg} in {parsed.name}"
                        )
                    id.type_obj = Any
                funcscope.set_ident(a.arg, id)

            self.pending.append((funcscope, parsed))

            return

        # for imports, try to import the module ourselves so that we can have proper types.
        # this won't work well for relative imports or things that need a particular sys.path
        # but will work for standard library modules
        if isinstance(parsed, ast.Import):
            for alias in parsed.names:
                n = alias.asname
                if n is None or n == "":
                    n = alias.name

                id = Ident()
                id.line = parsed.lineno
                try:
                    if alias.name in PyReflector.alias_modules:
                        id.type_obj = PyReflector.alias_modules[alias.name]
                    else:
                        id.type_obj = __import__(alias.name, globals(), locals())
                except ImportError:
                    if self.debug_types:
                        print(f"Couldn't import {alias.name}")
                    id.type_obj = Any
                parent.set_ident(n, id)

        if isinstance(parsed, ast.ImportFrom):
            module = None
            if parsed.module is not None:
                try:
                    if parsed.module in PyReflector.alias_modules:
                        module = PyReflector.alias_modules[parsed.module]
                    else:
                        module = __import__(
                            parsed.module,
                            globals(),
                            locals(),
                            [a.name for a in parsed.names],
                            parsed.level,
                        )
                except ImportError:
                    if self.debug_types:
                        print(f"Couldn't import {parsed.module}")
                    module = None
            for alias in parsed.names:
                n = alias.asname
                if n is None or n == "":
                    n = alias.name

                id = Ident()
                id.line = parsed.lineno
                if module is None:
                    try:
                        if parsed.module in PyReflector.alias_modules:
                            id.type_obj = PyReflector.alias_modules[alias.name]
                        else:
                            id.type_obj = __import__(
                                alias.name, globals(), locals(), [], parsed.level
                            )
                    except ImportError:
                        if self.debug_types:
                            print(f"Couldn't import {alias.name}")
                        id.type_obj = Any
                else:
                    if hasattr(module, alias.name):
                        id.type_obj = getattr(module, alias.name)
                    else:
                        id.type_obj = Any
                parent.set_ident(n, id)

        # global/nonlocal we ignore for now, we assume the type won't
        # change with any assignments there

        targets = []
        values = []
        unpacking = False
        ident_col = None

        # AugAssign doesn't create a new object, so ignore it

        # for other things that create a new identifier register both the set of
        # target names and the source values. For pure assignments note the
        # column where the LHS ends so that we can identify both possibly different
        # types of `foo` in the statement `foo = foo.bar`

        if isinstance(parsed, ast.For) or isinstance(parsed, ast.AsyncFor):
            if isinstance(parsed.target, ast.Tuple) or isinstance(
                parsed.target, ast.List
            ):
                targets = parsed.target.elts
            else:
                targets = [parsed.target]
            values = [parsed.iter] * len(targets)
            unpacking = True

        # unless this actually creates a new name we don't have to do anything
        if isinstance(parsed, ast.With) or isinstance(parsed, ast.AsyncWith):
            for item in parsed.items:
                if item.optional_vars is not None:
                    if isinstance(item.optional_vars, ast.Name):
                        targets += [item.optional_vars]
                        values += [item.context_expr]
                    elif isinstance(item.optional_vars, ast.Tuple):
                        unpacking = True
                        targets += item.optional_vars.elts
                        values += [item.context_expr] * len(item.optional_vars.elts)

        # for annotated assignments, trust the annotation and don't try to evaluate the
        # actual RHS
        if isinstance(parsed, ast.AnnAssign):
            targets += [parsed.target]
            values += [parsed.annotation]
            if parsed.value is not None:
                ident_col = parsed.value.col_offset

        if isinstance(parsed, ast.Assign):
            if len(parsed.targets) == 1 and (
                isinstance(parsed.targets[0], ast.Tuple)
                or isinstance(parsed.targets[0], ast.List)
            ):
                targets = parsed.targets[0].elts
                unpacking = True
            else:
                targets = parsed.targets
            values = [parsed.value] * len(targets)
            if parsed.value is not None:
                ident_col = parsed.value.col_offset

        # starred has no effect for our purposes
        for i in range(len(targets)):
            t = targets[i]
            while isinstance(t, ast.Starred):
                t = t.value
            targets[i] = t

        # we were in control of these arrays so they should be identically sized
        if len(targets) != len(values):
            raise RuntimeError("Didn't get equal number of targets and values")

        for i in range(len(targets)):
            t = targets[i]

            ident_scope = parent
            if isinstance(t, ast.Name):
                name = t.id

                if isinstance(t.ctx, ast.Load):
                    raise ValueError("Didn't expect loading target")
            elif _is_self_lookup(t) and parent.parent is not None:
                # just to help the type checked, is_self_lookup already checked this
                if isinstance(t, ast.Attribute):
                    name = t.attr
                    ident_scope = parent.parent

                    # walk up to the class, in case of nested functions
                    while (
                        ident_scope.type_obj is None and ident_scope.parent is not None
                    ):
                        ident_scope = ident_scope.parent

                    if isinstance(t.ctx, ast.Load):
                        raise ValueError("Didn't expect loading target")
                else:
                    raise RuntimeError("invalid")
            else:
                # otherwise do nothing, this is an assignment of a value and we don't
                # track fully dynamic types and attributes
                continue

            id = Ident()
            id.line = _get_linerange(parsed)[0]
            v = values[i]
            id.type_obj = self._get_type(parent, v)

            if unpacking:
                t: Any = id.type_obj
                if _is_generic(Tuple, t):
                    # either out of bounds, or a `Tuple[...]`, either way call it Any
                    if i < len(t.__args__):
                        id.type_obj = t.__args__[i]
                    else:
                        id.type_obj = Any
                elif _is_generic(List, t):
                    id.type_obj = t.__args__[0]
                else:
                    id.type_obj = t

            if ident_col is not None:
                id.col = ident_col
            ident_scope.set_ident(name, id)

        # should only get here for things like loops, ifs, etc NOT for classes and functions
        if _is_scope_node(parsed):
            raise TypeError("Should not be recursing for scope node")

        for recurse in ["body", "orelse", "finalbody"]:
            if recurse in parsed._fields:
                for e in getattr(parsed, recurse):
                    self._process_stmt(parent, e)

    # function to process the n'th item in the pending list. Usually 0 to
    # continue processing in declaration order but can be out-of-order if we
    # want to crystallise a guessed return type for a function in order to
    # get a better type at an earlier callsite
    def _process_pending(self, idx: int):
        scope, node = self.pending.pop(idx)

        # node is a module, class, or function. Process all the identifiers in it
        # and add any nested classes or functions to the pending list
        if _is_scope_node(node):
            for i, st in enumerate(getattr(node, "body")):
                self._process_stmt(scope, st)

                if (
                    isinstance(st, ast.Expr)
                    and isinstance(st.value, ast.Constant)
                    and isinstance(st.value.value, str)
                ):
                    if isinstance(node, ast.ClassDef):
                        scope.set_ident_docs(st.value.value)
                    elif isinstance(node, ast.FunctionDef) and i == 0:
                        scope.ident.type_obj.user_doc = st.value.value

            # for functions that want guessed return types (lazy_node is not None)
            # do that now
            if (
                scope is not None
                and scope.ident is not None
                and scope.ident.lazy_node is not None
            ):
                if scope.ident.type_obj is not None:
                    callable = scope.ident.type_obj
                    if isinstance(scope.ident.type_obj, UserFunc):
                        callable = callable.callable_type
                    args = callable.__args__[0:-1]
                    ret_type = self._guess_return_value(scope, scope.ident.lazy_node)

                    # if this function was a property, don't make a callable just set
                    # the return type
                    new_type = callable
                    if isinstance(scope.ident.lazy_node, ast.FunctionDef) and any(
                        [
                            isinstance(a, ast.Name) and a.id == "property"
                            for a in scope.ident.lazy_node.decorator_list
                        ]
                    ):
                        scope.ident.type_obj = new_type = ret_type
                    elif len(args) == 1 and args[0] == ...:
                        new_type = Callable[(..., ret_type)]
                    else:
                        new_type = Callable[[a for a in args], ret_type]

                    if isinstance(scope.ident.type_obj, UserFunc):
                        scope.ident.type_obj.callable_type = new_type
                    else:
                        scope.ident.type_obj = new_type
                scope.ident.lazy_node = None
                pass
        else:
            raise TypeError("Unexpected type of object in pending list")

    def _process_module(self):
        if self.module is not None:

            # start with just the module
            modscope = Scope()
            modscope.name = "module"
            modscope.parsed = self.module
            modscope.parent = None
            modscope.is_class = False

            # set all globals, ignoring reserved ones with __ prefix - so this can be
            # globals() without needing extra filtering
            for k, v in self.starting_globals.items():
                if k.startswith("__"):
                    continue
                id = Ident()
                id.line = 0
                id.type_obj = self._handle_aliases(v)
                modscope.set_ident(k, id)

            # modules don't have line ranges, so go to the last entry in the body
            if len(self.module.body) > 0:
                r = _get_linerange(self.module.body[-1])
            else:
                self.scopes = [modscope]
                return

            # start with every line pointing to the module scope
            self.scopes = [modscope] * (r[1] + 1)

            for e in self.module.body:
                self._process_stmt(modscope, e)

            while len(self.pending) > 0:
                self._process_pending(0)

    # walk into things like function calls and list definitions/comprehensions to find
    # the atomic expression that we can grab the type of. This is expected to return
    # from either a name or an attribute lookup but could also return a function or class
    # type if the location is on their definition
    def _get_atom_expr(self, parsed: ast.AST, line: int, col: int) -> Optional[ast.AST]:
        # we expect this to be present but it's not guaranteed
        end_col_offset = getattr(parsed, "end_col_offset", 9999)
        line_range = _get_linerange(parsed)
        col_range = _get_colrange(parsed)

        # early out if this node doesn't contain the desired location
        if line_range[0] >= 0 and (line < line_range[0] or line > line_range[1]):
            return None

        if (
            col_range[0] >= 0
            and line_range[0] == line_range[1]
            and (col < col_range[0] or col >= col_range[1])
        ):
            return None

        # simple wrapper for a statement that's an expression
        if isinstance(parsed, ast.Expr):
            return self._get_atom_expr(parsed.value, line, col)

        # names and aliases are atomic
        if isinstance(parsed, ast.Name) or isinstance(parsed, ast.alias):
            return parsed

        # constants we consider atomic, for simplicity and for debugging
        if isinstance(parsed, ast.Constant):
            return parsed

        if sys.version_info >= (3, 14):
            if isinstance(parsed, ast.JoinedStr) or isinstance(parsed, ast.TemplateStr):
                return parsed

        # legacy types before consolidation into constant
        if sys.version_info < (3, 8):
            if isinstance(parsed, ast.Num):
                return parsed
            if isinstance(parsed, ast.Str):
                return parsed
            if isinstance(parsed, ast.Bytes):
                return parsed
            if isinstance(parsed, ast.NameConstant):
                return parsed

        # for an attribute lookup, see if it matches in the value part, so `foo.bar` would match `foo`
        # if we're in the first part, otherwise the whole thing
        if isinstance(parsed, ast.Attribute):
            ret = self._get_atom_expr(parsed.value, line, col)
            if ret is not None:
                return ret

            return parsed

        multi_fields = [
            "orelse",
            "finalbody",
            "bases",
            "decorator_list",
            "targets",
            "values",
            "elts",
            "comparators",
            "ifs",
            "defaults",
            "keys",
            "names",
        ]

        # handle listcomps/generators specially so we can return a hacky thing saying
        # which generator to use
        if isinstance(parsed, ast.ListComp) or isinstance(parsed, ast.GeneratorExp):
            for g in parsed.generators:
                ret = self._get_atom_expr(g.target, line, col)
                if ret is not None:
                    return _target_in_gen(ret, g)
                ret = self._get_atom_expr(g.iter, line, col)
                if ret is not None:
                    return ret
                for ifg in g.ifs:
                    ret = self._get_atom_expr(ifg, line, col)
                    if ret is not None:
                        return ret

        if not isinstance(parsed, ast.Lambda):
            multi_fields += ["body"]

        if isinstance(parsed, ast.Call):
            multi_fields += ["args"]

        # in an if expression, body and orelse are expressions not lists of statements
        if isinstance(parsed, ast.IfExp):
            multi_fields.remove("body")
            multi_fields.remove("orelse")

        for multi_field in multi_fields:
            if multi_field in parsed._fields:
                for inner in getattr(parsed, multi_field):
                    ret = self._get_atom_expr(inner, line, col)
                    if ret is not None:
                        return ret

        if "args" not in multi_fields and "args" in parsed._fields:
            args: ast.arguments = getattr(parsed, "args")
            for inner in args.kw_defaults + args.defaults:
                if inner is not None:
                    ret = self._get_atom_expr(inner, line, col)
                    if ret is not None:
                        return ret

            arg_list = args.args + args.kwonlyargs + [args.vararg] + [args.kwarg]
            if "posonlyargs" in args._fields:
                arg_list += args.posonlyargs

            first = True
            for arg in arg_list:
                if arg is not None:
                    if col < _get_colrange(arg)[0] and first:
                        break
                    first = False
                    if arg.annotation is not None:
                        ret = self._get_atom_expr(arg.annotation, line, col)
                        if ret is not None:
                            return ret
                    arg_col_range = _get_colrange(arg)
                    if arg.lineno == line and col <= arg_col_range[1]:
                        return arg

        # for any individual field that's an expr, recurse into it
        for field in parsed._fields:
            val = getattr(parsed, field)
            if isinstance(val, ast.expr):
                ret = self._get_atom_expr(val, line, col)
                if ret is not None:
                    return ret

        if sys.version_info < (3, 9) and "slice" in parsed._fields:
            slice = getattr(parsed, "slice")
            for inner_attr in ["value", "lower", "upper"]:
                if hasattr(slice, inner_attr):
                    ret = self._get_atom_expr(getattr(slice, inner_attr), line, col)
                    if ret is not None:
                        return ret

        # if we match on the first line but didn't match anything else (args, bases)
        # then return the function/class itself
        if isinstance(parsed, ast.ClassDef) or isinstance(parsed, ast.FunctionDef):
            if line == parsed.lineno:
                return parsed

        # if a call contains the target point but we didn't match above (in func or args)
        # then the point is on an in-between character like ( or , in between arguments.
        # find the closest atom before the point.
        if isinstance(parsed, ast.Call):
            # if we're pointing at the closing ) return the call instead. Note that without
            # accurate end-column information this will never match
            if col == col_range[1] - 1:
                return parsed

            # if there are no args or the col is before the first one, return the
            # function itself
            if len(parsed.args) == 0 or col < parsed.args[0].col_offset:
                return parsed.func

            # it's the last arg that starts before the target point
            lastarg = -1
            for i in range(len(parsed.args) - 1):
                if parsed.args[i + 1].col_offset > col:
                    lastarg = i
                    break

            # prefer using the last character of the arg to narrow down as it's more accurate
            arg = parsed.args[lastarg]
            if hasattr(arg, "end_col_offset"):
                return self._get_atom_expr(
                    arg, line, getattr(arg, "end_col_offset") - 1
                )
            return self._get_atom_expr(arg, line, arg.col_offset)

        # if we got here for a subscript then the column points at our closing bracket, not
        # the value we're subscripting or the subscript itself, so we should return the whole
        # expression
        if isinstance(parsed, ast.Subscript):
            return parsed

        return None

    # get the type of whatever element is at a particular location
    def get_location_type(self, line: int, col: int) -> Any:
        if self.module is None:
            raise ValueError("Can't get things with failed parse")
        try:
            expr = self._get_atom_expr(self.module, line, col)
            if expr is not None:
                return self._get_type(self.scopes[line], expr)
        except:
            pass
        return Any

    def get_location_tooltip(self, line: int, col: int) -> str:
        if self.module is None:
            return ""

        try:
            expr = self._get_atom_expr(self.module, line, col)
            if expr is None:
                return ""
        except:
            return ""

        return self._get_tooltip_for_node(expr, line)

    def _get_tooltip_for_node(self, expr: ast.AST, line: int) -> str:
        loctype = self._get_type(self.scopes[line], expr)

        if (
            loctype is not Any
            and (callable(loctype) or isinstance(loctype, UserFunc))
            and not isinstance(loctype, UserClass)
            and not inspect.isclass(loctype)
            and not _is_generic(List, loctype)
            and not _is_generic(Tuple, loctype)
            and not _is_generic(Dict, loctype)
            and not _is_generic(Set, loctype)
            and not _is_generic(Optional, loctype)
            and not _is_generic(Union, loctype)
        ):
            return self._make_func_tooltip(loctype)

        docappend = ""
        ret = ""
        if isinstance(expr, ast.Name):
            ret = f"{expr.id}: "

            if isinstance(loctype, UserClass):
                docappend = _remove_space_prefix(loctype.user_doc, 20)
            elif isinstance(loctype, UserFunc):
                docappend = _remove_space_prefix(loctype.user_doc, 20)
        elif isinstance(expr, ast.Attribute):
            ret = f"{expr.attr}: "

            try:
                parent_type = self._get_type(self.scopes[line], expr.value)
                parent_type = _unwrap_optional(parent_type)
                ret = f"{self.get_name(parent_type)}.{expr.attr}: "

                if isinstance(parent_type, UserClass):
                    member = parent_type.scope.get_ident(expr.attr, line, -1)

                    docappend = _remove_space_prefix(member.user_doc, 20)
                else:
                    docappend = _remove_space_prefix(
                        getattr(getattr(parent_type, expr.attr), "__doc__", ""), 20
                    )
            except:
                pass
        else:
            ret = "expression: "

        if loctype is Any:
            ret += "Unknown Type"
        elif isinstance(loctype, UserClass):
            ret += "class"
        elif isinstance(loctype, UserFunc):
            ret += self.get_name(loctype.callable_type)
        else:
            ret += self.get_name(loctype)

        if docappend != "":
            ret += "\n\n"
            ret += docappend

        return ret.strip()

    def _make_func_tooltip(self, functype: Any, arg_highlight: int = -1):
        ret = ""

        callname = "Callable"
        calldoc = ""
        if isinstance(functype, UserFunc):
            callname = functype.name
            calldoc = functype.user_doc
            functype = functype.callable_type

        if _is_generic(Callable, functype):
            if not hasattr(functype, "__args__"):
                return f"{callname}()"

            args = functype.__args__

            retType = args[-1]
            if retType == type(None):
                retType = None

            ret = f"{callname}(\n"
            indent = " " * 4
            for idx, arg in enumerate(args[0:-1]):
                argtext = f"arg{idx+1}"

                argtext += f": {self.get_name(arg)}"

                if idx == arg_highlight:
                    argtext = f"<b><u>{argtext}</u></b>"

                if idx < len(args[0:-1]) - 1:
                    argtext += ", "
                ret += indent + argtext + "\n"
            ret += ")"
            if retType is not None:
                ret += f" -> {self.get_name(retType)}"
            else:
                ret += " -> None"
            if arg_highlight >= 0:
                ret = ret.replace("\n", "<br>\n")
                ret = ret.replace("  ", "&nbsp;&nbsp;")

            if calldoc != "":
                ret += "\n\n"
                ret += _remove_space_prefix(calldoc, 20)

            return ret

        if isinstance(functype, ast.FunctionDef):
            ret = functype.name + "(\n"
            indent = " " * 4
            for idx, arg in enumerate(functype.args.args):
                argtext = arg.arg

                if arg.annotation is not None:
                    argtext += f": {_expr_to_str(arg.annotation)}"

                if idx == arg_highlight:
                    argtext = f"<b><u>{argtext}</u></b>"

                if (
                    idx < len(functype.args.args) - 1
                    or len(functype.args.posonlyargs) > 0
                    or len(functype.args.kwonlyargs) > 0
                ):
                    argtext += ","
                ret += indent + argtext + "\n"
            if len(functype.args.posonlyargs) > 0:
                ret += indent + "/,\n"
                for idx, arg in enumerate(functype.args.posonlyargs):
                    ret += indent + arg.arg

                    if arg.type_comment is not None:
                        ret += f": {arg.type_comment}"

                    if (
                        idx < len(functype.args.posonlyargs) - 1
                        or len(functype.args.kwonlyargs) > 0
                    ):
                        ret += ","
                    ret += "\n"
            if len(functype.args.kwonlyargs) > 0:
                ret += indent + "*,\n"
                for idx, arg in enumerate(functype.args.kwonlyargs):
                    ret += indent + arg.arg

                    if arg.type_comment is not None:
                        ret += f": {arg.type_comment}"

                    if idx < len(functype.args.kwonlyargs) - 1:
                        ret += ","
            ret += ")"
            if functype.returns is not None:
                ret += f" -> {self.get_name(self._get_type(self.scopes[functype.lineno], functype.returns))}"
            else:
                ret += " -> None"
            if arg_highlight >= 0:
                ret = ret.replace("\n", "<br>\n")
                ret = ret.replace("  ", "&nbsp;&nbsp;")
            # no docs, we're done here
            return ret

        if not callable(functype):
            return ret

        try:
            sig = inspect.signature(functype)
            ret = self.get_name(functype) + "("
            indent = " " * 4
            ret += "\n"
            first = True
            for idx, arg in enumerate(sig.parameters):
                if first and arg == "self":
                    continue
                first = False
                argtext = arg
                annot = sig.parameters[arg].annotation
                if annot is not None and annot != "":
                    argtext += f": {self.get_name(annot)}"

                if idx == arg_highlight:
                    argtext = f"<b><u>{argtext}</u></b>"

                ret += indent + argtext
                if idx < len(sig.parameters) - 1:
                    ret += ","
                ret += "\n"
            ret += ")"
            if sig.return_annotation is not inspect.Signature.empty:
                ret += f" -> {self.get_name(sig.return_annotation)}"
            else:
                ret += " -> None"
        except:
            ret = self.get_name(functype) + "() # unknown signature"
        ret += "\n\n"
        ret += _remove_space_prefix(getattr(functype, "__doc__", ""), 20)
        if arg_highlight >= 0:
            ret = ret.replace("\n", "<br>\n")
            ret = ret.replace("  ", "&nbsp;&nbsp;")
        return ret.strip()

    def get_autocompletion(
        self, line: int, expr: str
    ) -> Tuple[List[str], int, List[str]]:
        expr = _get_trailing_expr(expr).strip()

        if expr == "":
            return [], 0, []

        trailing_dot = expr[-1] == "."
        if trailing_dot:
            expr = expr[:-1]

        # fake the line it's on
        src = "\n" * (line - 1) + expr

        try:
            node = ast.parse(src)
            if not isinstance(node, ast.Module):
                return [], 0, []
            node = node.body[0]
            if not isinstance(node, ast.Expr):
                return [], 0, []
            node = node.value

            curscope = self.scopes[min(len(self.scopes) - 1, line)]

            ret: List[str] = []
            prefix_filter = ""

            def get_member_tooltip(member_name: str):
                if isinstance(node, ast.Attribute):
                    node.attr = member_name
                    return self._get_tooltip_for_node(node, line)
                elif isinstance(node, ast.Name):
                    node.id = member_name
                    return self._get_tooltip_for_node(node, line)
                else:
                    raise RuntimeError("Unexpected node type getting member tooltip")

            # for just a name, filter the identifiers at this point if there was no trailing dot
            if isinstance(node, ast.Name) and not trailing_dot:
                prefix_filter = node.id
                while curscope is not None:
                    for k, v in curscope.identifiers.items():
                        if k.startswith('_renderdoc'):
                            continue
                        if any([x.line <= line for x in v]):
                            ret.append(k)
                    curscope = curscope.parent
            else:
                # we provide completion for attribute access, which can either look like just a
                # name (if there was a trailing dot so we didn't hit the case above)
                base_type = Any
                prefix_filter = ""

                if isinstance(node, ast.Name) or trailing_dot:
                    base_type = self._get_type(curscope, node)

                    node = ast.Attribute(node, "", lineno=node.lineno)
                elif isinstance(node, ast.Attribute):
                    base_type = self._get_type(curscope, node.value)
                    prefix_filter = node.attr
                else:
                    return [], 0, []

                # if the base type is unknown in some fashion, nothing to do
                if base_type is Any or base_type is None:
                    return [], 0, []

                base_type = _unwrap_optional(base_type)

                # if this is a user type, look up its identifiers from our list
                if isinstance(base_type, UserClass):
                    base_ident = base_type.ident
                    if base_ident is not None:
                        base_scope = self.scopes[base_ident.line]
                        ret = list(base_scope.identifiers.keys())
                    else:
                        return [], 0, []
                else:
                    # pre-python 3.8 Dict, List etc are not the real types, substitute here
                    if sys.version_info < (3, 9):
                        if _is_generic(Dict, base_type):
                            base_type = dict
                        if _is_generic(List, base_type):
                            base_type = list
                        if _is_generic(Tuple, base_type):
                            base_type = tuple
                        if _is_generic(Set, base_type):
                            base_type = set

                    # otherwise filter dir()
                    ret = dir(base_type)

                    # remove any typing types that might be in dir() of module stubs for type hints
                    import typing

                    if inspect.ismodule(base_type):
                        ret = [
                            x
                            for x in ret
                            if not hasattr(typing, x)
                            or (getattr(typing, x) != getattr(base_type, x))
                        ]

            # apply the prefix filter
            ret = list(
                filter(lambda x: x.upper().startswith(prefix_filter.upper()), ret)
            )

            # unless we already started typing _, remove __ items
            if not prefix_filter.startswith("_"):
                ret = list(filter(lambda x: not x.startswith("__"), ret))

            # sort alphabetically, case-insensitively
            ret = sorted(ret, key=lambda x: x.upper())

            return ret, len(prefix_filter), [get_member_tooltip(x) for x in ret]

        except:
            pass
        return [], 0, []

    def get_funccompletion(self, line: int, expr: str) -> Tuple[str, str, str]:
        func, argidx = _get_func_arg(_get_trailing_expr(expr + ")"))

        try:
            # fake the line it's on
            src = "\n" * (line - 1) + func

            node = ast.parse(src)
            if not isinstance(node, ast.Module) or len(node.body) == 0:
                return "", "", ""
            node = node.body[0]

            curscope = self.scopes[min(len(self.scopes) - 1, line)]

            func_type = self._get_type(curscope, node)

            if func_type == Any:
                return "", "", ""

            if (
                _is_generic(Callable, func_type) or isinstance(func_type, UserFunc)
            ) and self._last_ident is not None:
                func_scope = self.scopes[self._last_ident.line]
                # skip invisible self, when looking at methods
                if func_scope.parent is not None and func_scope.parent.is_class:
                    argidx += 1
                if isinstance(func_scope.parsed, ast.FunctionDef):
                    func_node = func_scope.parsed
                    if argidx < len(func_node.args.args):
                        return (
                            func,
                            func_node.args.args[argidx].arg,
                            self._make_func_tooltip(func_node, argidx),
                        )
                    else:
                        return (
                            func,
                            "",
                            self._make_func_tooltip(func_node, -1),
                        )
                return (
                    func,
                    f"arg{argidx+1}",
                    self._make_func_tooltip(func_type, argidx),
                )

            if isinstance(func_type, UserFunc):
                sig = inspect.signature(func_type.callable_type)
            else:
                sig = inspect.signature(func_type)

            params = list(sig.parameters)
            # skip invisible self, when looking at methods
            if params[0] == "self":
                argidx += 1
            if argidx < len(params):
                return func, params[argidx], self._make_func_tooltip(func_type, argidx)
            return func, "", self._make_func_tooltip(func_type)
        except:
            pass

        return "", "", ""

    # try to get a friendly name for a type based on its parent class and module
    def get_name(self, obj: Any) -> str:
        if isinstance(obj, str):
            return obj

        if isinstance(obj, UserFunc) or isinstance(obj, UserClass):
            return obj.name

        name = ""

        generics = [
            (Tuple, "Tuple"),
            (List, "List"),
            (Dict, "Dict"),
            (Set, "Set"),
        ]
        for g, n in generics:
            if _is_generic(g, obj):
                if not hasattr(obj, "__args__"):
                    return n
                args = ", ".join([self.get_name(a) for a in obj.__args__])
                return f"{n}[{args}]"

        if _is_generic(Callable, obj):
            ret_type = self.get_name(obj.__args__[-1])
            args = ", ".join([self.get_name(a) for a in obj.__args__[:-1]])
            return f"Callable[[{args}], {ret_type}]"

        # identify Optional[] looking like Union[x, None]
        if obj != _unwrap_optional(obj):
            return f"Optional[{_unwrap_optional(obj)}]"

        if hasattr(obj, "__objclass__"):
            cl = obj.__objclass__

            if hasattr(cl, "__name__"):
                name = f"{cl.__name__}."

            membernames = [x for x in dir(cl) if getattr(cl, x) == obj]

            if len(membernames) != 1:
                name = ""
            else:
                name += membernames[0]

                if hasattr(cl, "__module__") and cl.__module__ != "builtins":
                    modname = getattr(cl, "__module__")

                    for k in PyReflector.alias_modules.keys():
                        if PyReflector.alias_modules[k] == sys.modules[modname]:
                            modname = k
                            break

                    name = f"{modname}.{name}"

        if name == "" and hasattr(obj, "__module__"):
            mod = obj.__module__

            if mod in sys.modules:
                is_typing = mod == "typing"

                mod = sys.modules[mod]

                qualname = getattr(obj, "__qualname__", "")

                if _lookup_attrpath(mod, qualname) != obj:
                    names = dir(mod)
                    if is_typing:
                        names = filter(lambda x: x[0] != "_", names)
                    membernames = [x for x in names if getattr(mod, x) == obj]

                    if len(membernames) != 1:
                        qualname = ""
                    else:
                        qualname = membernames[0]

                if qualname != "":
                    if obj.__module__ == "builtins":
                        return qualname.split(".")[-1]

                    modname = obj.__module__

                    for k in PyReflector.alias_modules.keys():
                        package = getattr(sys.modules[modname], "__package__", "")
                        if PyReflector.alias_modules[k] == sys.modules[modname] or (
                            package != ""
                            and PyReflector.alias_modules[k] == sys.modules[package]
                        ):
                            modname = k
                            break

                    name = f"{modname}.{qualname}"

        if (
            name == ""
            and hasattr(obj, "__qualname__")
            and (not hasattr(obj, "__module__") or obj.__module__ != "typing")
        ):
            return getattr(obj, "__qualname__")

        if name == "" and hasattr(obj, "__name__"):
            name = getattr(obj, "__name__").split(".")[-1]

            for k in PyReflector.alias_modules.keys():
                if PyReflector.alias_modules[k] == obj:
                    return k

        if name == "" and hasattr(obj, "__forward_arg__"):
            name = getattr(obj, "__forward_arg__")

        if name == "":
            name = str(obj)

        return name

    # print all scopes and identifiers with their types
    def dump(self):
        seen = set()
        for s in self.scopes:
            if s in seen:
                continue
            seen.add(s)
            nest = 0
            p = s.parent
            while p is not None:
                nest += 1
                p = p.parent
            indent = "  " * nest
            print(f"{'==' * (nest+1)} {s.name}")
            accum = []
            for name in s.identifiers:
                for inst in s.identifiers[name]:
                    accum += [{"line": inst.line, "name": name, "type": inst.type_obj}]
            accum.sort(key=lambda x: x["line"])
            for a in accum:
                t = a["type"]
                # older pythons don't have a good str() for new types
                if "NewType" in str(t):
                    t = t.__name__
                print(f"{indent}Line {a['line']}: {a['name']} is {t}")


# self-testing by parsing this file (or any file on the command line)
# TEST BEGIN
import sys, re
from typing import List, Dict, Any, Tuple, Callable, Optional, cast

error_code = """
def func_with_error(self):
    print("hi")
    self.value = self.
"""

# these tests fail too much without column information that requires at least 3.8
if __name__ == "__main__" and sys.version_info >= (3, 8):
    file = __file__
    if len(sys.argv) >= 2:
        file = sys.argv[1]
    else:
        file = __file__

    with open(file) as f:
        text = f.read().expandtabs(4)
        text += error_code

        # trim to the start of the test, but preserve line numbers
        offs = text.index("# TEST BEGIN")
        start_line = text.count("\n", 0, offs)
        text = ("\n" * start_line) + text[offs:]

        # empty globals, pretend this is pristine - it will handle builtins internally
        refl = PyReflector(text, {}, False)

        if not refl.valid():
            raise RuntimeError(f"Failed to parse {file}")

        # put user types into globals for easier matching
        globals().update(refl.user_types)

        # import some things we want to check against but not be in globals
        import random
        import base64

        passed = 0

        # for auto complete
        entry = ""
        calltype = ""
        param = ""
        expected_prefix_len = 0
        expected_results = []
        expected_missing = []

        # find automatic test prompts
        lines = refl.parsed_text.splitlines()
        for i, line_text in enumerate(lines):
            if "# NAME:" in line_text and not "#exclude" in line_text:
                col = line_text.index("^")

                # allow multiple checks on the same line
                while lines[i - 1].lstrip()[0] == "#":
                    i -= 1

                # this naturally targets the previous line due to 1-based and 0-based
                line = i

                actual = refl.get_location_type(line, col)

                actual = refl.get_name(actual)

                expect = line_text[line_text.index("NAME: ") + 6 :]

                if actual == expect:
                    passed += 1  # name match
                else:
                    raise RuntimeError(
                        f"{file}:{line}:{col+1} expected name '{actual}' to match '{expect}'\n"
                        + refl.get_line_source(line)
                        + "\n"
                        + (" " * col)
                        + "^"
                    )

            # quick check to make sure we don't match this if here
            if "# TYPE:" in line_text and not "#exclude" in line_text:
                col = line_text.index("^")

                # allow multiple checks on the same line
                while lines[i - 1].lstrip()[0] == "#":
                    i -= 1

                # this naturally targets the previous line due to 1-based and 0-based
                line = i

                actual = refl.get_location_type(line, col)

                expect_text = line_text[line_text.index("TYPE: ") + 6 :]
                expect = eval(expect_text)
                if (
                    actual == expect
                    or (
                        isinstance(expect, UserClass)
                        and isinstance(actual, UserClass)
                        and expect.name == actual.name
                    )
                    or (
                        isinstance(expect, UserFunc)
                        and isinstance(actual, UserFunc)
                        and expect.name == actual.name
                        and expect.callable_type == actual.callable_type
                    )
                ):
                    passed += 1  # types match
                else:
                    raise RuntimeError(
                        f"{file}:{line}:{col+1} expected type '{actual}' to match '{expect_text}'\n"
                        + refl.get_line_source(line)
                        + "\n"
                        + (" " * col)
                        + "^"
                    )

            if "# ENTRY" in line_text and not "#exclude" in line_text:
                entry = line_text[line_text.index("ENTRY: ") + 7 :]

            # for autocomplete, results we expect and must not see
            if "# RESULT" in line_text and not "#exclude" in line_text:
                text = line_text[line_text.index("RESULT: ") + 8 :]
                if "#" in text:
                    result = text.split("#")
                else:
                    result = (text, "")
                expected_results.append(result)
            if "# MISSING" in line_text and not "#exclude" in line_text:
                expected_missing.append(line_text[line_text.index("MISSING: ") + 9 :])
            if "# PREFIX" in line_text and not "#exclude" in line_text:
                expected_prefix_len = int(line_text[line_text.index("PREFIX: ") + 8 :])

            # for function completion the type of the callable, and the current parameter
            if "# CALLTYPE" in line_text and not "#exclude" in line_text:
                calltype = line_text[line_text.index("CALLTYPE: ") + 10 :]
            if "# PARAM" in line_text and not "#exclude" in line_text:
                param = line_text[line_text.index("PARAM: ") + 7 :]

            if "# AUTOCOMPLETE TEST" in line_text and not "#exclude" in line_text:
                line = i + 1

                completions, prefix_len, tooltips = refl.get_autocompletion(line, entry)

                if expected_prefix_len != prefix_len:
                    raise RuntimeError(
                        f"{file}:{line} expected prefix length of '{expected_prefix_len}' for '{entry}', got '{prefix_len}'"
                    )

                for res, tooltip in expected_results:
                    if res not in completions:
                        raise RuntimeError(
                            f"{file}:{line} expected entry '{res}' in autocompletion for '{entry}'"
                        )
                    idx = completions.index(res)
                    if tooltip not in tooltips[idx]:
                        raise RuntimeError(
                            f"{file}:{line} expected entry '{res}' tooltip '{tooltips[idx]}' "
                            + f"to contain {tooltip} in autocompletion for '{entry}'"
                        )
                    passed += 1

                for m in expected_missing:
                    if m in completions:
                        raise RuntimeError(
                            f"{file}:{line} unexpected entry '{m}' in autocompletion for '{entry}'"
                        )
                    passed += 1

                # reset lists
                expected_results = []
                expected_missing = []

            if "# FUNCCOMPLETE TEST" in line_text and not "#exclude" in line_text:
                line = i

                actual_calltype, actual_param, _ = refl.get_funccompletion(line, entry)

                if actual_calltype != calltype:
                    raise RuntimeError(
                        f"{file}:{line} expected function '{calltype}' in function completion for '{entry}', got '{actual_calltype}'"
                    )
                else:
                    passed += 1

                if actual_param != param:
                    raise RuntimeError(
                        f"{file}:{line} expected parameter '{param}' in function completion for '{entry}', got '{actual_param}'"
                    )
                else:
                    passed += 1

                pass

        # some manual tests
        nop_tests = [
            # edge case
            ("", ""),
            # no translation, even with brackets or \\ chars
            ("simple", "simple"),
            ("some_expression([foo: {}])", "some_expression([foo: {}])"),
            ("expression with \\ somehow", "expression with \\ somehow"),
            # plain strings
            ("expr('blah')", "expr('xxxx')"),
            ('expr("blah")', 'expr("xxxx")'),
            # strings with alternate quotes
            ("expr('bl \"a \"h')", "expr('xxxxxxxx')"),
            ("expr(\"bl 'a 'h\")", 'expr("xxxxxxxx")'),
            # string with escaped quotes
            ("expr('blah\\', foo')", "expr('xxxxxxxxxxx')"),
            ('expr("blah\\", foo")', 'expr("xxxxxxxxxxx")'),
            # strings of both types
            (
                'expr("blah", \'foo\', "bar \' blah")',
                'expr("xxxx", \'xxx\', "xxxxxxxxxx")',
            ),
            # escape characters
            (
                'expr("blah\\123 \\05 \\h5F \\h5f \\u63fb44 \\U008270fF \\N{SNAKE}")',
                'expr("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx")',
            ),
        ]

        for inval, expected in nop_tests:
            actual = _nopstrings(inval)

            if actual == expected:
                passed += 1
            else:
                raise RuntimeError(
                    f"expected {inval} to nop out to {expected} but got {actual}"
                )

        expr_tests = [
            ("ret = hello(a, b) + world(c, d)", "world(c, d)"),
            # simple direct and valid expressions
            ("hello", "hello"),
            ("foo.bar", "foo.bar"),
            ("foo[0]", "foo[0]"),
            ("foo[bar.foo]", "foo[bar.foo]"),
            ("foo()", "foo()"),
            ("foo(bar)", "foo(bar)"),
            ("foo(bar, qux)", "foo(bar, qux)"),
            ("(bar, qux)", "(bar, qux)"),
            ("[bar, qux]", "[bar, qux]"),
            ("[thing.other for thing in list]", "[thing.other for thing in list]"),
            # strings will be nop'd
            ('{"bar": 2, "qux": 3}', '{"xxx": 2, "xxx": 3}'),
            # complex chained but valid expression
            (
                "foo.bar[other.index].member(param1, param2=blah).data",
                "foo.bar[other.index].member(param1, param2=blah).data",
            ),
            # function calls with nested parameters
            (
                "foo(tuple_param = (1,2), list_param = [1,2], dict_param = {1: 4, 8: 2})",
                "foo(tuple_param = (1,2), list_param = [1,2], dict_param = {1: 4, 8: 2})",
            ),
            (
                "foo(other_func(1,2), obj.method([1,2], blah))",
                "foo(other_func(1,2), obj.method([1,2], blah))",
            ),
            # grabbing subexpression in a larger expression (still valid)
            ("ret = hello", "hello"),
            ("ret = hello + world", "world"),
            ("ret = hello > world", "world"),
            ("ret = hello(a, b) + world(c, d)", "world(c, d)"),
            (
                "ret = [thing.other for thing in list]",
                "[thing.other for thing in list]",
            ),
            ("x, y = func1(a, b), func2(c, d)", "func2(c, d)"),
            ("if condition: thing_doer.do(x, y)", "thing_doer.do(x, y)"),
            ("statement1(call); statement2(call)", "statement2(call)"),
            # simple expressions but with trailing . that's invalid
            ("hello.", "hello."),
            ("foo.bar.", "foo.bar."),
            ("foo[0].", "foo[0]."),
            ("foo[bar.foo].", "foo[bar.foo]."),
            ("foo(bar, qux).other.", "foo(bar, qux).other."),
            # open subscript
            ("foo[0", "0"),
            ("foo[bar.other", "bar.other"),
            ("foo[bar.other + blah", "blah"),
            # open list comp
            ("blah = [other.foo for other in list", "list"),
            # partial function calls - these are handled separately for parameter completion
            ("func(param, param2", "param2"),
            ("func(param", "param"),
            ("func(", ""),
            # invalid function calls or calls containing problems in their arguments
            # this is useful so if we are in a function call we can 'cap' it with a )
            # without caring, and count parameters
            ("func(param, param2, )", "func(param, param2, )"),
            ("func(param, thing. )", "func(param, thing. )"),
            # testing behaviour with an extra )
            ("foobar)", "foobar)"),
            ("func_call())", "func_call())"),
            ("func_call().member)", "func_call().member)"),
            # complex nesting of function calls
            ("func1(param, func2(other, param3, kw_param=func3(blah", "blah"),
            ("func1(param, func2(other, param3, kw_param=func3(blah)", "func3(blah)"),
            ("func1(param, func2(other, param3, func3(blah)", "func3(blah)"),
            (
                "func1(param, func2(other, param3, thing.func3(blah)",
                "thing.func3(blah)",
            ),
        ]

        for inval, expected in expr_tests:
            actual = _get_trailing_expr(inval)

            if actual == expected:
                passed += 1
            else:
                raise RuntimeError(
                    f"expected trailing expr of '{inval}' to be '{expected}' but got '{actual}'"
                )

        func_tests = [
            # direct cases
            ("func()", ("func", 0)),
            ("func()", ("func", 0)),
            ("func(param)", ("func", 0)),
            ("func(param,)", ("func", 1)),
            ("func(param, p)", ("func", 1)),
            # cases with complex arguments
            ("func(param, (1, 2))", ("func", 1)),
            ("func(param, [1, 2])", ("func", 1)),
            ("func(param, func2())", ("func", 1)),
            ("func(param, func2(), c)", ("func", 2)),
            ("func(param, func2([1,2],(3,4)), c)", ("func", 2)),
            # failure cases
            ("not_a_call", ("", -1)),
            ("[1, 2, 3, func()]", ("", -1)),
            ("(tuple,with,params)", ("", -1)),
            ("call(a, b).with.trail", ("", -1)),
        ]

        for inval, expected in func_tests:
            actual = _get_func_arg(inval)

            if actual == expected:
                passed += 1
            else:
                raise RuntimeError(
                    f"For '{inval}' expected argument {expected[1]} in '{expected[0]}' but got {actual[1]} in '{actual[0]}'"
                )

        print(f"{passed} tests passed!")

# parsing tests are below here
if __name__ == "impossible":
    import math

    ## simple direct types

    simple_int = 5
    #  ^    # TYPE: int

    other_thing = simple_int
    #  ^                   # TYPE: int
    #                 ^    # TYPE: int

    list_item = ["a"]
    #  ^    # TYPE: List[str]

    list_item = [1, 2, 3]
    #  ^    # TYPE: List[int]

    tuple_item = (1, 2, 3)
    #  ^    # TYPE: Tuple[int,int,int]

    diff_tuple_item = (1, "asdf", 4.4)
    #  ^    # TYPE: Tuple[int,str,float]

    dict_item = {"asdf": 5, "foo": 3}
    #  ^    # TYPE: Dict[str, int]

    dict_item = {"asdf": 5, "foo": 3.4}
    #  ^    # TYPE: Dict[str, Any]

    dict_item = {"asdf": 5, 3: 4}
    #  ^    # TYPE: Dict[Any, int]

    dict_item = {"asdf": 5, 3: "foo"}
    #  ^    # TYPE: Dict[Any, Any]

    ## assignments

    # check that we get the right type even when an identifier changes types multiple times
    override = 5
    #  ^    # TYPE: int

    override = 4.4
    #  ^    # TYPE: float

    override = "abc"
    #  ^    # TYPE: str

    # multi-assignment
    aaa = bbb = 5
    # ^              # TYPE: int
    #      ^         # TYPE: int

    # unpacking
    aaa, bbb = (1, "str")
    # ^              # TYPE: int
    #    ^           # TYPE: str

    ## type annotations

    annot_item: List[int] = []
    #  ^    # TYPE: List[int]

    annot_item2: Tuple[int, str]
    #  ^       # TYPE: Tuple[int, str]

    annot_item3: Optional[str] = None
    #  ^       # TYPE: Optional[str]

    annot_item4: Dict[int, str] = {}
    #  ^       # TYPE: Dict[int, str]

    annot_item5: Callable[[int, float], str]
    #  ^       # TYPE: Callable[[int, float], str]

    # annotations are trusted completely
    annot_wrong_item: List[int] = 0  # type: ignore
    #  ^    # TYPE: List[int]

    ## operations

    a, b = 2, 3

    ccc = a * b
    # ^    # TYPE: int

    ccc = -a
    # ^    # TYPE: int

    ccc = a < b
    # ^    # TYPE: bool

    ccc = (a > 0) or (b > 0)
    # ^    # TYPE: bool

    ## loops
    for counter in range(5):
        # ^    # TYPE: int
        print(counter)

    some_list = ["a", "b", "c"]

    for counter, value in enumerate(some_list):
        #  ^                  # TYPE: int
        #          ^          # TYPE: str
        print(f"{counter} - {value}")

    for value in some_list:
        # ^          # TYPE: str
        print(value)

    ## list comprehensions

    list_comp = [x * 2 for x in range(10)]
    #   ^    # TYPE: List[int]

    list_comp = ["foo" * x for x in range(10)]
    #   ^    # TYPE: List[str]

    list_comp = [len(x) for x in list_comp if len(x) > 4]
    #   ^    # TYPE: List[int]

    tuple_list = [(1, "a"), (2, "b"), (3, "c")]
    #   ^    # TYPE: List[Tuple[int,str]]

    unpack_comp = [x for x, y in tuple_list]
    #   ^                          # TYPE: List[int]
    #                    ^         # TYPE: int
    #                       ^      # TYPE: str

    unpack_comp = [y for x, y in tuple_list]
    #   ^    # TYPE: List[str]

    ## Iterators we treat as lists for simplicity

    iter_comp = (x * 2 for x in range(10))
    #   ^            # TYPE: List[int]

    iter_comp = next(x * 2 for x in range(10))
    #   ^            # TYPE: int

    ## subscript/attribute accesses

    val = list_item[0]
    # ^                 # TYPE: int
    #      ^            # TYPE: List[int]
    #               ^   # TYPE: int
    #                ^  # TYPE: int

    val = list_item[0:2]
    # ^                   # TYPE: List[int]
    #      ^              # TYPE: List[int]
    #               ^     # TYPE: int
    #                 ^   # TYPE: int
    #                  ^  # TYPE: List[int]

    val = tuple_item[0]
    # ^                  # TYPE: int
    #       ^            # TYPE: Tuple[int, int, int]
    #                ^   # TYPE: int
    #                 ^  # TYPE: int

    val = diff_tuple_item[0]
    # ^                  # TYPE: int

    val = diff_tuple_item[1]
    # ^                  # TYPE: str

    val = diff_tuple_item[2]
    # ^                  # TYPE: float

    # tuples we don't try to evaluate the subscript
    val = tuple_item[0:2]
    # ^                    # TYPE: Tuple[int, int, int]
    #       ^              # TYPE: Tuple[int, int, int]
    #                ^     # TYPE: int
    #                  ^   # TYPE: int
    #                   ^  # TYPE: Tuple[int, int, int]

    ## casts (either explicitly with typing.cast or implicit between
    ##        lists and tuples)

    val = list(list_item)
    # ^                   # TYPE: List[int]

    val = cast(str, list_item)
    # ^                   # TYPE: str

    class Inner:
        foo: int
        bar: str

    class Outer:
        inner: Inner
        val: float

    # this will force Inner to be procesed early
    inner_assign = Inner()
    #  ^    # TYPE: Inner
    inner_assign = inner_assign.foo
    #  ^    # TYPE: int

    inner_assign = Inner()
    inner_assign = inner_assign.bar
    #  ^                               # TYPE: str
    #                   ^              # TYPE: Inner
    #                            ^     # TYPE: str

    outer_list: List[Outer] = []

    val = outer_list
    # ^    # TYPE: List[Outer]

    val = outer_list[0]
    # ^    # TYPE: Outer

    val = outer_list[0].inner
    # ^    # TYPE: Inner

    val = outer_list[0].inner.bar
    # ^                             # TYPE: str
    #         ^                     # TYPE: List[Outer]
    #                ^              # TYPE: int
    #                 ^             # TYPE: Outer
    #                     ^         # TYPE: Inner
    #                          ^    # TYPE: str

    ## functions and calls

    def annot_function(arg1: str, arg2: int) -> bool:
        #            ^    # TYPE: UserFunc("annot_function", Callable[[str, int], bool])
        return len(arg1) < arg2

    val = annot_function("foobar", 4)
    # ^    # TYPE: bool

    # function returns can be guessed if all returns are the same type
    def guess_function(arg1, arg2):
        #            ^    # TYPE: UserFunc("guess_function", Callable[[Any, Any], float])
        if len(arg1) < arg2:
            return 4.4
        return 5.5

    val = guess_function("foobar", 4)
    # ^    # TYPE: float

    outer_scope_val = 5

    def function():
        global outer_scope_val

        ret = 0
        ret += outer_scope_val
        #            ^    # TYPE: int

        outer_scope_val = "blah"

        ret += len(outer_scope_val)
        #            ^    # TYPE: str

        return ret

    a = 5
    b = 6.6
    c = Inner()
    d = "foo"
    e = [5.5]
    f = (5, 5)

    def func1(a, b) -> float: ...

    def func2(c, d) -> float: ...

    complex = (func1(a, b) + func2(c, d)) * math.sqrt(len([g * f[0] for g in e]))
    #   ^                                                                         # TYPE: float
    #                          ^                                                  # TYPE: UserFunc("func2", Callable[[Any, Any], float])
    #                              ^                                              # TYPE: Inner
    #                                                                        ^    # TYPE: List[float]

    def func3(a, b, c, d, e, f) -> bool: ...

    bbb = b

    func3(a, bbb, c, d, e[0], f[1])
    #   ^                                 # TYPE: UserFunc("func3", Callable[[Any] * 6, bool])
    #    ^                                # TYPE: UserFunc("func3", Callable[[Any] * 6, bool])
    #     ^                               # TYPE: int
    #      ^                              # TYPE: int
    #       ^                             # TYPE: int
    #        ^                            # TYPE: float
    #                ^                    # TYPE: str
    #                 ^                   # TYPE: str
    #                  ^                  # TYPE: str
    #                   ^                 # TYPE: List[float]
    #                     ^               # TYPE: int
    #                      ^              # TYPE: float
    #                       ^             # TYPE: float
    #                        ^            # TYPE: float
    #                             ^       # TYPE: bool

    def func4() -> int: ...

    test_val = 123
    true_val = "true"
    false_val = "false"

    ternary = true_val if test_val > func4() else false_val
    #   ^                                                   # TYPE: str
    #          ^                                            # TYPE: str
    #                       ^                               # TYPE: int
    #                                      ^                # TYPE: int
    #                                               ^       # TYPE: str

    def annot_func1(param1, param2, param3=5, *, param4, param5="hello") -> int:
        #                ^                                                        # TYPE: Any
        #                               ^                                         # TYPE: int
        #                                           ^                             # TYPE: Any
        #                                                    ^                    # TYPE: str
        #                                                           ^             # TYPE: str
        ...

    ## imports

    from random import randint

    a = randint(2, 3)
    #       ^  # TYPE: random.randint

    from random import choice as pickyourpoison

    a = pickyourpoison([1, 2, 3])
    #       ^  # TYPE: random.choice
    #       ^  # NAME: random.choice

    import base64 as base32times2

    a = base32times2.b64encode(b"hello")
    #                    ^  # TYPE: base64.b64encode
    #                    ^  # NAME: base64.b64encode

    ## class methods and properties

    class ContainerClass(Outer):
        #                   ^  # TYPE: Outer
        def __init__(self):
            self.counter = 0

            self.complex = Inner()

            self.value = self.make_value()
            #       ^  # TYPE: float

            self.other = self.make_other(self.value)
            #       ^                                 # TYPE: float
            #                                   ^     # TYPE: float

        def make_value(self) -> float:
            #      ^  # TYPE: UserFunc("make_value", Callable[[Any], float])
            ...

        def make_other(self, val: float):
            #      ^  # TYPE: UserFunc("make_other", Callable[[Any, float], float])
            if val > 0.0:
                return 1.0
            if val < 0.0:
                return -1.0
            return 0.0

        @property
        def prop(self) -> Dict[str, int]:
            return {}

        @prop.setter
        def prop(self, val): ...

        @property
        def prop2(self):
            return "blah"

        @prop2.setter
        def prop2(self, val): ...

        def do_thing(self):
            self.other = -self.other
            #       ^               # TYPE: float
            #                    ^  # TYPE: float

            if self.complex.bar == "hello":
                # ^                           # TYPE: ContainerClass
                #       ^                     # TYPE: Inner
                #            ^               # TYPE: str
                return 1.234

            lookup = self.prop
            #  ^  # TYPE: Dict[str, int]
            key = self.prop2
            # ^  # TYPE: str

            """
            random string that is not a docstring and should cause
            no problems
            """

            if key in lookup:
                return 4.321

            return self.value * self.other

    ## test names
    # mostly not useful here with only builtins to check
    # as this does not handle user-defined types

    list_item = []
    list_item.index(5)
    #           ^  # NAME: list.index

    dict_item = {}
    dict_item.update({})
    #           ^  # NAME: dict.update

    # ENTRY: foo(
    # PREFIX: 0
    # AUTOCOMPLETE TEST

    # ENTRY: dict_item.
    # RESULT: update
    # RESULT: clear
    # RESULT: keys
    # PREFIX: 0
    # AUTOCOMPLETE TEST

    # ENTRY: if dict_item.
    # RESULT: update
    # RESULT: clear
    # RESULT: keys
    # PREFIX: 0
    # AUTOCOMPLETE TEST

    # ENTRY: for dict_item.
    # RESULT: update
    # RESULT: clear
    # RESULT: keys
    # PREFIX: 0
    # AUTOCOMPLETE TEST

    # ENTRY: dict_item.po
    # RESULT: pop
    # RESULT: popitem
    # MISSING: keys
    # PREFIX: 2
    # AUTOCOMPLETE TEST

    autocomplete_var1 = 5
    autocomplete_var2 = "hi"
    autonotcomplete_var3 = (1, 1)

    # ENTRY: auto
    # RESULT: autocomplete_var1
    # RESULT: autocomplete_var2
    # RESULT: autonotcomplete_var3
    # PREFIX: 4
    # AUTOCOMPLETE TEST

    # ENTRY: autocomp
    # RESULT: autocomplete_var1
    # RESULT: autocomplete_var2
    # MISSING: autonotcomplete_var3
    # PREFIX: 8
    # AUTOCOMPLETE TEST

    # ENTRY: outer_list[0].
    # RESULT: inner
    # RESULT: val
    # PREFIX: 0
    # AUTOCOMPLETE TEST

    # ENTRY: outer_list[0].inner.
    # RESULT: foo
    # RESULT: bar
    # PREFIX: 0
    # AUTOCOMPLETE TEST

    # ENTRY: outer_list[0].inner.f
    # RESULT: foo
    # MISSING: bar
    # PREFIX: 1
    # AUTOCOMPLETE TEST
    def auto_function(param1, foobar, blah) -> bool: ...

    class FooClass:
        """
        docs of class
        """

        def method(self, param1: int, foobar: float, blah: str) -> bool:
            """
            docs of method
            """
            ...

        member: int = 5
        """
        docs of member
        """

    foo = FooClass()

    optfoo: Optional[FooClass]

    # ENTRY: randint(
    # CALLTYPE: randint
    # PARAM: a
    # FUNCCOMPLETE TEST

    # ENTRY: blah(randint(
    # CALLTYPE: randint
    # PARAM: a
    # FUNCCOMPLETE TEST

    # ENTRY: randint(0,
    # CALLTYPE: randint
    # PARAM: b
    # FUNCCOMPLETE TEST

    # ENTRY: auto_function(
    # CALLTYPE: auto_function
    # PARAM: param1
    # FUNCCOMPLETE TEST

    # ENTRY: auto_function(,
    # CALLTYPE: auto_function
    # PARAM: foobar
    # FUNCCOMPLETE TEST

    # ENTRY: auto_function(blah.asd
    # CALLTYPE: auto_function
    # PARAM: param1
    # FUNCCOMPLETE TEST

    # ENTRY: auto_function(func()
    # CALLTYPE: auto_function
    # PARAM: param1
    # FUNCCOMPLETE TEST

    # ENTRY: auto_function(func(),
    # CALLTYPE: auto_function
    # PARAM: foobar
    # FUNCCOMPLETE TEST

    # ENTRY: foo.method(func(),
    # CALLTYPE: foo.method
    # PARAM: foobar
    # FUNCCOMPLETE TEST

    # ENTRY: foo.  method(func(),
    # CALLTYPE: foo.  method
    # PARAM: foobar
    # FUNCCOMPLETE TEST

    # ENTRY: foo.method(func(), randint(
    # CALLTYPE: randint
    # PARAM: a
    # FUNCCOMPLETE TEST

    # ENTRY: randint(foo.method(func(),
    # CALLTYPE: foo.method
    # PARAM: foobar
    # FUNCCOMPLETE TEST

    # ENTRY: FooCl
    # RESULT: FooClass#docs of class
    # PREFIX: 5
    # AUTOCOMPLETE TEST

    # ENTRY: foo.
    # RESULT: member#docs of member
    # RESULT: method#docs of method
    # RESULT: method#arg1: int
    # RESULT: method#arg2: float
    # RESULT: method#arg3: str
    # RESULT: method#-> bool
    # PREFIX: 0
    # AUTOCOMPLETE TEST

    # ENTRY: optfoo.
    # RESULT: member#docs of member
    # RESULT: method#docs of method
    # RESULT: method#arg1: int
    # RESULT: method#arg2: float
    # RESULT: method#arg3: str
    # RESULT: method#-> bool
    # PREFIX: 0
    # AUTOCOMPLETE TEST
