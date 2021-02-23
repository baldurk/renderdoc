import rdtest
import renderdoc as rd


class GL_Draw_Zoo(rdtest.Draw_Zoo):
    demos_test_name = 'GL_Draw_Zoo'
    internal = False

    def check_capture(self):
        rdtest.Draw_Zoo.check_capture(self)

        draw = self.find_draw("GL_PRIMITIVE_RESTART")

        self.check(draw is not None)

        draw = draw.next

        self.controller.SetFrameEvent(draw.eventId, True)

        pipe = self.controller.GetPipelineState()

        ib = pipe.GetIBuffer()


        self.check(pipe.IsStripRestartEnabled())
        self.check((pipe.GetStripRestartIndex() & ((1 << (ib.byteStride*8)) - 1)) == 0xffff)

        draw = self.find_draw("GL_PRIMITIVE_RESTART_FIXED_INDEX")

        self.check(draw is not None)

        draw = draw.next

        self.controller.SetFrameEvent(draw.eventId, True)

        pipe = self.controller.GetPipelineState()

        ib = pipe.GetIBuffer()

        self.check(pipe.IsStripRestartEnabled())
        self.check((pipe.GetStripRestartIndex() & ((1 << (ib.byteStride*8)) - 1)) == 0xffff)
