import enum, time, inspect
import io, os, re, shutil, sys
import builtins, keyword, types, typing
from typing import List, Set, Dict, Tuple, Any, Optional, Callable, Type, TypeVar

# precompiled regexs for parsing restructuredtext documentation
RTYPE_PATTERN = re.compile(r":rtype:\s*(.*)")
TYPE_PATTERN = re.compile(r":type:\s*(.*)")
DATA_PATTERN = re.compile(r".. data::\s*(.*)")
FUNC_PATTERN = re.compile(r".. function::\s*([^\(]*).*")
ANY_PATTERN = re.compile(r"^.. [a-z]", re.MULTILINE)
PARAM_PATTERN = re.compile(r":param\s+([^:=]*)\s+([^: =]+)(\s*=[^:]*)?:")

# on msys, use crlf output
nl = None
if sys.platform == "msys":
    nl = "\r\n"


# RenderDoc/swig specific filtering
def shouldskip(name) -> bool:
    if name.startswith("Swig") or name.startswith("SWIG"):
        return True

    if name.startswith("rdcarray_"):
        return True

    if name in ["this", "thisown", "acquire", "append", "disown", "next", "own"]:
        return True

    return False


# helper class for writing lines to a file or string buffer
class Stream:
    def __init__(self, f: Optional[io.TextIOWrapper]):
        self.file = f
        self.buffer = ""
        self.indent_level = 0

    file: Optional[io.TextIOWrapper]
    buffer: str
    indent_level: int

    def println(self, line: str):
        self._write("    " * self.indent_level)
        self._write(line)
        self._write("\n")

    def _write(self, s: str):
        if self.file is not None:
            self.file.write(s)
        else:
            self.buffer += s

    def printlines(self, lines: str):
        lines_split = lines.strip("\r\n").splitlines()
        indent = len(lines_split[0]) - len(lines_split[0].lstrip())
        start = 0
        if all([l.startswith(" " * indent) or l == "" for l in lines_split]):
            start = indent
        for l in lines_split:
            self.println(l[start:].rstrip())

    def merge(self, stream: "Stream"):
        self._write(stream.buffer)

    def indent(self):
        self.indent_level += 1

    def dedent(self):
        self.indent_level -= 1


# dictionary from type or function to a list of names that it depends on (and must import)
dependencies = {}
# a list of dummy types created to fill in missing types in the module
dummy_types = []
# a dictionary for any types that need forward references, to a list of all types that
# must be forward referenced
fwd_ref = {}


# recursively decompose a dependency annotation and process all sub-dependencies.
# this both accumulates a list of dependencies as well as returns the unqualified
# name of the annotation, so List[foo.bar] will become just List[bar] to account
# for later `import bar from foo`
#
# List[Tuple[foo, bar]] will find [typing.List, typing.Tuple, foo and bar] as dependencies
#
# context will be either the parent module or class
def process_annotation(context: Any, deps: Optional[List[str]], annot: str) -> str:
    # only have to decompose things with []
    if "[" not in annot:
        # don't add dependencies on builtins, there's no need
        if deps is not None and not annot.startswith("builtins."):
            deps.append(annot)

        # grab only the innermost name of the type
        ret = annot.split(".")[-1]
        # escape in strings our own name
        if ret == context.__name__:
            return f"'{ret}'"
        # as well as anything that must be forward refereced
        if context.__name__ in fwd_ref and ret in fwd_ref[context.__name__]:
            return f"'{ret}'"
        return ret

    callable = annot.startswith("Callable") or annot.startswith("typing.Callable")

    # use non-eval path if possible, only available on python 3.14 though :(
    # we make a locals set from the module, and add top-level imported modules
    # so that e.g. datetime.datetime can be found
    locals = dict(sys.modules[context.__module__].__dict__)
    locals.update(sys.modules)
    while True:
        try:
            from typing import ForwardRef, evaluate_forward_ref  # type: ignore

            dep_type = evaluate_forward_ref(
                ForwardRef(annot), globals=globals(), locals=locals
            )
            break
        # if we hit a NameError this is a type that is otherwise unknown,
        # create a fake type that will then be declared as a dummy type
        except NameError as n:
            name = re.findall(r"'([^']*)'", str(n))[0]
            dummy_types.append(name)
            globals()[name] = TypeVar(name)  # type: ignore
            continue
        # if the import failed, we have to use eval()
        except ImportError:
            try:
                eval_annot = annot
                if callable:
                    eval_annot = eval_annot.replace("NoneType", "None")
                    # TypeVars will get printed as ~Type
                    eval_annot = eval_annot.replace("~", "")
                dep_type = eval(eval_annot, globals(), locals)
                break
            except NameError as n:
                name = re.findall(r"'([^']*)'", str(n))[0]
                dummy_types.append(name)
                globals()[name] = TypeVar(name)  # type: ignore
                continue
            except:
                raise ValueError(
                    f"Couldn't evaluate dependency '{annot}' in {context.__module__}.{context.__name__}"
                )

    # we only expect to get typing declarations like this, e.g. Tuple[] List[] etc.
    if dep_type.__module__ != "typing":
        raise ValueError("Expected typing type in complex dependency")

    # add a dependency on the typing object itself
    optional = union = False
    # detect Optional[] / Union[]
    if hasattr(dep_type, "__origin__") and dep_type.__origin__ is typing.Union:
        # Union with just [x, None] is Optional
        if type(None) in dep_type.__args__ and len(dep_type.__args__) == 2:
            optional = True
        else:
            union = True
    if deps is not None:
        if optional:
            deps.append(f"typing.Optional")
        elif union:
            deps.append(f"typing.Union")
        elif "_name" in dir(dep_type):
            deps.append(f"typing.{dep_type._name}")
        else:
            deps.append(f"typing.{dep_type.__name__}")

    # iterate over all inner types
    inner = []
    for a in dep_type.__args__:
        # add some special cases
        if a == Ellipsis:
            inner.append("...")
            continue
        if a is None or a is type(None):
            if not optional:
                inner.append("None")
            continue
        if a == Any:
            inner.append("Any")
            if deps is not None:
                deps.append("typing.Any")
            continue

        # for typing objects use str() so that we get the full type including arguments
        # we don't add to the dependency list here so dependencies are properly qualified
        if a.__module__ == "typing" and type(a) != TypeVar:
            inner.append(process_annotation(context, None, str(a)))
        else:
            inner.append(process_annotation(context, None, a.__name__))

        # get the module for this inner type, which may be complex for enums
        module = a.__module__
        if module.startswith("importlib") or module.startswith("_frozen_importlib"):
            module = [
                m.__name__ for m in sys.modules.values() if a.__name__ in m.__dict__
            ][0]

        # use the 'real' name for TypeVars, or for local references
        if module == context.__module__ or type(a) == TypeVar:
            process_annotation(context, deps, a.__name__)
        # for typing objects use str() so that we get the full type including arguments
        elif module == "typing":
            process_annotation(context, deps, str(a))
        # process the type under its name qualified by its module otherwise
        else:
            process_annotation(context, deps, f"{module}.{a.__name__}")

    # Callables must format their arguments and return type (the last argument)
    if callable:
        ret = inner[-1]
        del inner[-1]
        inner = ", ".join(inner)
        if "_name" in dir(dep_type):
            assert dep_type._name is not None
            return f"{dep_type._name}[[{inner}], {ret}]"
        else:
            return f"{dep_type.__name__}[[{inner}], {ret}]"

    inner = ", ".join(inner)
    if optional:
        return f"Optional[{inner}]"
    elif union:
        return f"Union[{inner}]"
    elif "_name" in dir(dep_type):
        assert dep_type._name is not None
        return f"{dep_type._name}[{inner}]"
    else:
        return f"{dep_type.__name__}[{inner}]"


def add_dependencies(context: Any, deps: List[str], annot: str):
    process_annotation(context, deps, annot)


def unqualify(context: Any, annot: str) -> str:
    return process_annotation(context, None, annot)


# get all the dependencies recursively for a given object
def get_all_deps(name: str) -> List[str]:
    def recurse(name, all_deps, processed):
        processed.append(name)
        for dep in dependencies[name]:
            if dep not in all_deps:
                all_deps.append(dep)
            if dep not in processed and dep in dependencies:
                all_deps = recurse(dep, all_deps, processed)
        return all_deps

    return recurse(name, [], [])


# get the tuple needed for importing a given dependency
def get_import(dep: str) -> Tuple[Optional[str], Optional[str]]:
    if dep in builtins.__dict__ or dep == "None":
        return (None, None)

    if dep in typing.__dict__:
        return ("typing", dep)

    if dep == "datetime":
        return ("datetime", "datetime")

    # real thing to import

    # local dependency
    if "." not in dep:
        return (".", dep)
    else:
        mod, obj = dep.split(".")
        return (mod, obj)


# get all of the imports of a given type
def collect_imports(deps: Set[str]) -> Dict[str, List[str]]:
    ret = {}

    for dep in deps:
        mod, obj = get_import(dep)
        if obj is None:
            continue
        if mod not in ret:
            ret[mod] = []
        ret[mod].append(obj)

    for m in ret.keys():
        ret[m].sort()

    return ret


# generate a function's stubs into a stream,
# either a global function or a method in a class
def gen_function(file: Stream, class_parent: Any, func: Callable):
    global dependencies

    args = []
    ret = ""

    qualname = func.__name__
    if class_parent is not None:
        qualname = f"{class_parent.__name__}.{qualname}"

    deps = []

    context = class_parent
    if context is None:
        context = func

    # pull return type and all parameters out of the function's documentation
    if func.__doc__ is None:
        raise ValueError("Unexpected None docstring")

    match = RTYPE_PATTERN.search(func.__doc__)
    if match is not None:
        add_dependencies(context, deps, match[1])
        ret = f" -> {unqualify(context, match[1])}"

    for param in PARAM_PATTERN.findall(func.__doc__):
        type: str = param[0]
        name: str = param[1]
        default: str = param[2].lstrip()

        # enforce spacing around the default value
        if len(default) > 0 and default[0] == "=":
            default = default[1:].lstrip()
            while default.count(".") > 1:
                default = default[default.index(".") + 1 :]
            default = " = " + default

        add_dependencies(context, deps, type)
        type = unqualify(context, type)
        # rename any arguments that are python keywords - e.g. 'from'
        if keyword.iskeyword(name):
            name += "_"
        args.append(f"{name}: {type}{default}")

    if class_parent is not None and inspect.ismethoddescriptor(func):
        args.insert(0, "self")

    args = ", ".join(args)

    file.println("")
    if class_parent is not None and not inspect.ismethoddescriptor(func):
        file.println("@staticmethod")
    file.println(f"def {func.__name__}({args}){ret}:")
    file.indent()
    file.println('"""')
    docstring = func.__doc__.strip()
    i = 0
    while docstring.startswith(f"{func.__name__}("):
        i += 1
        if i > 100:
            print(docstring)
        # trim any auto-generated function signatures
        try:
            docstring = docstring[docstring.index("\n") :].strip()
        except ValueError:
            break
    file.printlines(docstring)
    file.println('"""')
    file.println("pass")
    file.dedent()
    file.println("")

    dependencies[qualname] = set(deps)
    return dependencies[qualname]


# find a .. data definition in a docstring, for either enum values or const
# integers
def get_data_docstring(docstring, item_name) -> Optional[str]:
    offs = 0
    while True:
        result = DATA_PATTERN.search(docstring, offs)
        # stop if there are no more datas - we should have one for each, but don't fail
        if result is None:
            return None

        # check if this is the data declaration we're looking for
        data_decl = result.group(1).strip()
        if data_decl == item_name:
            # it is! see if there's another data after
            data_start = result.end(1)
            next_result = DATA_PATTERN.search(docstring, data_start)
            # if there isn't, the docstring is the remainder of the class doc,
            # otherwise the docstring stops at the next data
            if next_result is None:
                return docstring[data_start:]
            else:
                return docstring[data_start : next_result.start(0)]
        offs = result.start(1)


# generate the stub for a class
def gen_class(file: Stream, class_obj: Type):
    global dependencies

    bases = [b for b in class_obj.__bases__ if not shouldskip(b.__name__)]

    deps = []

    if len(bases) > 0:
        for b in bases:
            if b.__module__ == class_obj.__module__:
                deps.append(b.__name__)
            else:
                deps.append(f"{b.__module__}.{b.__name__}")

    bases_string = ", ".join([b.__name__ for b in bases])

    if class_obj.__doc__ is None:
        raise ValueError("Unexpected None docstring")

    lines = class_obj.__doc__.strip().splitlines()

    constructors: List[List[Tuple[str, str]]] = []
    while lines[0].strip().startswith(class_obj.__name__ + "("):
        args = lines[0].strip()
        start = args.find("(")
        args = args[start + 1 : -1]
        annot_split = lambda arg: (arg.split(":")[0].strip(), arg.split(":")[1].strip())
        constructors.append([annot_split(arg) for arg in args.split(",") if arg != ""])
        del lines[0]

    if len(constructors) > 0:
        file.println("from typing import overload")
        file.println("")

    class_doc = ("\n".join(lines)).strip()

    file.println(f"class {class_obj.__name__}({bases_string}):")
    file.indent()

    file.println("# Original docstring")
    file.println('"""')
    file.printlines(class_doc)
    file.println('"""')
    file.println("")
    file.println("")

    enum_docs = {}

    # enums have special handling
    if isinstance(class_obj, enum.EnumMeta):
        file.println("# Enum values")
        file.println("")

        commented = False
        for stage in [1, 2]:
            for item_name in class_obj.__dict__.keys():
                if item_name[0] == "_" and item_name[-1] == "_":
                    continue

                item = getattr(class_obj, item_name)

                # declare members that evaluate to themselves first.
                # these are the priority when dealing with any aliases so should come first
                if item.name != item_name and stage == 1:
                    continue
                if item.name == item_name and stage == 2:
                    continue

                if stage == 2 and not commented:
                    commented = True
                    file.println("# Aliases")
                    file.println("")

                file.println(f"{item_name} = {item.value}")

                enum_doc = get_data_docstring(class_obj.__doc__, item_name)

                if enum_doc is None:
                    # allow First/Count values to be undocumented
                    if item_name == "First":
                        enum_doc = "The first enum value, for ease of iteration."
                    elif item_name == "Count":
                        enum_doc = (
                            "The number of values in the enum, for ease of iteration."
                        )
                    else:
                        raise ValueError(
                            f"Couldn't find enum docstring for {item_name}"
                        )

                enum_docs[item_name] = enum_doc
                file.println('"""')
                file.printlines(enum_doc)
                file.println('"""')
                file.println("")
    else:
        for ctor in constructors:
            ctor_def = f"def __init__(self, "
            for param, annot in ctor:
                if annot != class_obj.__name__:
                    # a default value comes in with the annotation,
                    # we don't split it out otherwise so strip it here
                    type_str = annot.split("=")[0].strip()
                    add_dependencies(class_obj, deps, type_str)
                    ctor_def += f"{param}: {annot}, "
                else:
                    # escape any self-references in ''s
                    ctor_def += f"{param}: '{annot}', "

            ctor_def = ctor_def[:-2] + "):"

            file.println("@overload")
            file.println(ctor_def)
            file.indent()
            file.println('"""')
            # copy constructors have only one parameter of our own type
            if len(ctor) == 1 and ctor[0][1] == class_obj.__name__:
                file.println(
                    f"Construct a new {class_obj.__name__} with a deep copy of the input."
                )
            # default constructors have no parameters
            elif ctor == []:
                file.println(
                    f"Construct a new default-initialised {class_obj.__name__}."
                )
            # more complex value constructor with parameters
            else:
                file.println(
                    f"Construct a new {class_obj.__name__} using provided values."
                )
            file.println('"""')
            file.println("pass")
            file.dedent()
            file.println("")
        if len(constructors) > 0:
            file.println("")
            file.println("def __init__(self): pass")
            file.println("")

        for item_name in class_obj.__dict__.keys():
            if item_name.startswith("__"):
                continue
            if shouldskip(item_name):
                continue

            item = getattr(class_obj, item_name)

            if item.__doc__ is None:
                raise ValueError("Unexpected None docstring")

            if inspect.ismethoddescriptor(item) or inspect.isbuiltin(item):
                deps += gen_function(file, class_obj, item)
            elif inspect.isgetsetdescriptor(item):
                doc = TYPE_PATTERN.search(item.__doc__)
                if doc is None:
                    raise ValueError(
                        f"Didn't find type pattern in docstring for {item.__name__}"
                    )
                type_str = doc[1]

                if type_str != class_obj.__name__:
                    add_dependencies(class_obj, deps, type_str)

                type_str = unqualify(class_obj, type_str)

                # we generate properties here so we can attach docstrings

                file.println("@property")
                file.println(f"def {item_name}(self) -> {type_str}:")
                file.indent()
                file.println('"""')
                file.printlines(item.__doc__)
                file.println('"""')
                file.println("pass")
                file.dedent()
                file.println("")
                file.println(f"@{item_name}.setter")
                file.println(f"def {item_name}(self, value: {type_str}):")
                file.indent()
                file.println("pass")
                file.dedent()
                file.println("")
            elif type(item) is int:
                doc = get_data_docstring(class_obj.__doc__, item_name)

                if doc is None:
                    raise ValueError("Unexpected None data docstring")

                # this comment doesn't get picked up by docstrings anywhere but is useful if you jump-to-definition
                # and some IDEs may still process it
                file.println(f"{item_name} = {item}")
                file.println('"""')
                file.printlines(doc)
                file.println('"""')
                file.println("")
            else:
                raise ValueError(
                    f"Unknown type of member {item_name} in {class_obj.__name__}"
                )

    file.dedent()

    if len(enum_docs) > 0:
        file.println("")
        file.println("# Assign __doc__ for enum values for easier introspection")
        file.println("")
    for val in enum_docs.keys():
        file.println(f'{class_obj.__name__}.{val}.__doc__ = """')
        file.printlines(enum_docs[val])
        file.println('"""')
        file.println("")

    # don't declare dependencies on ourselves even if we have recursive members or
    # functions
    deps = set(deps)
    if class_obj.__name__ in deps:
        deps.remove(class_obj.__name__)

    dependencies[class_obj.__name__] = deps


def gen(module: types.ModuleType, destpath: str):
    global dependencies, dummy_types, fwd_ref

    begin = time.time()

    dependencies.clear()
    dummy_types.clear()
    fwd_ref.clear()

    output_basepath = os.path.join(destpath, module.__name__)
    if __file__ in dir(module):
        print(
            f"Generating stubs for {module.__name__} from {module.__file__}, writing to {output_basepath}"
        )
    else:
        print(f"Generating stubs for {module.__name__}, writing to {output_basepath}")

    shutil.rmtree(output_basepath, ignore_errors=True)
    os.makedirs(output_basepath, exist_ok=True)

    separate_decls = {}
    inline_decls = {}

    for item_name in dir(module):
        if item_name.startswith("__"):
            continue
        if shouldskip(item_name):
            continue

        if "_" in item_name:
            segments = item_name.split("_")
            if hasattr(module, segments[0]) and inspect.isclass(
                getattr(module, segments[0])
            ):
                continue

        item = getattr(module, item_name)

        if inspect.isclass(item):
            decl = Stream(None)
            gen_class(decl, item)

            separate_decls[item_name] = decl

            if item.__doc__ is None:
                raise ValueError("Unexpected None docstring")

            # Find synthetic functions documented that don't exist -
            # this is used for things like callback declarations and additional
            # documentation on them
            result = FUNC_PATTERN.search(item.__doc__)

            while result is not None:
                # check if this is the data declaration we're looking for
                func_name = result.group(1).strip()
                offs = result.end(0)
                next_result = ANY_PATTERN.search(item.__doc__, offs)
                if next_result is None:
                    func_doc = item.__doc__[offs:]
                else:
                    func_doc = item.__doc__[offs : next_result.start(0)]

                args = []
                ret = ""

                deps = []

                match: Optional[re.Match[str]] = RTYPE_PATTERN.search(func_doc)
                if match is not None:
                    add_dependencies(item, deps, match[1])
                    ret = f" -> {unqualify(item, match[1])}"

                for param in PARAM_PATTERN.findall(func_doc):
                    typename: str = param[0]
                    name: str = param[1]
                    add_dependencies(item, deps, typename)
                    typename = unqualify(item, typename)
                    args.append(f"{name}: {typename}")

                args = ", ".join(args)

                decl = Stream(None)

                decl.println("")
                decl.println("# Synthetic function")
                decl.println(f"def {func_name}({args}){ret}:")
                decl.indent()
                decl.println('"""')
                decl.printlines(func_doc)
                decl.println('"""')
                decl.println("pass")
                decl.dedent()
                decl.println("")

                separate_decls[func_name] = decl

                dependencies[func_name] = set(deps)

                result = FUNC_PATTERN.search(item.__doc__, offs)
        elif inspect.isfunction(item) or inspect.isbuiltin(item):
            decl = Stream(None)
            gen_function(decl, None, item)

            inline_decls[item_name] = decl
        else:
            raise ValueError(
                f"Unknown type of object {item_name} in module root: {type(item)}"
            )

    file_map = {}
    circular_groups = {}

    all_deps = {}
    for name in separate_decls.keys():
        all_deps[name] = get_all_deps(name)

    circular = 1

    # detect circular dependencies
    for name in separate_decls.keys():
        # if we've already merged this one previously, skip it
        if name in file_map.keys():
            continue

        group = [name]

        # for each dependency
        for dep in all_deps[name]:
            if dep not in all_deps.keys():
                continue
            if dep in group:
                continue

            # if we can get back to the existing group from its dependencies
            if len(set(group).intersection(set(all_deps[dep]))) > 0:
                # it's part of the group
                group.append(dep)

        if len(group) > 1:
            group.sort()
            group_list = ", ".join(group)
            print(f"Circular dependency detected: {group_list}")

            filename = f"{module.__name__}_circular{circular}"
            circular += 1
            for g in group:
                file_map[g] = filename
                circular_groups[g] = group

                for h in group:
                    if g == h:
                        continue
                    if g not in fwd_ref:
                        fwd_ref[g] = []
                    fwd_ref[g].append(h)

                # regenerate the classes to use the forward references
                decl = Stream(None)
                gen_class(decl, getattr(module, g))

                separate_decls[g] = decl

                # remove the direct dependencies between the group
                for h in group:
                    if h in dependencies[g]:
                        dependencies[g].remove(h)

    all_deps = set()

    for deps in dependencies.values():
        all_deps = all_deps.union(deps)

    imports = collect_imports(all_deps)

    for local in imports["."]:
        if local not in dependencies:
            if local not in dummy_types:
                dummy_types.append(local)

    if len(dummy_types) > 0:
        print(
            f"WARNING: Some types could not be found, and were mapped to dummy types:"
        )
        print("    " + ", ".join(list(dummy_types)))

    for dummy in dummy_types:
        dependencies[dummy] = set()

    with open(
        os.path.join(output_basepath, "__init__.py"),
        mode="w",
        newline=nl,
        encoding="utf-8",
    ) as out_init:
        init = Stream(out_init)

        init.println("# Stubs for {module.__name__}")
        init.println("")

        func_deps = set()

        for name, stream in inline_decls.items():
            func_deps = func_deps.union(dependencies[name])

        imports = collect_imports(func_deps)

        needed_dummies = sorted(list(dummy_types))

        # don't need to import anything locally, it's already going to be imported below in the classes
        del imports["."]

        for dep in sorted(imports.keys()):
            objs = ", ".join(imports[dep])
            init.println(f"from {dep} import {objs}")

        if len(needed_dummies) > 0:
            init.println("")
            init.println("# Dummy types")
            init.println("from typing import NewType, Optional")
            for dummy in needed_dummies:
                init.println(f"{dummy} = Optional[NewType('{dummy}', int)]")

        init.println("")
        init.println("# Classes")

        files_written = []

        for name, stream in separate_decls.items():

            filename = name
            circular = False
            if name in file_map.keys():
                filename = file_map[filename]
                circular = True

                with open(
                    os.path.join(output_basepath, f"_{name}.py"),
                    mode="a",
                    newline=nl,
                    encoding="utf-8",
                ) as file:
                    out = Stream(file)

                    out.println("# Classes in combined file for circular dependency")
                    out.println(f"from .{filename} import {name}")
            else:
                filename = "_" + name

            with open(
                os.path.join(output_basepath, filename + ".py"),
                mode="a",
                newline=nl,
                encoding="utf-8",
            ) as file:
                out = Stream(file)

                circular = None

                if circular and filename not in files_written:
                    out.println(
                        "# File with multiple classes to resolve circular dependency"
                    )
                    circular = circular_groups[name]
                    classes = ", ".join(circular)
                    out.println(f"# {classes}")
                    out.println("")

                needed_dummies = []

                imports = collect_imports(dependencies[name])
                for dep in sorted(imports.keys()):
                    if dep == ".":
                        for obj in imports[dep]:
                            if obj in dummy_types:
                                needed_dummies.append(obj)
                            else:
                                out.println(f"from ._{obj} import {obj}")
                    else:
                        objs = ", ".join(imports[dep])
                        out.println(f"from {dep} import {objs}")

                if circular is not None:
                    for g in circular:
                        imports = collect_imports(dependencies[name])
                        if "." in imports.keys():
                            for obj in imports["."]:
                                if obj in dummy_types and obj not in needed_dummies:
                                    needed_dummies.append(obj)
                    pass

                if len(needed_dummies) > 0 and filename not in files_written:
                    needed_dummies.sort()
                    out.println("")
                    out.println("# Dummy types")
                    for dummy in needed_dummies:
                        out.println(f"from . import {dummy}")

                if len(dependencies[name]) > 0:
                    out.println("")

                out.merge(stream)

                files_written.append(filename)

            init.println(f"from .{filename} import {name}")

        init.println("")
        init.println("# Functions")

        # functions do not have dependencies on each other, can be done in any order
        for name, stream in inline_decls.items():
            init.println("")
            init.merge(stream)

    end = time.time()
    print(f"Generated in {int((end-begin)*1000)} ms")
    print("")
