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
            logfile = os.path.join(util.get_tmp_dir(), 'demos.log')
            timeout = self.demos_timeout
            if timeout is None:
                timeout = util.get_demos_timeout()
            return capture.run_and_capture(util.get_demos_binary(), self.demos_test_name + " --log " + logfile,
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
        for action in action_list:
            # If this action matches, return it
            if action.eventId >= start_event and (name == '' or name in self.action_name(action)):
                return action

            # Recurse to children - depth-first search
            ret: rd.ActionDescription = self._find_action(name, start_event, action.children)

            # If we found our action, return
            if ret is not None:
                return ret

            # Otherwise continue to next in the list

        # If we didn't find anything, return None
        return None

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

        self.check(os.path.exists(self.capture_filename), "Didn't generate capture in make_capture")

        log.print("Loading capture")

        self.controller = analyse.open_capture(self.capture_filename, opts=self.get_replay_options())
        self.sdfile = self.controller.GetStructuredFile()

        log.print("Checking capture")

        self.check_capture()

        if self.controller is not None:
            self.controller.Shutdown()

    def invoketest(self, debugMode):
        start_time = self.get_time()
        self.run()
        duration = self.get_time() - start_time
        log.print("Test ran in {}".format(duration))
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

    def process_trace(self, trace: rd.ShaderDebugTrace):
        variables = {}
        cycles = 0
        while True:
            states = self.controller.ContinueDebug(trace.debugger)
            if len(states) == 0:
                break

            for state in states:
                for change in state.changes:
                    variables[change.after.name] = change.after

            cycles = states[-1].stepIndex

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

        raise KeyError("Couldn't parse path {}".format(path))


    def evaluate_source_var(self, sourceVar: rd.SourceVariableMapping, debugVars):
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

    def check_export(self, capture_filename):
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
