#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#

import sys
import re
import os
import glob
import argparse
import inspect
from enum import EnumMeta
from typing import List

import struct

os.chdir(os.path.realpath(os.path.dirname(__file__)))

# path to module libraries for windows
if struct.calcsize("P") == 8:
    binpath = '../x64/'
else:
    binpath = '../Win32/'

# Prioritise release over development builds
sys.path.insert(0, os.path.abspath(binpath + 'Development/pymodules'))
sys.path.insert(0, os.path.abspath(binpath + 'Release/pymodules'))

# Add the build paths to PATH so renderdoc.dll can be located
os.environ["PATH"] = os.path.abspath(binpath + 'Development/') + os.pathsep + os.environ["PATH"]
os.environ["PATH"] = os.path.abspath(binpath + 'Release/') + os.pathsep + os.environ["PATH"]

# path to module libraries for linux
sys.path.insert(0, os.path.abspath('../build/lib'))

import renderdoc as rd
import qrenderdoc as qrd

parser = argparse.ArgumentParser()
parser.add_argument('-p', '--path', help="Add a path to interface files to search (can be used multiple times)", action='append')
parser.add_argument('-d', '--dump-combined', help="Set a path to dump the combined header that's being searched through", type=str)
parser.add_argument('--debug-mismatches', help="Set a path to a folder, each mismatch creates a file in here with the failure", type=str)
parser.add_argument('-v', '--verbose',
                    help="Run verbosely", action="store_true")
args = parser.parse_args()

paths = ['../renderdoc/api/replay', '../qrenderdoc/Code/Interface']
if args.path is not None:
    paths += args.path

if args.verbose:
    print("Searching for interface files in: {}".format(paths))

headers = ''

for path in paths:
    for f in glob.glob(os.path.join(os.path.abspath(path), '**', '*.h'), recursive=True):
        if args.verbose:
            print("Adding interface file {}".format(f))
        with open(f, 'r') as file:
            headers += file.read()
            
if args.dump_combined is not None:
    with open(args.dump_combined, 'w') as file:
        file.write(headers)

if args.debug_mismatches is not None and os.path.isdir(args.debug_mismatches):
    for f in glob.glob(os.path.join(os.path.abspath(args.debug_mismatches), '*')):
        if args.verbose:
            print("Removing {} in debug mismatches folder".format(f))
        os.unlink(f)

def make_c_type(ret: str, pattern: bool, typelist: List[str]):
    orig_type = ret

    # strip namespace
    if ret[0:10] == 'renderdoc.':
        ret = ret[10:]

    # Handle pipelines that are renamed
    if ret == 'D3D11State':
        ret = 'D3D11Pipe::State'
    elif ret == 'D3D12State':
        ret = 'D3D12Pipe::State'
    elif ret == 'GLState':
        ret = 'GLPipe::State'
    elif ret == 'VKState':
        ret = 'VKPipe::State'

    if ret in ['bool', 'void']:
        pass
    elif ret == 'str':
        ret = '(const )?rdc(inflexible)?str ?[&*]?' if pattern else 'rdcstr'
    elif ret == 'int':
        ret = '(u?int[163264]{2}|size)_t' if pattern else 'int' # ambiguous
    elif ret == 'float':
        ret = 'float|double' if pattern else 'float'
    elif ret == 'bytes':
        ret = '(const )?bytebuf ?[&*]?' if pattern else 'bytebuf'
    elif ret == 'List[Tuple[str,str]]': # special case
        ret = 'rdcstrpairs'
    elif ret == 'Tuple[str,str]': # special case
        ret = 'rdcstrpair'
    elif ret[0:5] == 'List[':
        inner = make_c_type(ret[5:-1], pattern, typelist)
        ret = '(const )?rdcarray<{}> ?[&*]?'.format(inner) if pattern else 'rdcarray<{}>'.format(inner)
    elif ret[0:6] == 'Tuple[':
        inners = [make_c_type(i.strip(), pattern, typelist) for i in ret[6:-1].split(',')]
        if pattern:
            inner = ',\s*'.join(inners)
        else:
            inner = ', '.join(inners)
        ret = '(const )?rdcpair<{}> ?[&*]?'.format(inner) if pattern else 'rdcpair<{}>'.format(inner)
    elif pattern:
        if ret[-8:] == 'Callback':
            ret = '(RENDERDOC_)?{}'.format(ret)
        else:
            if orig_type not in typelist:
                typelist.append(orig_type)

            ret = '(const )?I?{} ?[&*]?'.format(ret)
        
    return ret

RTYPE_PATTERN = re.compile(r":rtype: (.*)")
PARAM_PATTERN = re.compile(r":param ([^:]*) ([^: ]*):")
TYPE_PATTERN = re.compile(r":type: (.*)")

count = 0

def check_function(parent_name, objname, obj, source, global_func, typelist):
    global count, args

    if args.verbose:
        print("Checking {} function {}.{}".format('global' if global_func else 'member', parent_name, objname))

    docstring = obj.__doc__

    params = PARAM_PATTERN.findall(docstring)

    funcargs = ['', '']
    for p in params:
        if len(funcargs[0]) > 0:
            funcargs[0] += ',\s*'
            funcargs[1] += ', '
        funcargs[0] += make_c_type(p[0], True, typelist) + ' ?' + p[1] + "(\s*=[^,]*)?"
        funcargs[1] += make_c_type(p[0], False, typelist) + ' ' + p[1]

    result = RTYPE_PATTERN.search(docstring)
    if result is not None:
        ret = result.group(1)
    else:
        ret = 'void'

    global_pattern = ''
    if global_func:
        global_pattern = '(RENDERDOC_CC\s*RENDERDOC_)?'

    pattern = '(?s){} ?{}{}\(\s*{}\)'.format(make_c_type(ret, True, typelist), global_pattern, objname, funcargs[0])
    clean = '{} {}({})'.format(make_c_type(ret, False, typelist), objname, funcargs[1])

    match = re.search(pattern, source, re.MULTILINE | re.DOTALL)

    pattern2 = None
    # global functions returning strings can't return an rdcstr, they have to return const char *
    if match is None and ret == 'str':
        pattern2 = '(?s)const char \*{}{}\(\s*{}\)'.format(global_pattern, objname, funcargs[0])
        match = re.search(pattern2, source, re.MULTILINE | re.DOTALL)

    if match is None:
        count += 1
        print("Error {:3} in {}: {}".format(count, parent_name, clean))
        if args.debug_mismatches is not None and os.path.isdir(args.debug_mismatches):
            with open(os.path.join(os.path.abspath(args.debug_mismatches), '{:03}-{}.{}.txt'.format(count, parent_name, objname)), 'w') as file:
                file.write("# Failed to find matching declaration for {}.{}\n".format(parent_name, objname))
                file.write("#\n")
                file.write("# Matched against {}\n".format(pattern))
                if pattern2 is not None:
                    file.write("# Matched against {}\n".format(pattern2))
                file.write("#\n")
                file.write("#\n")
                file.write("\n")
                file.write("\n")
                file.write(source)

def check_used_types(objname, module, used_types):
    global count

    if args.verbose:
        print('Checking {} referenced types: {}'.format(objname, module, used_types))

    for t in used_types:
        type_name = t
        parent = module

        while True:
            if t in dir(parent):
                break

            # Allow some types that are opaque
            if parent == rd and t in ['ANativeWindow', 'NSView', 'CALayer', 'wl_display', 'wl_surface', 'HWND', 'xcb_connection_t', 'xcb_window_t', 'Display', 'Drawable']:
                break

            if parent == qrd and t in ['QWidget']:
                break

            idx = t.find('.')
            if idx >= 0:
                parent_name = t[0:idx]
                if parent_name in dir(parent):
                    parent = parent.__dict__[parent_name]
                elif parent_name == 'renderdoc':
                    parent = rd
                elif parent_name == 'qrenderdoc':
                    parent = qrd
                t = t[idx+1:]
                continue

            count += 1
            print("Error {:3} in {}: Unrecognised reference {}".format(count, objname, type_name))
            if type_name in dir(rd):
                print("  - Maybe missing namespace to refer to renderdoc.{}?".format(type_name))
            break

for mod_name in ['renderdoc', 'qrenderdoc']:
    mod = sys.modules[mod_name]
    if args.verbose:
        print("===== Checks for {} =====".format(mod_name))
    for objname in dir(mod):
        if re.search('__|SWIG|ResourceId_Null|rdcarray_of|Structured.*List', objname):
            continue

        # skip some functions that have special bindings and won't be easily found
        if objname in ['CreateRemoteServerConnection', 'DumpObject', 'GetDefaultCaptureOptions', 'GetSupportedDeviceProtocols']:
            if args.verbose:
                print("Skipping {}".format(objname))
            continue

        obj = mod.__dict__[objname]

        docstring = obj.__doc__

        if 'INTERNAL:' in docstring:
            continue

        qualname = '{}.{}'.format(mod_name, objname)

        if isinstance(obj, EnumMeta):
            if args.verbose:
                print("Skipping enum {}".format(qualname))

            # don't check enums
            continue
        elif isinstance(obj, type):
            if args.verbose:
                print("Checking class {}".format(qualname))

            # Grab the source to just this class to search in
            source = re.search('(struct|class|union) I?' + objname + '(\n|\s*:[^A-Za-z][\s:a-zA-Z]*\n)\{.*?^}', headers, re.MULTILINE | re.DOTALL)
            
            namespace = None

            if source is None and objname[0:2] in ['VK', 'GL']:
                pipe = objname[0:2] + 'Pipe'
                objname = objname[2:]

                namespace = re.search('namespace ' + pipe + '.* namespace ' + pipe, headers, re.MULTILINE | re.DOTALL)
                namespace = namespace.group(0)

            if source is None and objname[0:5] in ['D3D11', 'D3D12']:
                pipe = objname[0:5] + 'Pipe'
                objname = objname[5:]

                namespace = re.search('namespace ' + pipe + '.* namespace ' + pipe, headers, re.MULTILINE | re.DOTALL)
                namespace = namespace.group(0)

            if source is None and namespace is not None:
                source = re.search('(struct|class|union) I?' + objname + '[^{]*\{.*?^}', namespace, re.MULTILINE | re.DOTALL)
                
            source = source.group(0)

            instance = None
            try:
                instance = obj()
            except TypeError:
                pass

            for member_name in obj.__dict__.keys():
                if '__' in member_name or member_name in ['this', 'thisown']:
                    continue

                member = obj.__dict__[member_name]

                # Skip some known functions that cannot be easily matched this way
                if '{}.{}'.format(objname, member_name) in ['RemoteHost.Connect',
                                                            'CaptureContext.EditShader',
                                                            'ReplayController.DebugThread',
                                                            'ReplayController.GetHistogram',
                                                            'SDObject.AddChild',
                                                            'SDObject.AsInt',
                                                            'SDObject.AsFloat',
                                                            'SDObject.AsString']:
                    if args.verbose:
                        print("Skipping {}.{}".format(objname, member_name))
                    continue

                if callable(member):
                    used_types = []

                    check_function(qualname, member_name, member, source, False, used_types)

                    check_used_types('{}.{}'.format(qualname, member_name), mod, used_types)
                elif instance and '__get__' in dir(member):
                    value = getattr(instance, member_name)

                    type_name = type(value).__name__

                    if type(value).__module__ != mod_name:
                        type_name = type(value).__module__ + '.' + type_name

                    type_name = re.sub('(.*)rdcarray_of_(.*)', 'List[\\1\\2]', type_name)
                    type_name = re.sub('(renderdoc\.)?u?int[163264]{2}_t', 'int', type_name)
                    type_name = re.sub('(renderdoc\.)?rdcstr', 'str', type_name)
                    type_name = re.sub('Pipe_', '', type_name)
                    type_name = re.sub('StructuredBufferList', 'List[bytes]', type_name)
                    type_name = re.sub('StructuredObjectList', 'List[SDObject]', type_name)
                    type_name = re.sub('StructuredChunkList', 'List[SDChunk]', type_name)

                    # Maybe in future we could enforce :type: on all members? For now we
                    # only really care about ones we might want to access properties on,
                    # so not builtin types (lists/tuples excluded) or ResourceId
                    if type(value).__module__ not in [rd.__name__, qrd.__name__] or type_name == 'ResourceId' and type(value) is not tuple:
                        continue

                    if args.verbose:
                        print('Checking struct member {}.{}'.format(qualname, member_name))

                    result = TYPE_PATTERN.search(member.__doc__)
                    if result is not None:
                        type_decl = result.group(1)
                    else:
                        type_decl = None

                    if type_decl is None:
                        count += 1
                        print("Error {:3}: {}.{} is missing :type: declaration, should be {}".format(count, qualname, member_name, type_name))
                    elif type_decl != type_name:
                        count += 1
                        print("Error {:3}: {}.{} has wrong :type: declaration {}, should be {}".format(count, qualname, member_name, type_decl, type_name))
        elif callable(obj):
            used_types = []

            # check the function in all headers globally
            check_function(mod_name, objname, obj, headers, True, used_types)

            check_used_types(qualname, mod, used_types)

if count > 0:
    print("{} problems detected".format(count))
    sys.exit(count)

print("No problems detected")
sys.exit(0)
