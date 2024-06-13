#!/usr/bin/env python3

import os
import sys
import struct

if len(sys.argv) != 2:
    print(f"Usage: {sys.argv[0]} [path/to/stubs/folder]")
    sys.exit(1)
    
# path to module libraries for windows
if struct.calcsize("P") == 8:
    binpath = '../x64/'
else:
    binpath = '../Win32/'

# Prioritise release over development builds
sys.path.insert(0, os.path.abspath(binpath + 'Development/pymodules'))
sys.path.insert(0, os.path.abspath(binpath + 'Release/pymodules'))

# Add the build paths to PATH so renderdoc.dll can be located
os.environ["PATH"] += os.pathsep + os.path.abspath(binpath + 'Development/')
os.environ["PATH"] += os.pathsep + os.path.abspath(binpath + 'Release/')

if sys.platform == 'win32' and sys.version_info[1] >= 8:
    os.add_dll_directory(binpath + 'Release/')
    os.add_dll_directory(binpath + 'Development/')

# path to module libraries for linux
sys.path.insert(0, os.path.abspath('../build/lib'))

from stubs_generation.helpers import generator3

if __name__ == '__main__':
    generator3.main(['renderdoc', '-d', sys.argv[1]])
    generator3.main(['qrenderdoc', '-d', sys.argv[1]])
