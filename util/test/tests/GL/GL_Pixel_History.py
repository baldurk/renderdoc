import renderdoc as rd
import rdtest
from typing import List

class GL_Pixel_History(rdtest.TestCase):
    slow_test = True
    demos_test_name = 'GL_Pixel_History'
    demos_frame_cap = 10

    def check_capture(self):
        action = self.get_first_action()

        while True:
            glclear: rd.ActionDescription = self.find_action('glClear', action.eventId)
            action: rd.ActionDescription = self.find_action('glDraw', action.eventId+1)

            if action is None:
                break

            rdtest.log.success(f'Testing Event {action.eventId} started.')
            self.controller.SetFrameEvent(action.eventId, True)

            pipe: rd.PipeState = self.controller.GetPipelineState()
            rt = pipe.GetOutputTargets()[0]
            tex: rd.ResourceId = rt.resource
            sub = rd.Subresource()

            texDescription : rd.TextureDescription = self.get_texture(tex)
            rdtest.log.print("Testing format {}".format(texDescription.format.Name()))

            if texDescription.format.type == rd.ResourceFormatType.R5G5B5A1 or texDescription.format.type == rd.ResourceFormatType.R5G6B5:
                eps = 1.0 / 32.0 
            elif texDescription.format.type == rd.ResourceFormatType.R10G10B10A2:
                eps = 1.0 / 1024.0
            elif texDescription.format.type == rd.ResourceFormatType.R11G11B10:
                eps = 0.01
            elif texDescription.format.compByteWidth == 1:
                eps = 1.0 / 255.0
            else:
                eps = 2.0 / 16384.0

            x, y = 190, 149
            rdtest.log.print("Testing pixel {}, {}".format(x, y))
            events = [glclear.eventId, action.eventId]
            modifs: List[rd.PixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.format.compType)
            self.check_events(events, modifs, False)
            self.check_pixel_value(tex, x, y, modifs[-1].postMod.col.floatValue, sub=sub, cast=rt.format.compType, eps=eps)
            self.check_shader_out_with_postmod(modifs[-1].shaderOut.col.floatValue, modifs[-1].postMod.col.floatValue, texDescription.format.compCount, eps)

            x, y = 328, 199
            rdtest.log.print("Testing pixel {}, {}".format(x, y))
            events = [glclear.eventId]
            modifs: List[rd.pixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.format.compType)
            self.check_events(events, modifs, False)
            self.check_pixel_value(tex, x, y, modifs[-1].postMod.col.floatValue, sub=sub, cast=rt.format.compType, eps=eps)

            rdtest.log.success('Testing Event {} completed.'.format(action.eventId))

    def check_events(self, events, modifs, hasSecondary):
        eventMatchingModifs = modifs[(-1 * len(events)):]

        # modifications can show results from previous colour passes which we don't care about for now, so we only check the last relevant modifs
        for i in range(len(eventMatchingModifs)):
            self.check(eventMatchingModifs[i].eventId == events[i], f"Expected event with id {events[i]}, but got {eventMatchingModifs[i].eventId}")

    def check_shader_out_with_postmod(self, shaderOut, postMod, compCount: int, eps: float):
        for i in range(compCount):
            if not rdtest.value_compare(shaderOut[i], postMod[i], eps=eps):
                raise rdtest.TestFailureException(f"shaderOut and postMod differ by {shaderOut[i] - postMod[i]}")
