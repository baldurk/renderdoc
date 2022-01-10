#!/usr/bin/env python3

import sys
import os
import functools
from xml.sax.saxutils import escape
import xml.etree.ElementTree as ET

# on msys, use crlf output
nl = None
if sys.platform == 'msys':
    nl = "\r\n"

if len(sys.argv) <= 1:
    print('Usage: python3 {} filename.ui [filename2.ui ...]'.format(sys.argv[0]))
    sys.exit(0)

def sort_prop_key(a):
    # declare all non-<property> to be equal, and also greater than to all <property>
    # because sorted() is stable this means we only rearrange properties relative to each other
    # and put them at the front, the rest are kept in-order as-is
    if a.tag != "property":
        return "z"

    return "a" + a.get('name')

def sort_grid_key(a):
    # same with non-items in a grid layout, but at the front (these are e.g. properties)
    if a.tag != "item":
        return "a"

    return "z{:08}{:08}".format(int(a.get('row')), int(a.get('column')))

def canonicalise_ui(elem):

    # sort properties in alphabetical order. Unclear if Qt creator
    # has a fixed order for these, but it seems like it might?
    #elem[:] = sorted(elem, key=sort_prop_key)

    if elem.tag == "layout" and elem.get('class') == 'QGridLayout':
        elem[:] = sorted(elem, key=sort_grid_key)

    for e in elem:
        canonicalise_ui(e)

def write_ui_xml(f, elem, indent):
    f.write(' ' * indent)
    f.write('<{}'.format(elem.tag))
    for k,v in elem.items():
        f.write(' {}="{}"'.format(k,v))
    if elem.text or len(elem) > 0:
        f.write('>')
        if elem.text and len(elem.text.strip()) > 0:
            f.write(escape(elem.text).replace('"', '&quot;'))
        if len(elem) > 0:
            f.write('\n')
            for e in elem:
                iconset_tail = write_ui_xml(f, e, indent+1)
            if not iconset_tail:
                f.write(' ' * indent)
        f.write('</{}>'.format(elem.tag))
        # hack for weird iconset formatting
        if elem.tail is None or len(elem.tail.strip()) == 0:
            f.write('\n')
        else:
            f.write(elem.tail.strip())
            return True
    else:
        f.write('/>\n'.format(elem.tag))

    return False

for filename in sys.argv[1:]:
    print("Formatting {}...".format(filename))
    uifile = os.path.abspath(filename)

    ui = ET.parse(uifile)

    canonicalise_ui(ui.getroot())

    with open(uifile, mode='w', newline=nl, encoding='utf-8') as f:
        f.write('<?xml version="1.0" encoding="UTF-8"?>\n')

        write_ui_xml(f, ui.getroot(), 0)

