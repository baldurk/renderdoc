import rdtest
import renderdoc as rd


class D3D12_Vertex_UAV(rdtest.TestCase):
    demos_test_name = 'D3D12_Vertex_UAV'

    def check_capture(self):
        out: rd.ReplayOutput = self.controller.CreateOutput(rd.CreateHeadlessWindowingData(100, 100), rd.ReplayOutputType.Texture)

        quad_seen = []

        for pass_name in ["Normal", "Collide"]:
            for base_event_name in ["5_0", "5_1", "6_0"]:
                name = pass_name + "_" + base_event_name
                marker = self.find_action(name)

                if marker is None:
                    continue

                rdtest.log.print("Checking quad overdraw on {}".format(name))

                self.controller.SetFrameEvent(marker.next.eventId, True)

                pipe: rd.PipeState = self.controller.GetPipelineState()

                tex = rd.TextureDisplay()
                tex.resourceId = pipe.GetOutputTargets()[0].resourceId

                tex.overlay = rd.DebugOverlay.QuadOverdrawPass
                out.SetTextureDisplay(tex)

                out.Display()

                overlay_id: rd.ResourceId = out.GetDebugOverlayTexID()

                picked = self.controller.PickPixel(overlay_id, 5, 5, rd.Subresource(0,0,0), rd.CompType.Float).floatValue

                if any([p != picked[0] for p in picked]):
                    raise rdtest.TestFailureException("Quad overdraw isn't correct: {}".format(picked))

                quad_seen.append(picked[0])

                rdtest.log.success("Quad overdraw is good on {}".format(name))

                if not pipe.GetShaderReflection(rd.ShaderStage.Pixel).debugInfo.debuggable:
                    rdtest.log.print("Skipping undebuggable shader.")
                    continue

                # Debug the shader
                trace = self.controller.DebugPixel(50, 50, rd.DebugPixelInputs())
                if trace.debugger is None:
                    raise rdtest.TestFailureException("Pixel shader at {} could not be debugged.".format(name))
                    self.controller.FreeTrace(trace)

                cycles, variables = self.process_trace(trace)

                output = self.find_output_source_var(trace, rd.ShaderBuiltin.ColorOutput, 0)

                debugged = self.evaluate_source_var(output, variables)

                self.controller.FreeTrace(trace)

                if not rdtest.value_compare(debugged.value.f32v[0:4], [1.0, 1.0, 0.0, 1.0]):
                    raise rdtest.TestFailureException("Pixel shader at {} did not debug correctly.".format(name))

                rdtest.log.success("Shader debugging at {} was successful".format(name))

        quad_seen = sorted(quad_seen)
        if quad_seen != [float(a) for a in range(1, len(quad_seen) + 1)]:
            raise rdtest.TestFailureException("Quad overdraw values are inconsistent: {}".format(quad_seen))

        out.Shutdown()
