import renderdoc as rd
import rdtest

class VK_Multi_View(rdtest.TestCase):
    demos_test_name = 'VK_Multi_View'

    def check_capture(self):
        x = 200
        y = 150

        for test_name in ["Vertex: viewIndex", "Geometry: viewIndex", "Fragment: viewIndex", "No viewIndex"]:
            rdtest.log.print(f"Test {test_name}")
            label = self.find_action(test_name)
            if label is None:
                continue
            action = label.nextAction
            assert action is not None
            self.set_event(action.eventId, True)

            pipe = self.controller.GetPipelineState()

            for view in range(2):
                # Debug the pixel shader
                inputs = rd.DebugPixelInputs()
                inputs.view = view
                trace = self.controller.DebugPixel(x, y, inputs)

                cycles, variables = self.process_trace(trace)
                output = self.find_output_source_var(trace, rd.ShaderBuiltin.ColorOutput, 0)
                assert output is not None
                debugged = self.evaluate_source_var(output, variables)
                slice = view + 1
                sub = rd.Subresource(0, slice, 0)
                self.check_pixel_value(pipe.GetOutputTargets()[0].resource, x, y, debugged.value.f32v[0:4], sub=sub)
                self.controller.FreeTrace(trace)

                inst = 0
                postvs = self.get_postvs(action, rd.MeshDataStage.VSOut, instance=inst, view=view)
                for vtx in range(action.numIndices):
                    idx = vtx
                    self.check_vertex_debug(vtx, idx, inst, postvs, view=view)
                rdtest.log.print(f"View {view} Slice {slice} passed")

        for test_name in ["viewportIndex choice"]:
            rdtest.log.print(f"Test {test_name}")
            label = self.find_action(test_name)
            if label is None:
                continue
            action = label.nextAction
            assert action is not None
            self.set_event(action.eventId, True)

            pipe = self.controller.GetPipelineState()

            for view in range(2):
                if view == 0:
                    x, y = 100, 140
                else:
                    x, y = 300, 140

                # Debug the pixel shader
                inputs = rd.DebugPixelInputs()
                inputs.view = view
                trace = self.controller.DebugPixel(x, y, inputs)

                cycles, variables = self.process_trace(trace)
                output = self.find_output_source_var(trace, rd.ShaderBuiltin.ColorOutput, 0)
                assert output is not None
                debugged = self.evaluate_source_var(output, variables)
                slice = view + 1
                sub = rd.Subresource(0, slice, 0)
                self.check_pixel_value(pipe.GetOutputTargets()[0].resource, x, y, debugged.value.f32v[0:4], sub=sub)
                self.controller.FreeTrace(trace)

                inst = 0
                postvs = self.get_postvs(action, rd.MeshDataStage.VSOut, instance=inst, view=view)
                for vtx in range(action.numIndices):
                    idx = vtx
                    self.check_vertex_debug(vtx, idx, inst, postvs, view=view)
                rdtest.log.print(f"View {view} Slice {slice} passed")

        rdtest.log.success("All tests matched")


