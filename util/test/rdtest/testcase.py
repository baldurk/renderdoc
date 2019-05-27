import os
import traceback
import copy
import re
import renderdoc as rd
from . import util
from . import analyse
from . import capture
from .logging import log, TestFailureException


class ShaderVariableCheck:
    def __init__(self, var: rd.ShaderVariable, name: str):
        self.var = var

        if self.var.name != name:
            raise TestFailureException("Variable {} name mismatch, expected '{}' but got '{}'"
                                       .format(self.var.name, name, self.var.name))

    def rows(self, rows_: int):
        if self.var.rows != rows_:
            raise TestFailureException("Variable {} row count mismatch, expected {} but got {}"
                                       .format(self.var.name, rows_, self.var.rows))

        return self

    def cols(self, cols_: int):
        if self.var.columns != cols_:
            raise TestFailureException("Variable {} column count mismatch, expected {} but got {}"
                                       .format(self.var.name, cols_, self.var.columns))

        return self

    def type(self, type_: rd.VarType):
        if self.var.type != type_:
            raise TestFailureException("Variable {} type mismatch, expected {} but got {}"
                                       .format(self.var.name, str(type_), str(self.var.type)))

        return self

    def value(self, value_: list):
        count = len(value_)
        if isinstance(value_[0], float):
            if self.var.value.fv[0:count] != value_:
                raise TestFailureException("Float variable {} value mismatch, expected {} but got {}"
                                           .format(self.var.name, value_, self.var.value.fv[0:count]))
        else:
            # hack - check signed and unsigned values
            if self.var.value.iv[0:count] != value_ and self.var.value.uv[0:count] != value_:
                raise TestFailureException("Int variable {} value mismatch, expected {} but got {} / {}"
                                           .format(self.var.name, value_, self.var.value.iv[0:count],
                                                   self.var.value.uv[0:count]))

        return self

    def longvalue(self, value_: list):
        count = len(value_)
        if isinstance(value_[0], float):
            if self.var.value.dv[0:count] != value_:
                raise TestFailureException("Float variable {} value mismatch, expected {} but got {}"
                                           .format(self.var.name, value_, self.var.value.dv[0:count]))
        else:
            # hack - check signed and unsigned values
            if self.var.value.s64v[0:count] != value_ and self.var.value.u64v[0:count] != value_:
                raise TestFailureException("Int variable {} value mismatch, expected {} but got {} / {}"
                                           .format(self.var.name, value_, self.var.value.s64v[0:count],
                                                   self.var.value.u64v[0:count]))

        return self

    def row_major(self):
        if not self.var.rowMajor:
            raise TestFailureException("Variable {} is not row-major, as expected"
                                       .format(self.var.name))

        return self

    def column_major(self):
        if self.var.rowMajor:
            raise TestFailureException("Variable {} is not row-major, as expected"
                                       .format(self.var.name))

        return self

    def arraySize(self, elements_: int):
        if len(self.var.members) != elements_:
            raise TestFailureException("Variable {} array size mismatch, expected {} but got {}"
                                       .format(self.var.name, elements_, len(self.var.members)))

        return self

    def structSize(self, elements_: int):
        if not self.var.isStruct:
            raise TestFailureException("Variable {} is not a struct as was expected"
                                       .format(self.var.name))

        if len(self.var.members) != elements_:
            raise TestFailureException("Variable {} struct size mismatch, expected {} but got {}"
                                       .format(self.var.name, elements_, len(self.var.members)))

        return self

    def members(self, member_callbacks: dict):
        for i, m in enumerate(self.var.members):
            if i in member_callbacks:
                member_callbacks[i](ShaderVariableCheck(m, m.name))
            elif m.name in member_callbacks:
                member_callbacks[m.name](ShaderVariableCheck(m, m.name))
            else:
                raise TestFailureException("Unexpected member in {}: {}"
                                           .format(self.var.name, m.name))


class ConstantBufferChecker:
    def __init__(self, variables: list):
        self._variables = variables

    def check(self, name: str):
        if len(self._variables) == 0:
            raise TestFailureException("Too many variables checked, {} has no matching data".format(name))
        return ShaderVariableCheck(self._variables.pop(0), name)

    def done(self):
        if len(self._variables) != 0:
            raise TestFailureException("Not all variables checked, {} still remain".format(len(self._variables)))


class TestCase:
    slow_test = False
    demos_test_name = ''
    demos_frame_cap = 5
    _test_list = {}

    @staticmethod
    def set_test_list(tests):
        TestCase._test_list = tests

    def check_support(self):
        if self.demos_test_name != '':
            if self.demos_test_name not in TestCase._test_list:
                return False,'Test {} not in compiled tests'.format(self.demos_test_name)
            return TestCase._test_list[self.demos_test_name]

        # Otherwise assume we can run - child tests can override if they want to do some other check
        return True,""

    def __init__(self):
        self.capture_filename = ""
        self.controller: rd.ReplayController = None
        self._variables = []

    def get_ref_path(self, name: str, extra: bool = False):
        if extra:
            return util.get_data_extra_path(os.path.join(self.__class__.__name__, name))
        else:
            return util.get_data_path(os.path.join(self.__class__.__name__, name))

    def check(self, expr, msg=None):
        if not expr:
            callstack = traceback.extract_stack()
            callstack.pop()
            assertion_line = callstack[-1].line

            assert_msg = re.sub(r'[^(]*\((.*)?\)', r'\1', assertion_line)

            if msg is None:
                raise TestFailureException('Assertion Failure: {}'.format(assert_msg))
            else:
                raise TestFailureException('Assertion Failure: {}'.format(msg))

    def get_capture(self):
        """
        Method to overload if not implementing a run(), using the default run which
        handles everything and calls get_capture() and check_capture() for you.

        :return: The path to the capture to open. If in a temporary path, it will be
          deleted if the test completes.
        """

        if self.demos_test_name != '':
            return capture.run_and_capture(util.get_demos_binary(), self.demos_test_name, self.demos_frame_cap)

        raise NotImplementedError("If run() is not implemented in a test, then"
                                  "get_capture() and check_capture() must be.")

    def check_capture(self):
        """
        Method to overload if not implementing a run(), using the default run which
        handles everything and calls get_capture() and check_capture() for you.
        """
        raise NotImplementedError("If run() is not implemented in a test, then"
                                  "get_capture() and check_capture() must be.")

    def _find_draw(self, name: str, start_event: int, draw_list):
        draw: rd.DrawcallDescription
        for draw in draw_list:
            # If this draw matches, return it
            if draw.eventId >= start_event and name in draw.name:
                return draw

            # Recurse to children - depth-first search
            ret: rd.DrawcallDescription = self._find_draw(name, start_event, draw.children)

            # If we found our draw, return
            if ret is not None:
                return ret

            # Otherwise continue to next in the list

        # If we didn't find anything, return None
        return None

    def find_draw(self, name: str, start_event: int = 0):
        """
        Finds the first drawcall matching given criteria

        :param name: The name to search for within the drawcalls
        :param start_event: The first eventId to search from.
        :return:
        """

        return self._find_draw(name, start_event, self.controller.GetDrawcalls())

    def get_vsin(self, draw: rd.DrawcallDescription, first_index: int=0, num_indices: int=0, instance: int=0, view: int=0):
        ib: rd.BoundVBuffer = self.controller.GetPipelineState().GetIBuffer()

        if num_indices == 0:
            num_indices = draw.numIndices
        else:
            num_indices = min(num_indices, draw.numIndices)

        mesh = rd.MeshFormat()
        mesh.numIndices = num_indices
        mesh.indexByteOffset = ib.byteOffset + draw.indexOffset * draw.indexByteWidth
        mesh.indexByteStride = draw.indexByteWidth
        mesh.indexResourceId = ib.resourceId
        mesh.baseVertex = draw.baseVertex

        attrs = analyse.get_vsin_attrs(self.controller, mesh)

        first_index = min(first_index, draw.numIndices-1)

        indices = analyse.fetch_indices(self.controller, mesh, 0, first_index, num_indices)

        return analyse.decode_mesh_data(self.controller, indices, attrs, 0)

    def get_postvs(self, data_stage: rd.MeshDataStage, first_index: int=0, num_indices: int=0, instance: int=0, view: int=0):
        mesh: rd.MeshFormat = self.controller.GetPostVSData(instance, view, data_stage)

        if mesh.numIndices == 0:
            return []

        if num_indices == 0:
            num_indices = mesh.numIndices
        else:
            num_indices = min(num_indices, mesh.numIndices)

        first_index = min(first_index, mesh.numIndices-1)

        indices = analyse.fetch_indices(self.controller, mesh, 0, first_index, num_indices)

        attrs = analyse.get_postvs_attrs(self.controller, mesh, data_stage)

        return analyse.decode_mesh_data(self.controller, indices, attrs, 0)

    def check_mesh_data(self, mesh_ref, mesh_data):
        for idx in mesh_ref:
            ref = mesh_ref[idx]
            if idx >= len(mesh_data):
                raise TestFailureException('Mesh data doesn\'t have expected element {}'.format(idx))

            data = mesh_data[idx]

            for key in ref:
                if key not in data:
                    raise TestFailureException('Mesh data[{}] doesn\'t contain data {} as expected. Data is: {}'.format(idx, key, list(data.keys())))

                if not util.value_compare(ref[key], data[key]):
                    raise TestFailureException('Mesh data[{}] \'{}\': {} is not as expected: {}'.format(idx, key, data[key], ref[key]))

        log.success("Mesh data is identical to reference")

    def run(self):
        self.capture_filename = self.get_capture()

        self.check(os.path.exists(self.capture_filename), "Didn't generate capture in make_capture")

        log.print("Loading capture")

        self.controller = analyse.open_capture(self.capture_filename)

        log.print("Checking capture")

        self.check_capture()

        self.controller.Shutdown()

    def invoketest(self):
        self.run()

    def get_first_draw(self):
        first_draw: rd.DrawcallDescription = self.controller.GetDrawcalls()[0]

        while len(first_draw.children) > 0:
            first_draw = first_draw.children[0]

        return first_draw

    def get_texture(self, id: rd.ResourceId):
        texs = self.controller.GetTextures()

        for t in texs:
            t: rd.TextureDescription
            if t.resourceId == id:
                return t

        return None

    def get_resource(self, id: rd.ResourceId):
        resources = self.controller.GetResources()

        for r in resources:
            r: rd.ResourceDescription
            if r.resourceId == id:
                return r

        return None

    def get_last_draw(self):
        last_draw: rd.DrawcallDescription = self.controller.GetDrawcalls()[-1]

        while len(last_draw.children) > 0:
            last_draw = last_draw.children[-1]

        return last_draw

    def check_final_backbuffer(self):
        img_path = util.get_tmp_path('backbuffer.png')
        ref_path = self.get_ref_path('backbuffer.png')

        last_draw: rd.DrawcallDescription = self.get_last_draw()

        self.controller.SetFrameEvent(last_draw.eventId, True)

        save_data = rd.TextureSave()
        save_data.resourceId = last_draw.copyDestination
        save_data.destType = rd.FileType.PNG

        self.controller.SaveTexture(save_data, img_path)

        if not util.png_compare(img_path, ref_path):
            raise TestFailureException("Reference and output backbuffer image differ", ref_path, img_path)

        log.success("Backbuffer is identical to reference")

    def check_export(self, capture_filename):
        recomp_path = util.get_tmp_path('recompressed.rdc')
        conv_zipxml_path = util.get_tmp_path('conv.zip.xml')
        conv_path = util.get_tmp_path('conv.rdc')

        origrdc = rd.OpenCaptureFile()
        status = origrdc.OpenFile(capture_filename, '', None)

        self.check(status == rd.ReplayStatus.Succeeded, "Couldn't open '{}': {}".format(capture_filename, str(status)))

        # Export to rdc, to recompress
        origrdc.Convert(recomp_path, '', None, None)
        origrdc.Convert(conv_zipxml_path, 'zip.xml', None, None)

        origrdc.Shutdown()

        # Load up the zip.xml file
        zipxml = rd.OpenCaptureFile()
        status = zipxml.OpenFile(conv_zipxml_path, 'zip.xml', None)

        self.check(status == rd.ReplayStatus.Succeeded, "Couldn't open '{}': {}".format(conv_zipxml_path, str(status)))

        # Convert out to rdc
        zipxml.Convert(conv_path, '', None, None)

        zipxml.Shutdown()

        if not util.md5_compare(recomp_path, conv_path):
            raise TestFailureException("Recompressed capture file doesn't match re-imported capture file", conv_path, recomp_path, conv_zipxml_path)

        log.success("Recompressed and re-imported capture files are identical")
