#!/usr/bin/env python3

import os
import sys
import struct
from types import ModuleType

if len(sys.argv) < 2:
    print(f"Usage: {sys.argv[0]} [path/to/tmp/folder]")
    sys.exit(1)

destpath = os.path.join(os.path.realpath(sys.argv[1]), "rdstubs")

os.makedirs(destpath, exist_ok=True)

# do everything relative to this script
docsdir = os.path.realpath(os.path.dirname(__file__))

# path to module libraries for windows
if struct.calcsize("P") == 8:
    binpath = os.path.abspath(os.path.join(docsdir, "../x64/"))
else:
    binpath = os.path.abspath(os.path.join(docsdir, "../Win32/"))

# Prioritise release over development builds
sys.path.insert(0, os.path.abspath(binpath + "/Development/pymodules"))
sys.path.insert(0, os.path.abspath(binpath + "/Release/pymodules"))

# Add the build paths to PATH so renderdoc.dll can be located
os.environ["PATH"] += os.pathsep + os.path.abspath(binpath + "/Development/")
os.environ["PATH"] += os.pathsep + os.path.abspath(binpath + "/Release/")

if sys.platform == "win32" and sys.version_info[1] >= 8:
    if os.path.exists(binpath + "/Release/"):
        os.add_dll_directory(binpath + "/Release/")
    if os.path.exists(binpath + "/Development/"):
        os.add_dll_directory(binpath + "/Development/")

# path to module libraries for linux
sys.path.insert(0, os.path.abspath(os.path.join(docsdir, "../build/lib")))

import shutil

shutil.rmtree(destpath)

import renderdoc as rd_module
import qrenderdoc as qrd_module
import stubgen

stubgen.gen(rd_module, destpath)
stubgen.gen(qrd_module, destpath)

sys.path.insert(0, os.path.dirname(destpath))

from rdstubs import renderdoc as rd_stub  # type: ignore
from rdstubs import qrenderdoc as qrd_stub  # type: ignore

import inspect, enum

for mod, stub in [(rd_module, rd_stub), (qrd_module, qrd_stub)]:
    stub: ModuleType

    # we enforce in naming checks that top level items like classes and functions do not contain a _
    mod_items = [x for x in dir(mod) if "_" not in x]
    stub_items = [x for x in dir(stub) if "_" not in x]

    missing = set(mod_items) - set(stub_items)

    if len(missing) > 0:
        raise RuntimeError(
            f"Some module items do not have a corresponding stub item: {list(missing)}"
        )

    for name, mod_item, stub_item in [
        (x, getattr(mod, x), getattr(stub, x)) for x in mod_items
    ]:
        if inspect.isclass(mod_item):
            if issubclass(mod_item, enum.Enum):
                members = mod_item.__members__
                for val in members.keys():
                    if not hasattr(stub_item, val):
                        raise RuntimeError(
                            f"{mod.__name__}.{name} stub for enum is missing {val}"
                        )
                    if getattr(stub_item, val) != members[val]:
                        raise RuntimeError(
                            f"{mod.__name__}.{name} stub for enum has wrong value: {members[val]} vs {getattr(stub_item, val)}"
                        )
            else:
                swig_vars = [
                    "disown",
                    "acquire",
                    "own",
                    "append",
                    "next",
                    "this",
                    "thisown",
                ]
                members = [
                    x for x in dir(mod_item) if x[0] != "_" and x not in swig_vars
                ]

                for member_name in members:
                    if not hasattr(stub_item, member_name):
                        raise RuntimeError(
                            f"{mod.__name__}.{name} stub for class is missing {member_name}"
                        )

                    mod_member = getattr(mod_item, member_name)
                    stub_member = getattr(stub_item, member_name)

                    if inspect.ismethoddescriptor(mod_member) or inspect.isbuiltin(
                        mod_member
                    ):
                        if not inspect.isfunction(stub_member):
                            raise RuntimeError(
                                f"{mod.__name__}.{name}.{member_name} stub is not a member function"
                            )
                    elif inspect.isgetsetdescriptor(mod_member):
                        if not isinstance(stub_member, property):
                            raise RuntimeError(
                                f"{mod.__name__}.{name}.{member_name} stub is not a property"
                            )
                    elif isinstance(mod_member, int):
                        if mod_member != stub_member:
                            raise RuntimeError(
                                f"{mod.__name__}.{name}.{member_name} stub has wrong value: {mod_member} vs {stub_member}"
                            )

shutil.rmtree(destpath)

print("Generated stubs look OK")