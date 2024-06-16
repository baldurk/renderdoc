import renderdoc as rd
import rdtest
from typing import List


class D3D11_Pixel_History_Zoo(rdtest.TestCase):
    slow_test = True
    demos_test_name = 'D3D11_Pixel_History_Zoo'

    def check_capture(self):
        actions = self.controller.GetRootActions()

        for d in self.controller.GetRootActions():
            # Only process test actions
            if not d.customName.startswith('Test'):
                continue

            # Go to the last child action
            self.controller.SetFrameEvent(d.children[-1].eventId, True)

            if any(['UInt tex' in d.customName for d in d.children]):
                value_selector = lambda x: x.uintValue
                shader_out = (0, 1, 1234, 5)
            elif any(['SInt tex' in d.customName for d in d.children]):
                value_selector = lambda x: x.intValue
                shader_out = (0, 1, -1234, 5)
            else:
                value_selector = lambda x: x.floatValue
                shader_out = (0.0, 1.0, 0.1234, 0.5)

            pipe: rd.PipeState = self.controller.GetPipelineState()

            rt = pipe.GetOutputTargets()[0]

            vp: rd.Viewport = pipe.GetViewport(0)

            tex = rt.resource
            x, y = (int(vp.width / 2), int(vp.height / 2))

            tex_details = self.get_texture(tex)

            sub = rd.Subresource()
            if tex_details.arraysize > 1:
                sub.slice = rt.firstSlice
            if tex_details.mips > 1:
                sub.mip = rt.firstMip

            modifs: List[rd.PixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.format.compType)

            # Should be at least two modifications in every test - clear and action
            self.check(len(modifs) >= 2)

            self.check_modifiations(modifs, value_selector, shader_out)
            rdtest.log.success("shader output and premod/postmod is consistent")

            # The current pixel value should match the last postMod
            self.check_pixel_value(tex, x, y, value_selector(modifs[-1].postMod.col), sub=sub, cast=rt.format.compType)

            # Also the red channel should be zero, as it indicates errors
            self.check(float(value_selector(modifs[-1].postMod.col)[0]) == 0.0)

        self.check_uavwrite()

    def check_uavwrite(self):
        uavWrite = self.find_action("UAVWrite")
        if uavWrite is None:
            rdtest.log.print("UAVWrite Test not found")
            return

        self.controller.SetFrameEvent(uavWrite.next.eventId, True)
        pipe: rd.PipeState = self.controller.GetPipelineState()
        uav = pipe.GetReadWriteResources(rd.ShaderStage.Pixel)[0].descriptor
        vp: rd.Viewport = pipe.GetViewport(0)
        tex = uav.resource
        x, y = 4, 4
        sub = rd.Subresource()
        value_selector = lambda x: x.floatValue
        shader_out = (0.3451, 0.43922, 0.0, 1.0)

        modifs: List[rd.PixelModification] = self.controller.PixelHistory(tex, x, y, sub, uav.format.compType)
        # Should be two modifications - clear and draw
        self.check(len(modifs) == 2)
        self.check_modifiations(modifs, value_selector, shader_out)
        rdtest.log.success("UAVWrite shader output and premod/postmod is consistent")
        # The current pixel value should match the last postMod
        self.check_pixel_value(tex, x, y, value_selector(modifs[-1].postMod.col), sub=sub, cast=uav.format.compType)

    def check_modifiations(self, modifs: List[rd.PixelModification], value_selector, shader_out):
        # Check that the modifications are self consistent - postmod of each should match premod of the next
        for i in range(len(modifs) - 1):
            if value_selector(modifs[i].postMod.col) != value_selector(modifs[i + 1].preMod.col):
                raise rdtest.TestFailureException(
                    "postmod at {}: {} doesn't match premod at {}: {}".format(modifs[i].eventId,
                                                                value_selector(modifs[i].postMod.col),
                                                                modifs[i + 1].eventId,
                                                                value_selector(modifs[i].preMod.col)))

            # A fragment event : postMod.stencil should be unknown
            if modifs[i].eventId == modifs[i+1].eventId:
                if modifs[i].postMod.stencil != -1 and modifs[i].postMod.stencil != -2:
                    raise rdtest.TestFailureException(
                    "postmod stencil at {} primitive {}: {} is not unknown".format(modifs[i].eventId,
                                                                                modifs[i].primitiveID,
                                                                                modifs[i].postMod.stencil))

            if self.get_action(modifs[i].eventId).flags & rd.ActionFlags.Drawcall:
                if not rdtest.value_compare(value_selector(modifs[i].shaderOut.col), shader_out):
                    raise rdtest.TestFailureException(
                        "Shader output {} isn't as expected {}".format(value_selector(modifs[i].shaderOut.col),
                                                                           shader_out))