#!/usr/bin/env python

if __name__ == '__main__':
    import sys
    extensions = ['AMD', 'ANDROID', 'ANGLE', 'APPLE', 'ARM', 'DMP', 'EXT', 'FJ', 'IMG', 'INTEL', 'KHR', 'NV', 'NVX', 'OES', 'OVR', 'QCOM', 'VIV']
    filenames = ['gl32.h', 'gl2ext.h']
    functions = {}
    for filename in filenames:
        functions[filename] = []
        with open(filename, 'rt') as f:
            for line in f:
                frags = line.split()
                if frags and frags[0] == 'GL_APICALL':
                    for frag in frags:
                        if frag.startswith('gl'):
                            functions[filename].append(frag)
        functions[filename].sort(key=str.lower)

    alias_extensions = []

    print('/******************************************************************************')
    print(' * The MIT License (MIT)')
    print(' *')
    print(' * Copyright (c) 2015-2016 Baldur Karlsson')
    print(' * Copyright (c) 2014 Crytek')
    print(' * Copyright (c) 2016 University of Szeged')
    print(' * Copyright (c) 2016 Samsung Electronics Co., Ltd.')
    print(' *')
    print(' * Permission is hereby granted, free of charge, to any person obtaining a copy')
    print(' * of this software and associated documentation files (the "Software"), to deal')
    print(' * in the Software without restriction, including without limitation the rights')
    print(' * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell')
    print(' * copies of the Software, and to permit persons to whom the Software is')
    print(' * furnished to do so, subject to the following conditions:')
    print(' *')
    print(' * The above copyright notice and this permission notice shall be included in')
    print(' * all copies or substantial portions of the Software.')
    print(' *')
    print(' * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR')
    print(' * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,')
    print(' * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE')
    print(' * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER')
    print(' * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,')
    print(' * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN')
    print(' * THE SOFTWARE.')
    print(' ******************************************************************************/\n')

    print('#pragma once\n')
    print('#include \"gles_common.h\"\n')

    print('struct GLHookSet')
    print('{')

    print('  // ++ dllexport')
    for f in functions['gl32.h']:
        aliases = ''
        for e in functions['gl2ext.h']:
            if e.startswith(f) and e[len(f):] in extensions:
                if aliases:
                    aliases += ', ' + e
                else:
                    aliases += '// aliases ' + e
                alias_extensions.append(e)
        print('  {:<60}{:<30}{}'.format('PFN' + f.upper() + 'PROC', f + ';', aliases))

    print('  // --\n\n  // ++ glext')
    for f in functions['gl2ext.h']:
        aliases = ''
        comment = '  '
        if f in alias_extensions:
            comment = '//'
        print('{}{:<60}{:<30}{}'.format(comment, 'PFN' + f.upper() + 'PROC', f + ';', aliases))
    print('  // --\n};\n')


