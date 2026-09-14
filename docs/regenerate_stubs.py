#!/usr/bin/env python3

import argparse
import os
import sys
import struct

parser = argparse.ArgumentParser()
parser.add_argument("stubspath", help="path/to/stubs/folder")
parser.add_argument(
    "-v",
    "--verbose",
    help="Enable debugger mode, exceptions are not caught by the framework.",
    action="store_true",
)
parser.epilog
args = parser.parse_args()

destpath = os.path.realpath(args.stubspath)

if args.verbose:
    print(f"Writing python stubs to {destpath}")

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

import renderdoc
import qrenderdoc
import stubgen

stubgen.gen(renderdoc, destpath, verbose=args.verbose)
stubgen.gen(qrenderdoc, destpath, verbose=args.verbose)
