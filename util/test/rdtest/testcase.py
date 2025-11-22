import os
import traceback
import copy
import re
import datetime
import renderdoc as rd
from . import util
from . import analyse
from . import capture
from .logging import log, TestFailureException
from typing import List


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
            vals = []
            if self.var.type == rd.VarType.Float:
                vals = list(self.var.value.f32v[0:count])
            elif self.var.type == rd.VarType.Double:
                vals = list(self.var.value.f64v[0:count])
            elif self.var.type == rd.VarType.Half:
                vals = list(self.var.value.f16v[0:count])

            if vals != list(value_):
                raise TestFailureException("Float variable {} value mismatch, expected {} but got {}"
                                           .format(self.var.name, value_, self.var.value.f32v[0:count]))
        else:
            vals = []
            if self.var.type == rd.VarType.UInt or self.var.type == rd.VarType.Bool:
                vals = list(self.var.value.u32v[0:count])
            elif self.var.type == rd.VarType.ULong:
                vals = list(self.var.value.u64v[0:count])
            elif self.var.type == rd.VarType.UShort:
                vals = list(self.var.value.u16v[0:count])
            elif self.var.type == rd.VarType.UByte:
                vals = list(self.var.value.u8v[0:count])
            elif self.var.type == rd.VarType.SInt:
                vals = list(self.var.value.s32v[0:count])
            elif self.var.type == rd.VarType.SLong:
                vals = list(self.var.value.s64v[0:count])
            elif self.var.type == rd.VarType.SShort:
                vals = list(self.var.value.s16v[0:count])
            elif self.var.type == rd.VarType.SByte:
                vals = list(self.var.value.s8v[0:count])

            if vals != list(value_):
                raise TestFailureException("Int variable {} value mismatch, expected {} but got {}"
                                           .format(self.var.name, value_, vals))

        return self

    def longvalue(self, value_: list):
        count = len(value_)
        if isinstance(value_[0], float):
            if list(self.var.value.f64v[0:count]) != list(value_):
                raise TestFailureException("Float variable {} value mismatch, expected {} but got {}"
                                           .format(self.var.name, value_, self.var.value.f64v[0:count]))
        else:
            # hack - check signed and unsigned values
            if list(self.var.value.s64v[0:count]) != list(value_) and list(self.var.value.u64v[0:count]) != list(value_):
                raise TestFailureException("Int variable {} value mismatch, expected {} but got {} / {}"
                                           .format(self.var.name, value_, self.var.value.s64v[0:count],
                                                   self.var.value.u64v[0:count]))

        return self

    def row_major(self):
        if not self.var.RowMajor():
            raise TestFailureException("Variable {} is not row-major, as expected"
                                       .format(self.var.name))

        return self

    def column_major(self):
        if not self.var.ColMajor():
            raise TestFailureException("Variable {} is not column-major, as expected"
                                       .format(self.var.name))

        return self

    def arraySize(self, elements_: int):
        if len(self.var.members) != elements_:
            raise TestFailureException("Variable {} array size mismatch, expected {} but got {}"
                                       .format(self.var.name, elements_, len(self.var.members)))

        return self

    def structSize(self, elements_: int):
        if not self.var.type == rd.VarType.Struct:
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

    def next_var(self):
        return self._variables[0]

    def done(self):
        if len(self._variables) != 0:
            raise TestFailureException("Not all variables checked, {} still remain".format(len(self._variables)))


class TestCase:
    slow_test = False
    internal = False
    demos_test_name = ''
    demos_frame_cap = 5
    demos_frame_count = 1
    demos_timeout = None
    demos_captures_expected = None
    _test_list = {}

    @staticmethod
    def set_test_list(tests):
        TestCase._test_list = tests

    def check_support(self, **kwargs):
        if self.demos_test_name != '':
            if self.demos_test_name not in TestCase._test_list:
                return False,'Test {} not in compiled tests'.format(self.demos_test_name)
            return TestCase._test_list[self.demos_test_name]

        # Otherwise assume we can run - child tests can override if they want to do some other check
        return True,""

    def __init__(self):
        self.capture_filename = ""
        self.controller: rd.ReplayController = None
        self.sdfile: rd.SDFile = None
        self._variables = []

    def get_time(self):
        return datetime.datetime.now(datetime.timezone.utc)

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

    def check_eq(self, a, b):
        self.check(a == b, '{} != {}'.format(a, b))

    def get_replay_options(self):
        """
        Method to overload if you want to override the replay options used.

        :return: The renderdoc.ReplayOptions to use.
        """

        return rd.ReplayOptions()

    def get_capture_options(self):
        """
        Method to overload if you want to override the capture options used.

        :return: The renderdoc.CaptureOptions to use.
        """

        return rd.CaptureOptions()

    def get_capture(self):
        """
        Method to overload if not implementing a run(), using the default run which
        handles everything and calls get_capture() and check_capture() for you.

        :return: The path to the capture to open. If in a temporary path, it will be
          deleted if the test completes.
        """

        if self.demos_test_name != '':
            logfile = os.path.join(util.get_tmp_dir(), util.get_current_test(), 'demos.log')
            remote_logfile = logfile
            exe = util.get_demos_binary()
            if util.get_remote_server() is not None:
                remote_logfile = util.get_remote_server().get_temp_path('demos.log')
                exe = util.get_remote_server().get_demos_exe()

            timeout = self.demos_timeout
            if timeout is None:
                timeout = util.get_demos_timeout()
            return capture.run_and_capture(exe,
                                           self.demos_test_name + " --log " + remote_logfile,
                                           self.demos_frame_cap, frame_count=self.demos_frame_count,
                                           captures_expected=self.demos_captures_expected, logfile=logfile,
                                           opts=self.get_capture_options(), timeout=timeout)

        raise NotImplementedError("If run() is not implemented in a test, then"
                                  "get_capture() and check_capture() must be.")

    def check_capture(self):
        """
        Method to overload if not implementing a run(), using the default run which
        handles everything and calls get_capture() and check_capture() for you.
        """
        raise NotImplementedError("If run() is not implemented in a test, then"
                                  "get_capture() and check_capture() must be.")

    def action_name(self, action: rd.ActionDescription):
        if len(action.customName) > 0:
            return action.customName

        return self.sdfile.chunks[action.events[-1].chunkIndex].name

    def _find_action(self, name: str, start_event: int, action_list):
        action: rd.ActionDescription
        bestMatch = None
        distance = 1000000
        for action in action_list:
            # If this action matches, return it
            if action.eventId >= start_event and (name == '' or name in self.action_name(action)):
                if action.eventId - start_event < distance:
                    bestMatch = action
                    distance = action.eventId - start_event

            # Recurse to children - depth-first search
            ret: rd.ActionDescription = self._find_action(name, start_event, action.children)

            # If we found our action, return
            if ret is not None:
                if ret.eventId - start_event < distance:
                    bestMatch = ret
                    distance = ret.eventId - start_event

            # Otherwise continue to next in the list

        # If we didn't find anything, return None
        return bestMatch

    def find_action(self, name: str, start_event: int = 0):
        """
        Finds the first action matching given criteria

        :param name: The name to search for within the actions
        :param start_event: The first eventId to search from.
        :return:
        """

        return self._find_action(name, start_event, self.controller.GetRootActions())

    def get_action(self, event: int = 0):
        """
        Finds the action for the given event

        :param event: The eventId to search for.
        :return:
        """

        return self._find_action('', event, self.controller.GetRootActions())

    def get_vsin(self, action: rd.ActionDescription, first_index: int=0, num_indices: int=0, instance: int=0, view: int=0):
        ib: rd.BoundVBuffer = self.controller.GetPipelineState().GetIBuffer()

        if num_indices == 0:
            num_indices = action.numIndices
        else:
            num_indices = min(num_indices, action.numIndices)

        ioffs = action.indexOffset * ib.byteStride

        mesh = rd.MeshFormat()
        mesh.numIndices = num_indices
        mesh.indexByteOffset = ib.byteOffset + ioffs
        mesh.indexByteStride = ib.byteStride
        mesh.indexResourceId = ib.resourceId
        mesh.baseVertex = action.baseVertex

        if ib.byteSize > ioffs:
            mesh.indexByteSize = ib.byteSize - ioffs
        else:
            mesh.indexByteSize = 0

        if not (action.flags & rd.ActionFlags.Indexed):
            mesh.indexByteOffset = 0
            mesh.indexByteStride = 0
            mesh.indexResourceId = rd.ResourceId.Null()

        attrs = analyse.get_vsin_attrs(self.controller, action.vertexOffset, mesh)

        first_index = min(first_index, action.numIndices-1)

        indices = analyse.fetch_indices(self.controller, action, mesh, 0, first_index, num_indices)

        return analyse.decode_mesh_data(self.controller, indices, indices, attrs, 0, 0)

    def get_postvs(self, action: rd.ActionDescription, data_stage: rd.MeshDataStage, first_index: int = 0,
                   num_indices: int = 0, instance: int = 0, view: int = 0):
        mesh: rd.MeshFormat = self.controller.GetPostVSData(instance, view, data_stage)

        if mesh.numIndices == 0:
            return []

        if num_indices == 0:
            num_indices = mesh.numIndices
        else:
            num_indices = min(num_indices, mesh.numIndices)

        first_index = min(first_index, mesh.numIndices-1)

        ib: rd.BoundVBuffer = self.controller.GetPipelineState().GetIBuffer()

        ioffs = action.indexOffset * ib.byteStride

        in_mesh = rd.MeshFormat()
        in_mesh.numIndices = num_indices
        in_mesh.indexByteOffset = ib.byteOffset + ioffs
        in_mesh.indexByteStride = ib.byteStride
        in_mesh.indexResourceId = ib.resourceId
        in_mesh.baseVertex = action.baseVertex

        if ib.byteSize > ioffs:
            in_mesh.indexByteSize = ib.byteSize - ioffs
        else:
            in_mesh.indexByteSize = 0

        if not (action.flags & rd.ActionFlags.Indexed):
            in_mesh.indexByteOffset = 0
            in_mesh.indexByteStride = 0
            in_mesh.indexResourceId = rd.ResourceId.Null()

        indices = analyse.fetch_indices(self.controller, action, mesh, 0, first_index, num_indices)
        in_indices = analyse.fetch_indices(self.controller, action, in_mesh, 0, first_index, num_indices)

        attrs = analyse.get_postvs_attrs(self.controller, mesh, data_stage)

        return analyse.decode_mesh_data(self.controller, indices, in_indices, attrs, 0, mesh.baseVertex)

    def parse_shader_var_type(self, varType):
        scalarType = varType
        countElems = 1
        if str(varType[-1]).isdigit():
            if str(varType[-2]).isdigit():
                scalarType = varType[:-2]
                countElems = int(varType[-2:])
            else:
                scalarType = varType[:-1]
                countElems = int(varType[-1:])
        return (scalarType, countElems)
        
    def get_source_shader_var_value(self, sourceVars: List[rd.SourceVariableMapping], name, varType, debuggerVars):
        sourceVar = [v for v in sourceVars if v.name == name]
        if len(sourceVar) != 1:
            raise TestFailureException(f"Couldn't find source variable {name} type:{varType}")

        scalarType, countElems = self.parse_shader_var_type(varType)

        debugged = self.evaluate_source_var(sourceVar[0], debuggerVars)
        if scalarType == 'float':
            return list(debugged.value.f32v[0:countElems])
        elif scalarType == 'int':
            return list(debugged.value.s32v[0:countElems])
        else:
            raise TestFailureException(f"Unhandled scalarType {scalarType} type:{varType}")
        return None

    def check_task_data(self, task_ref, task_data):
        for idx in task_ref:
            ref = task_ref[idx]
            if idx >= len(task_data):
                raise TestFailureException('Task data doesn\'t have expected element {}'.format(idx))

            data = task_data[idx]

            for key in ref:
                if key not in data:
                    raise TestFailureException('Task data[{}] doesn\'t contain data {} as expected. Data is: {}'.format(idx, key, list(data.keys())))

                if not util.value_compare(ref[key], data[key]):
                    raise TestFailureException('Task data[{}] \'{}\': {} is not as expected: {}'.format(idx, key, data[key], ref[key]))

        log.success("Task data is identical to reference")

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

    def check_pixel_value(self, tex: rd.ResourceId, x, y, value, *, sub=None, cast=None, eps=util.FLT_EPSILON):
        tex_details = self.get_texture(tex)
        res_details = self.get_resource(tex)

        if sub is None:
            sub = rd.Subresource(0,0,0)
        if cast is None:
            cast = rd.CompType.Typeless

        if tex_details is not None:
            if type(x) is float:
                x = int(((tex_details.width >> sub.mip) - 1) * x)
            if type(y) is float:
                y = int(((tex_details.height >> sub.mip) - 1) * y)

            if cast == rd.CompType.Typeless and tex_details.creationFlags & rd.TextureCategory.SwapBuffer:
                cast = rd.CompType.UNormSRGB

            # Reduce epsilon for RGBA8 textures if it's not already reduced
            if tex_details.format.compByteWidth == 1 and eps == util.FLT_EPSILON:
                eps = (1.0 / 255.0)
            if tex_details.format.compByteWidth == 2 and eps == util.FLT_EPSILON:
                eps = (1.0 / 16384.0)

        picked: rd.PixelValue = self.controller.PickPixel(tex, x, y, sub, cast)

        picked_value = picked.floatValue
        if cast == rd.CompType.UInt:
            picked_value = picked.uintValue
        elif cast == rd.CompType.SInt:
            picked_value = picked.intValue

        if not util.value_compare(picked_value, value, eps):
            save_data = rd.TextureSave()
            save_data.resourceId = tex
            save_data.destType = rd.FileType.PNG
            save_data.slice.sliceIndex = sub.slice
            save_data.mip = sub.mip
            save_data.sample.sampleIndex = sub.sample

            img_path = util.get_tmp_path('output.png')

            self.controller.SaveTexture(save_data, img_path)

            raise TestFailureException(
                "Picked value {} at {},{} doesn't match expectation of {}".format(picked_value, x, y, value),
                img_path)

        name = "Texture"
        if res_details is not None:
            name = res_details.name

        log.success("Picked value at {},{} in {} is as expected".format(x, y, name))

    def check_triangle(self, out = None, back = None, fore = None, vp = None):
        pipe: rd.PipeState = self.controller.GetPipelineState()

        # if no output is specified, check the current colour output at this action
        if out is None:
            out = pipe.GetOutputTargets()[0].resource

        tex_details = self.get_texture(out)

        # if no colours are specified, default to green on our dark grey
        if back is None:
            back = [0.2, 0.2, 0.2, 1.0]
        if fore is None:
            fore = [0.0, 1.0, 0.0, 1.0]
        if vp is None:
            vp = (0.0, 0.0, float(tex_details.width), float(tex_details.height))

        self.check_pixel_value(out, int(0.5*vp[2]+vp[0]), int(0.5*vp[3]+vp[1]), fore)
        self.check_pixel_value(out, int(0.5*vp[2]+vp[0]), int(0.3*vp[3]+vp[1]), fore)
        self.check_pixel_value(out, int(0.3*vp[2]+vp[0]), int(0.7*vp[3]+vp[1]), fore)
        self.check_pixel_value(out, int(0.7*vp[2]+vp[0]), int(0.7*vp[3]+vp[1]), fore)

        self.check_pixel_value(out, int(0.3*vp[2]+vp[0]), int(0.5*vp[3]+vp[1]), back)
        self.check_pixel_value(out, int(0.7*vp[2]+vp[0]), int(0.5*vp[3]+vp[1]), back)
        self.check_pixel_value(out, int(0.5*vp[2]+vp[0]), int(0.8*vp[3]+vp[1]), back)
        self.check_pixel_value(out, int(0.5*vp[2]+vp[0]), int(0.2*vp[3]+vp[1]), back)

        log.success("Simple triangle is as expected")

    def run(self):
        self.capture_filename = self.get_capture()

        self.check(util.target_path_exists(self.capture_filename), "Didn't generate capture in make_capture")

        log.print("Loading capture")

        self.controller = analyse.open_capture(self.capture_filename, opts=self.get_replay_options())
        self.sdfile = self.controller.GetStructuredFile()

        log.print("Checking capture")

        self.check_capture()

        if self.controller is not None:
            if not util.get_remote_server() is None:
                util.get_remote_server().CloseCapture(self.controller)
            else:
                self.controller.Shutdown()

    def invoketest(self, debugMode):
        start_time = self.get_time()
        self.run()
        duration = self.get_time() - start_time
        log.print("Test {} ran in {}".format(self.demos_test_name, duration))
        self.debugMode = debugMode

    def get_first_action(self):
        first_action: rd.ActionDescription = self.controller.GetRootActions()[0]

        while len(first_action.children) > 0:
            first_action = first_action.children[0]

        return first_action

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

    def get_resource_by_name(self, name: str):
        resources = self.controller.GetResources()

        for r in resources:
            r: rd.ResourceDescription
            if r.name == name:
                return r

        return None

    def get_last_action(self):
        last_action: rd.ActionDescription = self.controller.GetRootActions()[-1]

        while len(last_action.children) > 0:
            last_action = last_action.children[-1]

        return last_action

    def check_final_backbuffer(self):
        img_path = util.get_tmp_path('backbuffer.png')
        ref_path = self.get_ref_path('backbuffer.png')

        last_action: rd.ActionDescription = self.get_last_action()

        self.controller.SetFrameEvent(last_action.eventId, True)

        save_data = rd.TextureSave()
        save_data.resourceId = last_action.copyDestination
        save_data.destType = rd.FileType.PNG

        self.controller.SaveTexture(save_data, img_path)

        if not util.png_compare(img_path, ref_path):
            raise TestFailureException("Reference and output backbuffer image differ", ref_path, img_path)

        log.success("Backbuffer is identical to reference")

    def log_shader_variable(self, var: rd.ShaderVariable) -> None:
        log.print(f"Shader Variable: {var.name} Type:{var.type} Rows:{var.rows} Columns:{var.columns} Flags:{var.flags} CountMembers:{len(var.members)}")

        for i in range(var.rows * var.columns):
            type = var.type
            if type == rd.VarType.UByte or type == rd.VarType.SByte:
                log.print(f"Byte   {i}: {var.value.u8v[i]}")
            elif type == rd.VarType.Half or type == rd.VarType.UShort or type == rd.VarType.SShort:
                log.print(f"Half   {i}: {var.value.u16v[i]}")
            elif type == rd.VarType.Float:
                log.print(f"Float  {i}: {var.value.f32v[i]}")
            elif type == rd.VarType.UInt or type == rd.VarType.SInt or type == rd.VarType.Bool or type == rd.VarType.Enum:
                log.print(f"Int    {i}: {var.value.u32v[i]}")
            elif type == rd.VarType.Double:
                log.print(f"Double {i}: {var.value.f64v[i]}")
            elif type == rd.VarType.ULong or type == rd.VarType.SLong or type == rd.VarType.GPUPointer:
                log.print(f"Long   {i}: {var.value.u64v[i]}")
            else:
                log.print(f"???    {i}: {var.value.u64v[i]}")

        for m in range(len(var.members)):
            self.log_shader_variable(var.members[m])

    def compare_shader_variable_change(self, expectedChange: rd.ShaderVariableChange, change: rd.ShaderVariableChange, showDiffs = True) -> bool:
        ret = True
        difference = ""
        (res, difference) = analyse.shadervariable_equal(expectedChange.before, change.before)
        if not res:
            if not showDiffs:
                return False
            log.error(f"ShaderVariableChange different before {expectedChange.before.name} {change.before.name} {difference}")
            ret = False
        (res, difference) = analyse.shadervariable_equal(expectedChange.after, change.after)
        if not res:
            if not showDiffs:
                return False
            log.error(f"ShaderVariableChange different after {expectedChange.after.name} {change.after.name} {difference}")
            ret = False
        return ret

    def compare_shader_variable_changes(self, expectedChanges: List[rd.ShaderVariableChange], changes: List[rd.ShaderVariableChange], showDiffs = True) -> bool:
        ret = True
        if (len(expectedChanges) != len(changes)):
            if not showDiffs:
                return False
            log.error(f"Different number of changes:{len(expectedChanges)} != {len(changes)}")
            return False
        for i in range(len(expectedChanges)):
            expected = expectedChanges[i]
            change = changes[i]
            if not self.compare_shader_variable_change(expected, change, showDiffs):
                if not showDiffs:
                    return False
                log.error(f"ShaderVariableChange[{i}] does not match")
                ret = False
        return ret

    def compare_single_step(self, expectedState: rd.ShaderDebugState, state: rd.ShaderDebugState, showDiffs = True) -> bool:
        ret = True
        if expectedState.stepIndex != state.stepIndex:
            if not showDiffs:
                return False
            log.error(f"Different stepIndex: {expectedState.stepIndex} != {state.stepIndex}")
            ret = False
        if expectedState.flags != state.flags:
            if not showDiffs:
                return False
            log.error(f"Different flags: {expectedState.flags} != {state.flags}")
            ret = False
        if expectedState.nextInstruction != state.nextInstruction:
            if not showDiffs:
                return False
            log.error(f"Different nextInstruction: {expectedState.nextInstruction} != {state.nextInstruction}")
            ret = False
        if not self.compare_shader_variable_changes(expectedState.changes, state.changes, showDiffs):
            if not showDiffs:
                return False
            log.error(f"Different changes at nextInstruction:{expectedState.nextInstruction} stepIndex:{expectedState.stepIndex}")
            ret = False
        if len(expectedState.callstack) != len(state.callstack):
            if not showDiffs:
                return False
            log.error(f"Different callstack length: {len(expectedState.callstack)} != {len(state.callstack)}")
            return False
        for i in range(len(expectedState.callstack)):
            if expectedState.callstack[i] != state.callstack[i]:
                if not showDiffs:
                    return False
                log.error(f"Different callstack entry[{i}]: {expectedState.callstack[i]} != {state.callstack[i]}")
                ret = False

        return ret

    def compare_full_traces(self, expectedStates: List[rd.ShaderDebugState], states: List[rd.ShaderDebugState], showDiffs = True) -> bool:
        ret = True
        if len(expectedStates) != len(states):
            if not showDiffs:
                return False
            log.error(f"Traces have different number of states: {len(expectedStates)} != {len(states)}")
            return False
        for i in range(len(expectedStates)):
            if not self.compare_single_step(expectedStates[i], states[i], showDiffs):
                if not showDiffs:
                    return False
                log.error(f"Trace state[{i}] does not match")
                ret = False
        return ret

    def generate_full_trace(self, trace: rd.ShaderDebugTrace) -> List[rd.ShaderDebugState]:
        allStates = []
        allChanges = []
        while True:
            states = self.controller.ContinueDebug(trace.debugger)
            if len(states) == 0:
                break
            for state in states:
                allStates.append(state)
                allChanges.append(state.changes)
        self.validate_trace(allChanges)
        return allStates

    def process_trace(self, trace: rd.ShaderDebugTrace, validate: bool = True):
        variables = {}
        cycles = 0
        allChanges = []
                
        while True:
            states = self.controller.ContinueDebug(trace.debugger)
            if len(states) == 0:
                break

            for state in states:
                if validate:
                    allChanges.append(state.changes)
                for change in state.changes:
                    variables[change.after.name] = change.after

            cycles = states[-1].stepIndex

        if validate:
            self.validate_trace(allChanges)

        return cycles, variables

    def get_sig_index(self, signature, builtin: rd.ShaderBuiltin, reg_index: int = -1):
        search = (builtin, reg_index)
        signature_mapped = [(sig.systemValue, sig.regIndex) for sig in signature]

        if reg_index == -1:
            search = builtin
            signature_mapped = [x[0] for x in signature_mapped]

        if search in signature_mapped:
            return signature_mapped.index(search)
        return -1

    def find_source_var(self, sourceVars, signatureIndex, varType):
        vars = [x for x in sourceVars if x.signatureIndex == signatureIndex and x.variables[0].type == varType]

        if len(vars) == 0:
            return None

        return vars[0]

    def find_input_source_var(self, trace: rd.ShaderDebugTrace, builtin: rd.ShaderBuiltin, reg_index: int = -1):
        refl: rd.ShaderReflection = self.controller.GetPipelineState().GetShaderReflection(trace.stage)

        sig_index = self.get_sig_index(refl.inputSignature, builtin, reg_index)

        return self.find_source_var(trace.sourceVars, sig_index, rd.DebugVariableType.Input)

    def find_output_source_var(self, trace: rd.ShaderDebugTrace, builtin: rd.ShaderBuiltin, reg_index: int = -1):
        refl: rd.ShaderReflection = self.controller.GetPipelineState().GetShaderReflection(trace.stage)

        sig_index = self.get_sig_index(refl.outputSignature, builtin, reg_index)

        return self.find_source_var(trace.sourceVars, sig_index, rd.DebugVariableType.Variable)

    def get_debug_var(self, debugVars, path: str):
        # first look for exact match
        for name, var in debugVars.items():
            if name == path:
                return var

        child = ''
        remaining = ''

        # Otherwise, take off any child if we haven't started recursing
        m = re.match("([a-zA-Z0-9_]+)(\[.*|\..*)", path)
        if m:
            child = m.group(1)
            remaining = m.group(2)
        else:
            # array index
            m = re.match("(\[[0-9]*\])(.*)", path)
            if m:
                child = m.group(1)
                remaining = m.group(2)
            else:
                m = re.match("\.([a-zA-Z0-9_]+)(.*)", path)
                if m:
                    child = m.group(1)
                    remaining = m.group(2)

        if child != '':
            for name, var in debugVars.items():
                var: rd.ShaderVariable
                if name == child:
                    if remaining == '':
                        return var
                    else:
                        return self.get_debug_var({mem.name: mem for mem in var.members}, remaining)

            raise KeyError("Couldn't find {} in debug vars".format(path))

        raise KeyError(f"Couldn't find '{path}' in debug vars or parse it")


    def evaluate_source_var(self, sourceVar: rd.SourceVariableMapping, debugVars) -> rd.ShaderVariable:
        debugged = rd.ShaderVariable()
        debugged.name = sourceVar.name
        debugged.type = sourceVar.type
        debugged.rows = sourceVar.rows
        debugged.columns = sourceVar.columns
        f32v = [0.0] * 16
        for i, debugVarPath in enumerate(sourceVar.variables):
            debugVar = self.get_debug_var(debugVars, debugVarPath.name)
            debugged.flags = debugVar.flags
            f32v[i] = debugVar.value.f32v[debugVarPath.component]
        debugged.value.f32v = f32v
        return debugged

    def combine_source_vars(self, vars):
        NOT_FOUND = 100000

        processed = []

        # Keep looping until we're done
        while len(vars) > 0:
            # find the first member that contains a . or [ character in its name
            base = ''
            bare_array = False
            first_var = len(vars)
            for i,v in enumerate(vars):
                idx = NOT_FOUND
                if '.' in v.name:
                    idx = v.name.index('.')
                if '[' in v.name:
                    idx2 = v.name.index('[')
                    if idx2 < idx:
                        if idx == NOT_FOUND:
                            bare_array = True
                        idx = idx2
                    if idx2 == 0:
                        idx = v.name.index(']')+1

                if idx == NOT_FOUND:
                    processed.append(v)
                else:
                    first_var = i
                    base = v.name[:idx]
                    break

            del vars[0:first_var]

            # If no vars are found, we're done
            if base == '':
                continue

            members = []

            combined = rd.ShaderVariable()
            combined.name = base

            last_var = -1
            for i in range(len(vars)):
                check = vars[i].name[:len(base)+1]
                if check == base + '.' or check == base + '[':
                    last_var = i
                    v = vars[i]
                    v.name = v.name[len(base):]
                    if v.name[0] == '.':
                        v.name = v.name[1:]
                        combined.type = rd.VarType.Struct
                    if check == base + '.':
                        combined.type = rd.VarType.Struct
                    members.append(vars[i])

            if not bare_array:
                members = self.combine_source_vars(members)
            combined.members = members

            del vars[0:last_var+1]
            processed.append(combined)

            # Continue and combine the next set of vars (there could be multiple structs/arrays on the same level,
            # and we only combined the first set)

        return processed

    def retrieve_capture(self):
        if util.get_remote_server() is None:
            return self.capture_filename

        dest = util.get_tmp_path(self.capture_filename.split('/')[-1])
        log.print("Copying remote capture from '{}' to '{}'".format(self.capture_filename, dest))
        util.get_remote_server().CopyCaptureFromRemote(self.capture_filename, dest, None)
        return dest

    def check_export(self, capture_filename):
        capture_filename = self.retrieve_capture()

        recomp_path = util.get_tmp_path('recompressed.rdc')
        conv_zipxml_path = util.get_tmp_path('conv.zip.xml')
        conv_path = util.get_tmp_path('conv.rdc')

        origrdc = rd.OpenCaptureFile()
        result = origrdc.OpenFile(capture_filename, '', None)

        self.check(result == rd.ResultCode.Succeeded, "Couldn't open '{}': {}".format(capture_filename, str(result)))

        # Export to rdc, to recompress
        origrdc.Convert(recomp_path, '', None, None)
        origrdc.Convert(conv_zipxml_path, 'zip.xml', None, None)

        origrdc.Shutdown()

        # Load up the zip.xml file
        zipxml = rd.OpenCaptureFile()
        result = zipxml.OpenFile(conv_zipxml_path, 'zip.xml', None)

        self.check(result == rd.ResultCode.Succeeded, "Couldn't open '{}': {}".format(conv_zipxml_path, str(result)))

        # Convert out to rdc
        zipxml.Convert(conv_path, '', None, None)

        zipxml.Shutdown()

        if not util.md5_compare(recomp_path, conv_path):
            raise TestFailureException("Recompressed capture file doesn't match re-imported capture file", conv_path, recomp_path, conv_zipxml_path)

        log.success("Recompressed and re-imported capture files are identical")

    def check_debug_pixel(self, x: int, y: int):
        pipe: rd.PipeState = self.controller.GetPipelineState()
        if not pipe.GetShaderReflection(rd.ShaderStage.Pixel).debugInfo.debuggable:
            log.print("Skipping undebuggable shader.")
            return 

        # Debug the shader
        trace = self.controller.DebugPixel(x, y, rd.DebugPixelInputs())
        if trace.debugger is None:
            self.controller.FreeTrace(trace)
            raise TestFailureException(f"Pixel shader could not be debugged at {x},{y}.")

        _, variables = self.process_trace(trace)
        output = self.find_output_source_var(trace, rd.ShaderBuiltin.ColorOutput, 0)
        debugged = self.evaluate_source_var(output, variables)
        self.controller.FreeTrace(trace)

        try:
            self.check_pixel_value(pipe.GetOutputTargets()[0].resource, x, y, debugged.value.f32v[0:4])
        except TestFailureException as ex:
            raise TestFailureException(f"Pixel shader did not debug correctly at {x},{y}. {ex}")

        log.success(f"Pixel shader debugging at {x},{y} was successful")

    def decode_task_data(self, controller: rd.ReplayController, mesh: rd.MeshFormat, payload: rd.ConstantBlock, task: int = 0):

        begin = mesh.vertexByteOffset + mesh.vertexByteStride * task
        end = min(begin + mesh.vertexByteSize, 0xffffffffffffffff)
        buffer_data = controller.GetBufferData(mesh.vertexResourceId, begin, end -begin)

        ret = []
        offset = 0
        for var in payload.variables:
            var_data = {}
            var_data[var.name] = []
            # This is not complete to decode all possible payload layouts
            for i in range(var.type.elements):
                format = rd.ResourceFormat()
                format.compByteWidth = rd.VarTypeByteSize(var.type.baseType)
                format.compCount = var.type.columns
                format.compType = rd.VarTypeCompType(var.type.baseType)
                format.type = rd.ResourceFormatType.Regular

                data =  analyse.unpack_data(format, buffer_data, offset)
                var_data[var.name] += data
                offset += format.compByteWidth * format.compCount
            ret.append(var_data)

        return ret

    def get_task_data(self, action: rd.ActionDescription):
        mesh: rd.MeshFormat = self.controller.GetPostVSData(0, 0, rd.MeshDataStage.TaskOut)
        if mesh.numIndices == 0:
            raise TestFailureException("Task data is empty")

        if len(mesh.taskSizes) == 0:
            raise TestFailureException("Task data is empty")

        pipe: rd.PipeState = self.controller.GetPipelineState()
        shader = pipe.GetShaderReflection(rd.ShaderStage.Task)
        taskIdx = 0
        task = action.dispatchDimension
        data = []
        for x in range(task[0]):
            for y in range(task[1]):
                for z in range(task[2]):
                    data += self.decode_task_data(self.controller, mesh, shader.taskPayload, taskIdx)
                    taskIdx += 1
        return data

    def check_renderdoc_log_asserts(self):
        countAsserts = 0
        rdlog = rd.GetLogFile()
        with open(rdlog, 'r') as f:
            for line in f:
                if 'Assertion' in line:
                    log.error(line)
                    countAsserts += 1
        if countAsserts > 0:
            raise TestFailureException(f'Renderdoc log file contains {countAsserts} Asserts')

    def validate_shadervariable(self, var: rd.ShaderVariable):
        if len(var.members) != 0:
            if var.type != rd.VarType.Struct and var.type != rd.VarType.Unknown and var.type != rd.VarType.ConstantBlock:
                log.error(f"ShaderVariable {var.name} has members with invalid type {var.type}")
                return False
            if var.rows != 0:
                log.error(f"ShaderVariable {var.name} has members with invalid rows {var.rows}")
                return False
            if var.columns != 0:
                log.error(f"ShaderVariable {var.name} has members with invalid columns {var.columns}")
                return False

            for m in var.members:
                if not self.validate_shadervariable(m):
                    return False
            return True

        if var.type == rd.VarType.Struct:
            log.error(f"ShaderVariable {var.name} has invalid type {var.type}")
            return False

        if var.rows * var.columns == 0:
            log.error(f"ShaderVariable {var.name} has invalid rows * columns {var.rows} * {var.columns}")
            return False

        if var.rows * var.columns > 16:
            log.error(f"ShaderVariable {var.name} has invalid rows * columns {var.rows} * {var.columns}")
            return False

        return True

    def validate_trace(self, allChanges):
        # Step Forwards
        variables = {}
        for i in range(len(allChanges)):
            for c in allChanges[i]:
                if len(c.after.name) == 0:
                    if variables.get(c.before.name) is None:
                        raise TestFailureException(f"Step {i} ShaderVariableChange for '{c.before.name}' not found in existing variables")
                    else:
                        del variables[c.before.name]
                    # Validate c.before
                    if not self.validate_shadervariable(c.before):
                        raise TestFailureException(f"Step {i} ShaderVariableChange for '{c.before.name}' before is not well formed")

                else:
                    if c.after.name in variables:
                        # Step Forwards: not-first appearance of a variable "before" must equal currently known value
                        (res, difference) = analyse.shadervariable_equal(c.before, variables[c.after.name])
                        if not res:
                            raise TestFailureException(f"Step {i} ShaderVariableChange for '{c.after.name}' before does not match existing entry {difference}")
                    else:
                        # Step Forwards: first appearance of a variable must have "before" = {}
                        if c.before != rd.ShaderVariable():
                            raise TestFailureException(f"Step {i} ShaderVariableChange for '{c.after.name}' does not have NULL before")
                    variables[c.after.name] = c.after
                    # Validate c.after
                    if not self.validate_shadervariable(c.after):
                        raise TestFailureException(f"Step {i} ShaderVariableChange for '{c.after.name}' after is not well formed")

        # Step Backwards
        for i in reversed(range(len(allChanges))):
            for c in allChanges[i]:
                if len(c.before.name) == 0:
                    if variables.get(c.after.name) is None:
                        raise TestFailureException(f"Step {i} ShaderVariableChange for '{c.after.name}' not found in existing variables")
                    else:
                        del variables[c.after.name]
                    # Validate c.after
                    if not self.validate_shadervariable(c.after):
                        raise TestFailureException(f"Step {i} ShaderVariableChange for '{c.after.name}' after is not well formed")

                else:
                    if c.before.name in variables:
                        # Step Backwards: not-first appearance of a variable "after" must equal currently known value
                        (res, difference) = analyse.shadervariable_equal(c.after, variables[c.before.name])
                        if not res:
                            raise TestFailureException(f"Step {i} ShaderVariableChange for '{c.before.name}' after does not match existing entry {difference}")
                    else:
                        # Step Backwards: first appearance of a variable must have "after" = {}
                        if c.after != rd.ShaderVariable():
                            raise TestFailureException(f"Step {i} ShaderVariableChange for '{c.before.name}' does not have NULL after")
                    variables[c.before.name] = c.before
                    # Validate c.before
                    if not self.validate_shadervariable(c.before):
                        raise TestFailureException(f"Step {i} ShaderVariableChange for '{c.after.name}' before is not well formed")

        return True
