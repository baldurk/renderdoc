import renderdoc as rd
import rdtest
from typing import List

def event_id(x): return x.eventId

class GL_PixelHistory_Formats(rdtest.TestCase):
    demos_test_name = 'GL_PixelHistory_Formats'
    demos_frame_cap = 10

    def check_capture(self):
        action = self.get_first_action()

        while True:
            glclear: rd.ActionDescription = self.find_action('glClear', action.eventId)
            action: rd.ActionDescription = self.find_action('glDraw', action.eventId+1)

            if action is None:
                break

            rdtest.log.success(f'Test {action.eventId} started.')
            self.controller.SetFrameEvent(action.eventId, True)

            pipe: rd.PipeState = self.controller.GetPipelineState()
            rt: rd.BoundResource = pipe.GetOutputTargets()[0]
            tex: rd.ResourceId = rt.resourceId
            sub = rd.Subresource()
            textures: List[rd.TextureDescription] = self.controller.GetTextures()
            def predicate(texDesc):
                return texDesc.resourceId == tex
            texDescription = next(filter(predicate, textures))

            if texDescription.format.Name() == 'B5G5R5A1_UNORM' or texDescription.format.Name() == 'B5G6R5_UNORM':
                eps = 1.0 / 32.0 
            elif texDescription.format.Name() == 'R10G10B10A2_UNORM':
                eps = 1.0 / 1024.0
            elif texDescription.format.Name() == 'R11G11B10_FLOAT':
                eps = 0.01
            elif texDescription.format.compByteWidth == 1:
                eps = 1.0 / 255.0
            else:
                eps = 2.0 / 16384.0

            

            x, y = 190, 149
            events = [glclear.eventId, action.eventId]
            modifs: List[rd.PixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.typeCast)
            self.check_events(events, modifs, False)
            self.check_pixel_value(tex, x, y, modifs[-1].postMod.col.floatValue, sub=sub, cast=rt.typeCast, eps=eps)
            self.check_shader_out_with_postmod(modifs[-1].shaderOut.col.floatValue, modifs[-1].postMod.col.floatValue, texDescription.format.compCount, eps)

            x, y = 328, 199
            events = [glclear.eventId]
            modifs: List[rd.pixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.typeCast)
            self.check_events(events, modifs, False)
            self.check_pixel_value(tex, x, y, modifs[-1].postMod.col.floatValue, sub=sub, cast=rt.typeCast, eps=eps)

            rdtest.log.success('Test {} completed.'.format(action.eventId))


    def check_events(self, events, modifs, hasSecondary):
        self.check(len(modifs) == len(events), "Expected {} events, got {}, modifs {}".format(len(events), len(modifs), modifs))

        for i in range(len(modifs)):
            self.check(modifs[i].eventId == events[i], f"Expected event with id {events[i]}, but got {modifs[i].eventId}")



    def check_shader_out_with_postmod(self, shaderOut, postMod, compCount: int, eps: float):
        print(f"shaderOut: {shaderOut}, posMod: {postMod}")

        diff = 0

        for i in range(compCount):
            if not rdtest.value_compare(shaderOut[i], postMod[i], eps=eps):
                raise rdtest.TestFailureException(f"shaderOut and postMod differ by {shaderOut[i] - postMod[i]}")
