#!/usr/bin/env python3

import os
import io
import sys
import re
import argparse
from collections import OrderedDict

parser = argparse.ArgumentParser(description='Generate macros for handling GL dispatch table.')
parser.add_argument('-m', '--maxparam', type=int, default=17,
                   help='The maximum number of parameters to generate')

parsed_args = parser.parse_args()

# on msys, use crlf output
nl = None
if sys.platform == 'msys':
    nl = "\r\n"

# Get the file, relative to this script's location (same directory)
# that way we're not sensitive to CWD
pathname = os.path.abspath(os.path.dirname(sys.argv[0])) + os.path.sep

# open the file for write
f = open(pathname + 'gl_dispatch_table_defs.h', mode='w', newline = nl)

# Finding definitions in the dispatch table header
def_regex = re.compile('(?P<typedef>PFN.*PROC) (?P<name>.*);(\s*\/\/ aliases +)?(?P<aliases>[a-zA-Z0-9_ ,]*)?')

# Finding a function definition in the official headers
func_regex = re.compile('(WINAPI|APIENTRY) (w?gl[A-Za-z_0-9]+)\s?\(')

# Finding a typedef in the official headers
typedef_regex = re.compile('^typedef (?P<return>[A-Za-z_0-9\s*]+)\([A-Z_ *]* (?P<typedef>PFN[A-Z_0-9]+)\) \((?P<args>.*)\);')

# Replacing float arg[2] with float *arg in definitions
array_regex = re.compile('([A-Za-z_][a-zA-Z_0-9]*) ([A-Za-z_][a-zA-Z_0-9]*)\[[0-9]*\]')

# Split an argument definition up by extracting the last full word
argsplit_regex = re.compile('(.*)([\*\s])([a-zA-Z0-9]+)')

# List of hooks to define, will be filled out when processing the dispatch table header
hooks = []

# A dict of typedef information
# Elements contain:
#   'used':      True if it's used by our definitions or not - False if unsupported
#   'function':  The name of the function defined with this typedef.
#                e.g. typedefs['PFNGLBEGINPROC']['function'] = 'glBegin'.
#   'return':    The return type
#   'args':      The list of arguments with types and arguments separated
#                e.g. [["int", "a"], ["float", "b"]]
typedefs = {}

# Open the dispatch table file
with open(pathname + "gl_dispatch_table.h", 'r') as fp:
    # For each line that defines a dispatch pointer, process it
    for func in [line.strip() for line in fp.readlines() if "PFN" in line]:
        match = def_regex.search(func)

        # All lines that contain a dispatch pointer should match the regex
        if not match:
            raise RuntimeError("Badly formed definition: {0}".format(func))

        # Split the list of aliases
        aliases = match.group('aliases')
        aliases = re.split(', *', aliases) if aliases != '' else []

        # Add the hook
        hook = { 'typedef': match.group('typedef'), 'name': match.group('name'), 'aliases': aliases }
        hooks.append(hook)

        # Add the typedefs for the base function and all aliases as used
        typedefs['PFN{0}PROC'.format(hook['name'].upper())] = {'used': True}
        for a in aliases:
            typedefs['PFN{0}PROC'.format(a.upper())] = {'used': True}

# Read all the official headers into a single string
official_headers = []
for header in ['glcorearb.h', 'glext.h', 'gl32.h', 'glesext.h', 'wglext.h', 'legacygl.h']:
    with open(pathname + 'official' + os.path.sep + header, 'r') as fp:
        official_headers += fp.readlines()

# Look for function definitions and add typedef function names.
for line in official_headers:
    match = func_regex.search(line)
    if match:
        typedef = 'PFN{0}PROC'.format(match.group(2).upper())
        if typedef not in typedefs:
            typedefs[typedef] = {'used': False}

        typedefs[typedef]['function'] = match.group(2)

# Now find typedefs and add return type/argument data
for line in official_headers:
    match = typedef_regex.search(line)
    if match:
        typedef = match.group('typedef')
        typedefs[typedef]['return'] = match.group('return').strip()

        args = match.group('args')

        if args == '' or args == 'void':
            args = []
        else:
            # Replace array arguments with pointers - see glPathGlyphIndexRangeNV
            args = array_regex.sub(r"\1 *\2", match.group('args'))
            # Create an array with each parameter as an element
            args = [a.strip() for a in args.split(',')]
            # Split up each argument
            args = [re.split(' *, *', argsplit_regex.sub(r"\1\2,\3", a)) for a in args]

        typedefs[typedef]['args'] = args

# f.write the file, starting with a template header
f.write('''
/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018-2019 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#pragma once

// This file is autogenerated with gen_dispatch_table.py - any changes will be overwritten next time
// that script is run.
// $ ./gen_dispatch_table.py

// We need to disable clang-format since this struct is programmatically generated
// clang-format off




'''.lstrip())

# f.write the 'definitions' of these hooks - can be used for stringification or doing
# GetProcAddress style 'check name, return function'
f.write('#define ForEachSupported(FUNC) \\\n')

for hook in hooks:
    f.write('  FUNC({}, {}); \\\n'.format(hook['name'], hook['name']))
    for a in hook['aliases']:
        f.write('  FUNC({}, {}); \\\n'.format(hook['name'], a))
        
f.write("\n\n\n\n")

# f.write the actual definitions - used to forward into FuncWrapperN/AliasWrapperN to define exported
# hook implementations
f.write('#define DefineSupportedHooks() \\\n')

for hook in hooks:
    typedef = typedefs[hook['typedef']]

    num = len(typedef['args'])

    arglist = ''
    for arg in typedef['args']:
        arglist += ', {}, {}'.format(arg[0], arg[1])

    f.write('  FuncWrapper{}({}, {}{}); \\\n'.format(num, typedef['return'], hook['name'], arglist))
    for a in hook['aliases']:
        f.write('  AliasWrapper{}({}, {}, {}{}); \\\n'.format(num, typedef['return'], a, hook['name'], arglist))
        
f.write("\n\n\n\n")

f.write('#define ForEachUnsupported(FUNC) \\\n')

for key in OrderedDict(sorted(typedefs.items())):
    typedef = typedefs[key]

    # Don't f.write for functions we support, or wgl/etc functions
    if typedef['used'] or typedef['function'][0:2] != 'gl':
        continue

    f.write('  FUNC({}); \\\n'.format(typedef['function']))

f.write("\n\n\n\n")

# For all typedefs not in the hooks, define them as unsupported
f.write('#define DefineUnsupportedHooks() \\\n')

for key in OrderedDict(sorted(typedefs.items())):
    typedef = typedefs[key]

    # Don't f.write for functions we support, or wgl/etc functions
    if typedef['used'] or typedef['function'][0:2] != 'gl':
        continue

    num = len(typedef['args'])

    arglist = ''
    for arg in typedef['args']:
        arglist += ', {}, {}'.format(arg[0], arg[1])

    f.write('  UnsupportedWrapper{}({}, {}{}); \\\n'.format(num, typedef['return'], typedef['function'], arglist))
    
# Now generate wrapper macros
f.write('''

        
// the _renderdoc_hooked variants are to make sure we always have a function symbol exported that we
// can return from GetProcAddress. On posix systems if another library (or the application itself)
// creates a symbol called 'glEnable' we'll return the address of that, and break badly. Instead we
// leave the 'naked' versions for applications trying to import those symbols, and declare the
// _renderdoc_hooked for returning as a func pointer. The raw version calls directly into the hooked
// version to hopefully allow the linker to tail-call optimise and reduce the overhead.

''')

template = '''
#define FuncWrapper{num}(ret, function{macroargs}) \\
  ret HOOK_CC CONCAT(function, _renderdoc_hooked)({argdecl}) \\
  {{ \\
    SCOPED_GLCALL(function); \\
    UNINIT_CALL(function, {argpass}); \\
    return glhook.driver->function({argpass}); \\
  }} \\
  HOOK_EXPORT ret HOOK_CC GL_EXPORT_NAME(function)({argdecl}) \\
  {{ \\
    return CONCAT(function, _renderdoc_hooked)({argpass}); \\
  }} \\
  HOOK_EXPORT ret HOOK_CC function({argdecl});

#define AliasWrapper{num}(ret, function, realfunc{macroargs}) \\
  ret HOOK_CC CONCAT(function, _renderdoc_hooked)({argdecl}) \\
  {{ \\
    SCOPED_GLCALL(function); \\
    UNINIT_CALL(realfunc, {argpass}); \\
    return glhook.driver->realfunc({argpass}); \\
  }} \\
  HOOK_EXPORT ret HOOK_CC GL_EXPORT_NAME(function)({argdecl}) \\
  {{ \\
    return CONCAT(function, _renderdoc_hooked)({argpass}); \\
  }} \\
  HOOK_EXPORT ret HOOK_CC function({argdecl});

#define UnsupportedWrapper{num}(ret, function{macroargs}) \\
  typedef ret(HOOK_CC *CONCAT(function, _hooktype))({argdecl}); \\
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL; \\
  ret HOOK_CC CONCAT(function, _renderdoc_hooked)({argdecl}) \\
  {{ \\
    static bool hit = false; \\
    if(hit == false) \\
    {{ \\
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); \\
      hit = true; \\
    }} \\
    if(!CONCAT(unsupported_real_, function)) \\
      CONCAT(unsupported_real_, function) = \\
          (CONCAT(function, _hooktype))glhook.GetUnsupportedFunction(STRINGIZE(function)); \\
    return CONCAT(unsupported_real_, function)({argpass}); \\
  }} \\
  HOOK_EXPORT ret HOOK_CC GL_EXPORT_NAME(function)({argdecl}) \\
  {{ \\
    return CONCAT(function, _renderdoc_hooked)({argpass}); \\
  }} \\
  HOOK_EXPORT ret HOOK_CC function({argdecl});

'''

for num in range(parsed_args.maxparam+1):
    macroargs = ', '.join([('t{0}, p{0}'.format(n+1)) for n in range(num)])
    argdecl = ', '.join([('t{0} p{0}'.format(n+1)) for n in range(num)])
    argpass = ', '.join([('p{0}'.format(n+1)) for n in range(num)])

    macroargs = ', ' + macroargs if num > 0 else macroargs

    f.write(template.format(num=num, macroargs=macroargs,
                          argdecl=argdecl, argpass=argpass))

f.close()
