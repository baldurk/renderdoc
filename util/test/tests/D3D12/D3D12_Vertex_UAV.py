from typing import List

import rdtest
import renderdoc as rd


class D3D12_Vertex_UAV(rdtest.TestCase):
    demos_test_name = 'D3D12_Vertex_UAV'

    def check_capture(self):
        out = self.controller.CreateOutput(rd.CreateHeadlessWindowingData(100, 100), rd.ReplayOutputType.Texture)

        quad_seen: List[float] = []

        for pass_name in ["Normal", "Collide"]:
            for base_event_name in ["5_0", "5_1", "6_0"]:
                name = pass_name + "_" + base_event_name
                marker = self.find_action(name)

                if marker is None:
                    continue

                rdtest.log.print(f"Checking quad overdraw on {name}")

                self.set_event(marker.nextAction.eventId, True)

                pipe = self.controller.GetPipelineState()

                tex = rd.TextureDisplay()
                tex.resourceId = pipe.GetOutputTargets()[0].resource

                tex.overlay = rd.DebugOverlay.QuadOverdrawPass
                out.SetTextureDisplay(tex)

                out.Display()

                overlay_id = out.GetDebugOverlayTexID()

                picked = self.controller.PickPixel(overlay_id, 5, 5, rd.Subresource(0,0,0), rd.CompType.Float).floatValue

                if any([p != picked[0] for p in picked]):
                    raise rdtest.TestFailureException(f"Quad overdraw isn't correct: {picked}")

                quad_seen.append(picked[0])

                rdtest.log.success(f"Quad overdraw is good on {name}")

                # Debug the shader
                with self.debug_pixel(50, 50, rd.DebugPixelInputs()) as debug:
                    cycles, variables = self.process_trace(debug.trace)

                    output = self.find_output_source_var(debug.trace, rd.ShaderBuiltin.ColorOutput, 0)

                    debugged = self.evaluate_source_var(output, variables)

                    if not rdtest.value_compare(debugged.value.f32v[0:4], [1.0, 1.0, 0.0, 1.0]):
                        raise rdtest.TestFailureException(f"Pixel shader at {name} did not debug correctly.")

                    rdtest.log.success(f"Shader debugging at {name} was successful")

        quad_seen = sorted(quad_seen)
        if quad_seen != [float(a) for a in range(1, len(quad_seen) + 1)]:
            raise rdtest.TestFailureException(f"Quad overdraw values are inconsistent: {quad_seen}")

        out.Shutdown()
