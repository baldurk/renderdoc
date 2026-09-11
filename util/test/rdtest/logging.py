from __future__ import annotations
import os
import sys
import re
import traceback
import mimetypes
import threading
import difflib
import shutil
from typing import IO, Any, List, Type
from . import util


class TestFailureException(Exception):
    def __init__(self, message: str, *args: str):
        self.message = message
        self.files: List[str] = []
        for a in args:
            self.files.append(a)

    def __str__(self):
        return self.message

    def __repr__(self):
        return f"<TestFailureException '{self.message}' with files: {repr(self.files)}>"


class TestLogger:
    def __init__(self):
        self.indentation = 0
        self.test_name = ''
        self.outputs: List[IO[str]] = [sys.stdout]
        self.failed = False
        self.section_failed = False
        self.logged_exception = False
        self.mutex = threading.Lock()

    def subprocess_test(self, test: str, thread: int, out_buf: List[str], path: str, exc: Exception | None = None):
        with self.mutex:
            self.begin_test(test, True, thread)
            with open(path) as f:
                lines = f.readlines()
                for l in lines:
                    self.rawprint(l, with_stdout=False)
            sys.stdout.write(out_buf[0])
            sys.stdout.flush()
            sys.stderr.write(out_buf[1])
            sys.stderr.flush()
            if exc is not None:
                self.failure(exc)
            self.end_test(test, True, thread)

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

    def add_output(self, o: str | IO[str], header='', footer=''):
        if isinstance(o, str):
            os.makedirs(os.path.dirname(o), exist_ok=True)
            self.outputs.append(open(o, "a"))
        else:
            self.outputs.append(o)

    def print(self, line: str, with_stdout=True):
        self.rawprint('.. ' + line, with_stdout)

    def comment(self, line: str):
        self.rawprint('// ' + line)

    def header(self, text: str):
        self.rawprint('\n## ' + text + ' ##\n')

    def indent(self):
        self.indentation += 4

    def dedent(self):
        self.indentation -= 4

    def begin_test(self, test_name: str, print_header: bool=True, thread=-1):
        self.test_name = test_name
        if print_header:
            if thread >= 0:
                self.rawprint(f">> Test {test_name} (Worker {thread})")
            else:
                self.rawprint(f">> Test {test_name}")
        self.indent()

        self.failed = False
        self.logged_exception = False

    def end_test(self, test_name: str, print_footer: bool=True, thread=-1):
        if self.failed:
            self.rawprint("$$ FAILED")
        self.dedent()
        if print_footer:
            if thread >= 0:
                self.rawprint(f"<< Test {test_name} (Worker {thread})")
            else:
                self.rawprint(f"<< Test {test_name}")
        self.test_name = ''

    def begin_section(self, name: str):
        self.rawprint(f">> Section {name}")
        self.indent()
        self.section_failed = False
        self.logged_exception = False

    def end_section(self, name: str):
        if self.section_failed:
            self.rawprint("$$ FAILED")
        self.dedent()
        self.rawprint(f"<< Section {name}")

    def auto_section(self, name: str):
        class ScopedSection():
            def __init__(self, logger: TestLogger, name: str):
                self.name = name
                self.logger = logger

            def __enter__(self):
                self.logger.begin_section(name)

            def __exit__(self, exc_type: Type[Exception] | None, exc_value: Exception | None, traceback: Any):
                if exc_value is not None:
                    self.logger.failure(exc_value)
                self.logger.end_section(name)
            
        return ScopedSection(self, name)

    def inline_file(self, name: str, path: str, with_stdout: bool = False):
        self.rawprint(f">> Raw {name}")
        self.indent()
        with open(path) as f:
            lines = f.readlines()
            for l in lines:
                self.rawprint(l.strip(), with_stdout=with_stdout)
        self.dedent()
        self.rawprint(f"<< Raw {name}")

    def success(self, message: str):
        self.rawprint("** " + message)

    def error(self, message: str):
        self.failed = self.section_failed = True

        self.rawprint("!! " + message)

    def failure(self, ex: Exception):
        if self.logged_exception:
            return

        self.logged_exception = True
        self.failed = self.section_failed = True

        tb = traceback.extract_tb(sys.exc_info()[2])

        if isinstance(ex, AssertionError):
            assertion_line = tb[-1].line
            
            if assertion_line is not None:
                assert_msg = re.sub(r'assert (.*)', r'\1', assertion_line)
            else:
                assert_msg = "Unknown Assertion"

            self.rawprint(f"!+ ASSERT FAILURE in {self.test_name}: {assert_msg}")
        elif isinstance(ex, TestFailureException):
            self.rawprint(f"!+ FAILURE in {self.test_name}: {ex!s}")
        else:
            self.rawprint(f"!+ FAILURE in {self.test_name}: {type(ex).__name__} {ex!s}")

        self.rawprint('>> Callstack')
        for frame in reversed(tb):
            filename = util.sanitise_filename(frame.filename)
            filename = re.sub('.*site-packages/', 'site-packages/', filename)
            if filename[0] == '/':
                filename = filename[1:]
            self.rawprint(f"    File \"{filename}\", line {frame.lineno}, in {frame.name}")
            self.rawprint(f"        {frame.line}")
        self.rawprint('<< Callstack')

        if isinstance(ex, TestFailureException):
            file_list: List[str] = []
            for f in ex.files:
                fname = f'{self.test_name}_{os.path.basename(f)}'
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

                if mime[0] is None:
                    pass
                elif 'image' in mime[0]:
                    # If we have two files and they are images, a failed image comparison should have
                    # generated a diff.png. Grab it and include it
                    diff_tmp_file = util.get_tmp_path('diff.png')
                    if os.path.exists(diff_tmp_file):
                        diff_artifact = f'{self.test_name}_diff.png'
                        shutil.move(diff_tmp_file, util.get_artifact_path(diff_artifact))
                        diff_file = f' ({diff_artifact})'

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
