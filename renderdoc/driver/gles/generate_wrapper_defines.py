#!/usr/bin/env python
#
# The MIT License (MIT)
#
# Copyright (c) 2016 University of Szeged
# Copyright (c) 2016 Samsung Electronics Co., Ltd.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#

SupportedWrapperHeaderTemplate = """
#undef HookWrapper{IDX}
#define HookWrapper{IDX}(return_type, function{HAS_ARGS} {DEF_ARGS})                       \\
    typedef return_type (*CONCAT(function, _hooktype))({FUNCTION_ARGS});                                \\
    extern "C" __attribute__((visibility("default"))) return_type function({FUNCTION_ARGS});    \\
    extern return_type CONCAT(function, _renderdoc_hooked)({FUNCTION_ARGS});
"""

# required macros: SCOPED_LOCK_GUARD, DEBUG_WRAPPER(function), DEBUG_HOOKED(function), DRIVER()
SupportedWrapperImplementationTemplate = """
#undef HookWrapper{IDX}
#define HookWrapper{IDX}(return_type, function{HAS_ARGS} {DEF_ARGS})                       \\
    extern "C" __attribute__((visibility("default"))) return_type function({FUNCTION_ARGS})     \\
    {{  \\
        SCOPED_LOCK_GUARD();                \\
        DEBUG_WRAPPER(function);            \\
        return DRIVER()->function({ARGS});  \\
    }}  \\
    return_type CONCAT(function, _renderdoc_hooked)({FUNCTION_ARGS}) \\
    {{  \\
        SCOPED_LOCK_GUARD();                \\
        DEBUG_HOOKED(function);             \\
        return DRIVER()->function({ARGS});  \\
    }}
"""


UnSupportedWrapperHeaderTemplate = """
#undef HookWrapper{IDX}
#define HookWrapper{IDX}(return_type, function{HAS_ARGS} {DEF_ARGS})                       \\
  typedef return_type (*CONCAT(function, _hooktype))({FUNCTION_ARGS});                                \\
  extern CONCAT(function, _hooktype) CONCAT(unsupported_real_, function); \\
  extern "C" __attribute__((visibility("default"))) return_type function({FUNCTION_ARGS});    \\
  extern return_type CONCAT(function, _renderdoc_hooked)({FUNCTION_ARGS});
"""

# required macros: SCOPED_LOCK_GUARD, DEBUG_WRAPPER(function), DEBUG_HOOKED(function), DRIVER()
UnSupportedWrapperImplementationTemplate = """
#undef HookWrapper{IDX}
#define HookWrapper{IDX}(return_type, function{HAS_ARGS} {DEF_ARGS})                       \\
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL; \\
  extern "C" __attribute__((visibility("default"))) return_type function({FUNCTION_ARGS})     \\
  {{  \\
    return CONCAT(function, _renderdoc_hooked)({ARGS});  \\
  }}  \\
  return_type CONCAT(function, _renderdoc_hooked)({FUNCTION_ARGS}) \\
  {{  \\
    static bool hit = false;    \\
    if (hit == false)           \\
    {{                          \\
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); \\
      hit = true;               \\
    }}                          \\
    return CONCAT(unsupported_real_, function)({ARGS});  \\
  }}
"""



def generateDefines(write, template):

    write(template.format(IDX=0, FUNCTION_ARGS='', DEF_ARGS='', ARGS='', HAS_ARGS=""))
    for idx in range(1, 16):
        function_args = []
        def_args = []
        args = []
        for i in range(idx):
            function_args.append("T{0} A{0}".format(i))
            def_args.append("T{0}, A{0}".format(i))
            args.append("A{0}".format(i))

        write(template.format(IDX=idx,
                              FUNCTION_ARGS=', '.join(function_args),
                              DEF_ARGS=', '.join(def_args),
                              ARGS=', '.join(args),
                              HAS_ARGS=","))


if __name__ == "__main__":
    import sys
    if len(sys.argv) < 2:
        print("No output prefix specified!")
        print("Possible usage: {} gles_hooks".format(sys.argv[0]))
        sys.exit(-1)

    output_prefix = sys.argv[1]

    generate = [
        ( "_defines_supported.h", SupportedWrapperHeaderTemplate ),
        ( "_defines_supported_impl.h", SupportedWrapperImplementationTemplate),
        ( "_defines_unsupported.h", UnSupportedWrapperHeaderTemplate ),
        ( "_defines_unsupported_impl.h", UnSupportedWrapperImplementationTemplate),
    ]

    for item in generate:
        output_path = output_prefix + item[0]
        with open(output_path, "w") as header_file:
            generateDefines(header_file.write, item[1])
        print("File written: {}".format(output_path))
