import renderdoc as rd
import rdtest

# Not a real test, re-used by API-specific tests
class Draw_Zoo(rdtest.TestCase):
    internal = True

    def vid(self, action: rd.ActionDescription, val: int):
        # For D3D, Vertex ID is either 0-based for non indexed calls, or the raw index. That means no baseVertex applied
        if rd.IsD3D(self.props.pipelineType):
            if action.flags & rd.ActionFlags.Indexed:
                return float(val - action.baseVertex)
            return float(val)

        # For GL and vulkan it includes all offsets - so for non-indexed that includes vertexOffset, and for indexed
        # that includes baseVertex
        if action.flags & rd.ActionFlags.Indexed:
            return float(val)
        return float(val + action.vertexOffset)

    def iid(self, action: rd.ActionDescription, val: int):
        # See above - similar for instance ID, but only vulkan includes the instanceOffset in instance ID

        if (self.props.pipelineType == rd.GraphicsAPI.Vulkan) and action.flags & rd.ActionFlags.Instanced:
            return float(val + action.instanceOffset)

        return float(val)

    def check_action(self, action: rd.ActionDescription, ref_data):
        rdtest.log.print("Checking action {}".format(action.eventId))

        self.controller.SetFrameEvent(action.eventId, True)

        self.pipe: rd.PipeState = self.controller.GetPipelineState()

        refl: rd.ShaderReflection = self.pipe.GetShaderReflection(rd.ShaderStage.Vertex)

        num_verts = len(ref_data['pos'])

        vsin_pos_name = 'pos'
        for sig in refl.inputSignature:
            sig: rd.SigParameter
            if 'pos' in sig.varName.lower() or 'pos' in sig.semanticName.lower():
                vsin_pos_name = sig.varName
                if vsin_pos_name == '':
                    vsin_pos_name = sig.semanticName
                break

        vsout_pos_name = 'pos'
        for sig in refl.outputSignature:
            sig: rd.SigParameter
            if sig.systemValue == rd.ShaderBuiltin.Position:
                vsout_pos_name = sig.varName
                if vsout_pos_name == '':
                    vsout_pos_name = sig.semanticName
                break

        out_tex = self.pipe.GetOutputTargets()[0].resource

        vsin_ref = {}
        restarts = []

        if 'restarts' in ref_data:
            restarts = ref_data['restarts']

        ib = self.pipe.GetIBuffer()

        striprestart_index = self.pipe.GetRestartIndex() & ((1 << (ib.byteStride*8)) - 1)

        for v in range(num_verts):
            if v in restarts:
                vsin_ref[v] = {
                    'vtx': v,
                    'idx': striprestart_index,
                }
            else:
                vsin_ref[v] = {
                    'vtx': v,
                    'idx': ref_data['base']+v,
                    vsin_pos_name: ref_data['pos'][v],
                }

        self.check_mesh_data(vsin_ref, self.get_vsin(action))

        num_instances = action.numInstances
        if not (action.flags & rd.ActionFlags.Instanced):
            num_instances = 1

        rdtest.log.success("Checked vertex in data")

        for inst in range(num_instances):
            conv_out_pos = lambda x: [x[0] + float(inst) * 0.5, x[1], x[2], 1.0]

            vsout_ref = {}

            for v in range(num_verts):
                if v in restarts:
                    vsout_ref[v] = {
                        'vtx': v,
                        'idx': striprestart_index,
                    }
                else:
                    vsout_ref[v] = {
                        'vtx': v,
                        'idx': ref_data['base']+v,
                        vsout_pos_name: conv_out_pos(ref_data['pos'][v]),
                        'VID': self.vid(action, ref_data['base']+v),
                        'IID': self.iid(action, inst),
                    }

            postvs = self.get_postvs(action, rd.MeshDataStage.VSOut, instance=inst)

            self.check_mesh_data(vsout_ref, postvs)

            rdtest.log.success("Checked vertex out data in instance {}".format(inst))

            if self.props.shaderDebugging and refl.debugInfo.debuggable:
                for vtx in range(num_verts):
                    if vtx in restarts:
                        continue

                    idx = vsout_ref[vtx]['idx']

                    self.check_debug(vtx, idx, inst, postvs)
            else:
                rdtest.log.print('Not checking shader debugging, unsupported')

            for vert, coord in enumerate(ref_data['pixels'][inst]):
                if coord[0] == 0 and coord[1] == 0:
                    continue
                val = (self.vid(action, ref_data['base'] + vert), self.iid(action, inst), float(inst) * 0.5,
                       postvs[vert]['COLOR'][1] + postvs[vert]['TEXCOORD'][0])
                self.check_pixel_value(out_tex, coord[0], coord[1], val, eps=0.3)

            rdtest.log.success("Checked pixels in instance {}".format(inst))

        rdtest.log.success("Checked action {}".format(action.eventId))

    def check_debug(self, vtx, idx, inst, postvs):
        trace: rd.ShaderDebugTrace = self.controller.DebugVertex(vtx, inst, idx, 0)

        if trace.debugger is None:
            self.controller.FreeTrace(trace)

            raise rdtest.TestFailureException("Couldn't debug vertex {} in instance {}".format(vtx, inst))

        cycles, variables = self.process_trace(trace)

        for var in trace.sourceVars:
            var: rd.SourceVariableMapping
            if var.variables[0].type == rd.DebugVariableType.Variable and var.signatureIndex >= 0:
                name = var.name

                if name not in postvs[vtx].keys():
                    raise rdtest.TestFailureException("Don't have expected output for {}".format(name))

                expect = postvs[vtx][name]
                value = self.evaluate_source_var(var, variables)

                if len(expect) != value.columns:
                    raise rdtest.TestFailureException(
                        "Output {} at vert {} (idx {}) instance {} has different size ({} values) to expectation ({} values)"
                            .format(name, vtx, idx, inst, value.columns, len(expect)))

                debugged = value.value.f32v[0:value.columns]

                if not rdtest.value_compare(expect, debugged):
                    raise rdtest.TestFailureException(
                        "Debugged value {} at vert {} (idx {}) instance {}: {} doesn't exactly match postvs output {}".format(
                            name, vtx, idx, inst, debugged, expect))
        rdtest.log.success('Successfully debugged vertex {} in instance {}'
                           .format(vtx, inst))

    def check_capture(self):
        self.props: rd.APIProperties = self.controller.GetAPIProperties()

        test_marker: rd.ActionDescription = self.find_action("Test")

        action: rd.ActionDescription = test_marker.next

        rdtest.log.begin_section("Non-indexed, non-instanced cases")

        # Basic case
        ref = {
            'base': 0,
            'pos': [[-0.5, 0.5, 0.0], [0.0, -0.5, 0.0], [0.5, 0.5, 0.0]],
            'pixels': [[(12, 12), (24, 34), (35, 12)]],
        }

        self.check_action(action, ref)
        self.check(action.vertexOffset == 0)
        self.check(self.pipe.GetVBuffers()[0].byteOffset == 0)
        action = action.next

        # Vertex offset in the action
        ref = {
            'base': 0,
            'pos': [[-0.5, -0.5, 0.0], [0.0, 0.5, 0.0], [0.5, -0.5, 0.0]],
            'pixels': [[(60, 35), (72, 13), (83, 35)]],
        }

        self.check_action(action, ref)
        self.check(action.vertexOffset > 0)
        self.check(self.pipe.GetVBuffers()[0].byteOffset == 0)
        action = action.next

        # Vertex offset in action and in vertex binding
        ref = {
            'base': 0,
            'pos': [[-0.5, 0.0, 0.0], [0.0, -0.5, 0.0], [0.0, 0.5, 0.0]],
            'pixels': [[(108, 23), (119, 35), (119, 13)]],
        }

        self.check_action(action, ref)
        self.check(action.vertexOffset > 0)
        self.check(self.pipe.GetVBuffers()[0].byteOffset > 0)
        action = action.next

        rdtest.log.end_section("Non-indexed, non-instanced cases")

        rdtest.log.begin_section("indexed, non-instanced")

        # Basic case
        ref = {
            'base': 0,
            'pos': [[-0.5, 0.5, 0.0], [0.0, -0.5, 0.0], [0.5, 0.5, 0.0]],
            'pixels': [[(12, 60), (24, 82), (35, 60)]],
        }

        self.check_action(action, ref)
        self.check(action.indexOffset == 0)
        self.check(action.baseVertex == 0)
        self.check(action.vertexOffset == 0)
        self.check(self.pipe.GetVBuffers()[0].byteOffset == 0)
        self.check(self.pipe.GetIBuffer().byteOffset == 0)
        action = action.next

        # first index in the action
        ref = {
            'base': 5,
            'pos': [[-0.5, -0.5, 0.0], [0.0, 0.5, 0.0], [0.5, -0.5, 0.0]],
            'pixels': [[(60, 83), (72, 61), (83, 83)]],
        }

        self.check_action(action, ref)
        self.check(action.indexOffset > 0)
        self.check(action.baseVertex == 0)
        self.check(action.vertexOffset == 0)
        self.check(self.pipe.GetVBuffers()[0].byteOffset == 0)
        self.check(self.pipe.GetIBuffer().byteOffset == 0)
        action = action.next

        # first index and base vertex in the action
        ref = {
            'base': 13,
            'pos': [[-0.5, 0.0, 0.0], [0.0, -0.5, 0.0], [0.0, 0.5, 0.0]],
            'pixels': [[(108, 71), (119, 83), (119, 61)]],
        }

        self.check_action(action, ref)
        self.check(action.indexOffset > 0)
        self.check(action.baseVertex < 0)
        self.check(action.vertexOffset == 0)
        self.check(self.pipe.GetVBuffers()[0].byteOffset == 0)
        self.check(self.pipe.GetIBuffer().byteOffset == 0)
        action = action.next

        # first index and base vertex in the action, and vertex binding offset
        ref = {
            'base': 3,
            'pos': [[-0.5, 0.0, 0.0], [0.0, -0.5, 0.0], [0.0, 0.5, 0.0]],
            'pixels': [[(156, 71), (167, 83), (167, 61)]],
        }

        self.check_action(action, ref)
        self.check(action.indexOffset > 0)
        self.check(action.baseVertex < 0)
        self.check(action.vertexOffset == 0)
        self.check(self.pipe.GetVBuffers()[0].byteOffset > 0)
        self.check(self.pipe.GetIBuffer().byteOffset == 0)
        action = action.next

        # first index and base vertex in the action, and vertex & index binding offset
        ref = {
            'base': 4,
            'pos': [[0.0, -0.5, 0.0], [0.5, 0.0, 0.0], [0.0, 0.5, 0.0]],
            'pixels': [[(216, 82), (226, 71), (216, 61)]],
        }

        self.check_action(action, ref)
        self.check(action.indexOffset > 0)
        self.check(action.baseVertex < 0)
        self.check(action.vertexOffset == 0)
        self.check(self.pipe.GetVBuffers()[0].byteOffset > 0)
        # OpenGL doesn't support offset on index buffer bindings
        if self.props.pipelineType != rd.GraphicsAPI.OpenGL:
            self.check(self.pipe.GetIBuffer().byteOffset > 0)
        action = action.next

        # Skip indexed strips for now
        ref = {
            'base': 30,
            'pos': [
                [-0.5, 0.2, 0.0], [-0.5, 0.0, 0.0],
                [-0.3, 0.2, 0.0], [-0.3, 0.0, 0.0],
                [-0.1, 0.2, 0.0],
                [],  # restart
                [0.1, 0.2, 0.0], [0.1, 0.0, 0.0],
                [0.3, 0.2, 0.0], [0.3, 0.0, 0.0],
                [0.5, 0.2, 0.0], [0.5, 0.0, 0.0],
            ],
            'restarts': [5],
            'pixels': [[(252, 67), (252, 71), (256, 67)]],
        }

        self.check_action(action, ref)
        action = action.next

        ref = {
            'base': 30,
            'pos': [
                [-0.5, 0.2, 0.0], [-0.5, 0.0, 0.0],
                [-0.3, 0.2, 0.0], [-0.3, 0.0, 0.0],
                [-0.1, 0.2, 0.0],
                [],  # restart
                [0.1, 0.2, 0.0], [0.1, 0.0, 0.0],
                [0.3, 0.2, 0.0], [0.3, 0.0, 0.0],
                [0.5, 0.2, 0.0], [0.5, 0.0, 0.0],
            ],
            'restarts': [5],
            'pixels': [[(300, 67), (300, 71), (304, 67)]],
        }

        self.check_action(action, ref)
        action = action.next

        rdtest.log.end_section("indexed, non-instanced")

        rdtest.log.begin_section("non-indexed, instanced")

        # Basic case
        ref = {
            'base': 0,
            'pos': [[-0.5, 0.5, 0.0], [0.0, -0.5, 0.0], [0.5, 0.5, 0.0]],
            'pixels': [
                [(12, 108), (24, 130), (0, 0)],
                [(24, 108), (36, 130), (47, 108)],
            ],
        }

        self.check_action(action, ref)
        self.check(action.instanceOffset == 0)
        self.check(self.pipe.GetVBuffers()[1].byteOffset == 0)
        action = action.next

        # instance offset in the action
        ref = {
            'base': 0,
            'pos': [[-0.5, -0.5, 0.0], [0.0, 0.5, 0.0], [0.5, -0.5, 0.0]],
            'pixels': [
                [(60, 131), (72, 109), (0, 0)],
                [(72, 131), (84, 109), (95, 131)],
            ],
        }

        self.check_action(action, ref)
        self.check(action.instanceOffset > 0)
        self.check(self.pipe.GetVBuffers()[1].byteOffset == 0)
        action = action.next

        # instance offset in the action and offset on the instanced VB
        ref = {
            'base': 0,
            'pos': [[-0.5, 0.0, 0.0], [0.0, -0.5, 0.0], [0.0, 0.5, 0.0]],
            'pixels': [
                [(108, 120), (119, 131), (119, 108)],
                [(120, 120), (131, 131), (131, 108)],
            ],
        }

        self.check_action(action, ref)
        self.check(action.instanceOffset > 0)
        self.check(self.pipe.GetVBuffers()[1].byteOffset > 0)
        action = action.next

        rdtest.log.end_section("non-indexed, instanced")

        rdtest.log.begin_section("indexed, instanced")

        # Basic case
        ref = {
            'base': 5,
            'pos': [[-0.5, -0.5, 0.0], [0.0, 0.5, 0.0], [0.5, -0.5, 0.0]],
            'pixels': [
                [(12, 179), (24, 157), (0, 0)],
                [(24, 179), (36, 157), (47, 179)],
            ],
        }

        self.check_action(action, ref)
        self.check(action.instanceOffset == 0)
        self.check(self.pipe.GetVBuffers()[1].byteOffset == 0)
        action = action.next

        # instance offset in the action
        ref = {
            'base': 13,
            'pos': [[-0.5, 0.0, 0.0], [0.0, -0.5, 0.0], [0.0, 0.5, 0.0]],
            'pixels': [
                [(60, 168), (71, 179), (71, 156)],
                [(72, 168), (83, 179), (83, 156)],
            ],
        }

        self.check_action(action, ref)
        self.check(action.instanceOffset > 0)
        self.check(self.pipe.GetVBuffers()[1].byteOffset == 0)
        action = action.next

        # instance offset in the action and offset on the instanced VB
        ref = {
            'base': 23,
            'pos': [[0.0, -0.5, 0.0], [0.5, 0.0, 0.0], [0.0, 0.5, 0.0]],
            'pixels': [
                [(120, 178), (130, 168), (120, 157)],
                [(132, 178), (142, 168), (132, 157)],
            ],
        }

        self.check_action(action, ref)
        self.check(action.instanceOffset > 0)
        self.check(self.pipe.GetVBuffers()[1].byteOffset > 0)
        action = action.next

        rdtest.log.end_section("indexed, instanced")
