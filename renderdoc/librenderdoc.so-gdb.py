import re

class rdcstrPrinter(object):
    def __init__(self, val):
        self.val = val

    def to_string(self):
        if self.val['d']['fixed']['flags'] & 0xC000000000000000:
            return self.val['d']['alloc']['str']
        return self.val['d']['arr']['str'].cast(gdb.lookup_type("char").pointer())

    def display_hint(self):
        return 'string'

class rdcarrayPrinter(object):
    class _iterator():
        def __init__(self, value_type, elems, usedCount):
            self.value_type = value_type
            self.elems = elems
            self.usedCount = usedCount
            self.i = 0

        def __iter__(self):
            return self

        def __next__(self):
            if self.i >= self.usedCount:
                raise StopIteration
            ret = ('[{}]'.format(self.i), self.elems[self.i])

            self.i = self.i + 1
            return ret

    def __init__(self, val):
        self.val = val
        self.value_type = self.val.type.template_argument(0)
        
    def children(self):
        return self._iterator(self.value_type, self.val['elems'], self.val['usedCount'])

    def to_string(self):
        return None

    def display_hint(self):
        return 'array'

import gdb.printing

def register(objfile):
    """Register the pretty printers within the given objfile."""

    printer = gdb.printing.RegexpCollectionPrettyPrinter('renderdoc')

    printer.add_printer('rdcstr', r'^rdcstr$', rdcstrPrinter)
    printer.add_printer('rdcarray', r'^rdcarray<.*>$', rdcarrayPrinter)

    if objfile == None:
        objfile = gdb

    gdb.printing.register_pretty_printer(objfile, printer)

register(gdb.current_objfile())
