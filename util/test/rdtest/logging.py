import os
import sys
import re
import traceback
import mimetypes
import difflib
import shutil
from . import util


class TestFailureException(Exception):
    def __init__(self, message, *args):
        self.message = message
        self.files = []
        for a in args:
            self.files.append(str(a))

    def __str__(self):
        return self.message

    def __repr__(self):
        return "<TestFailureException '{}' with files: {}>".format(self.message, repr(self.files))


class TestLogger:
    def __init__(self):
        self.indentation = 0
        self.test_name = ''
        self.outputs = [sys.stdout]
        self.failed = False

    def subprocess_print(self, line: str):
        for o in self.outputs:
            o.write(line)
            o.flush()

    def rawprint(self, line: str, with_stdout=True):
        for o in self.outputs:
            if o == sys.stdout and not with_stdout:
                continue

            for l in line.split('\n'):
                if self.indentation > 0:
                    o.write(self.indentation*' ')
                o.write(l)
                o.write('\n')

            o.flush()

    def add_output(self, o, header='', footer=''):
        os.makedirs(os.path.dirname(o), exist_ok=True)
        self.outputs.append(open(o, "a"))

    def print(self, line: str, with_stdout=True):
        self.rawprint('.. ' + line, with_stdout)

    def comment(self, line: str):
        self.rawprint('// ' + line)

    def header(self, text):
        self.rawprint('\n## ' + text + ' ##\n')

    def indent(self):
        self.indentation += 4

    def dedent(self):
        self.indentation -= 4

    def begin_test(self, test_name: str, print_header: bool=True):
        self.test_name = test_name
        if print_header:
            self.rawprint(">> Test {}".format(test_name))
        self.indent()

        self.failed = False

    def end_test(self, test_name: str, print_footer: bool=True):
        if self.failed:
            self.rawprint("$$ FAILED")
        self.dedent()
        if print_footer:
            self.rawprint("<< Test {}".format(test_name))
        self.test_name = ''

    def success(self, message):
        self.rawprint("** " + message)

    def error(self, message):
        self.failed = True

        self.rawprint("!! " + message)

    def failure(self, ex):
        self.failed = True

        self.rawprint("!+ FAILURE in {}: {}".format(self.test_name, ex))

        self.rawprint('>> Callstack')
        tb = traceback.extract_tb(sys.exc_info()[2])
        for frame in reversed(tb):
            filename = util.sanitise_filename(frame.filename)
            filename = re.sub('.*site-packages/', 'site-packages/', filename)
            if filename[0] == '/':
                filename = filename[1:]
            self.rawprint("    File \"{}\", line {}, in {}".format(filename, frame.lineno, frame.name))
            self.rawprint("        {}".format(frame.line))
        self.rawprint('<< Callstack')

        if isinstance(ex, TestFailureException):
            file_list = []
            for f in ex.files:
                fname = '{}_{}'.format(self.test_name, os.path.basename(f))
                if 'data' in f:
                    ext = fname.rfind('.')
                    if ext > 0:
                        fname = fname[0:ext] + '_ref' + fname[ext:]
                if not os.path.exists(f):
                    continue
                shutil.copyfile(f, util.get_artifact_path(fname))
                file_list.append(fname)

            diff_file = ''
            diff = ''

            # Special handling for the common case where we have two files to generate comparisons
            if len(file_list) == 2:
                mime = mimetypes.guess_type(ex.files[0])

                if 'image' in mime[0]:
                    # If we have two files and they are images, a failed image comparison should have
                    # generated a diff.png. Grab it and include it
                    diff_tmp_file = util.get_tmp_path('diff.png')
                    if os.path.exists(diff_tmp_file):
                        diff_artifact = '{}_diff.png'.format(self.test_name)
                        shutil.move(diff_tmp_file, util.get_artifact_path(diff_artifact))
                        diff_file = ' ({})'.format(diff_artifact)

                elif 'text' in mime[0] or 'xml' in mime[0]:
                    with open(ex.files[0]) as f:
                        fromlines = f.readlines()
                    with open(ex.files[1]) as f:
                        tolines = f.readlines()
                    diff = difflib.unified_diff(fromlines, tolines, fromfile=file_list[0], tofile=file_list[1])

            if diff != '':
                self.rawprint("=+ Compare: " + ','.join(file_list) + diff_file)
                self.indent()
                self.rawprint(''.join(diff).strip())
                self.dedent()
                self.rawprint("=- Compare")
            elif len(file_list) > 0:
                self.rawprint("== Compare: " + ','.join(file_list) + diff_file)

        self.rawprint("!- FAILURE")


log = TestLogger()
