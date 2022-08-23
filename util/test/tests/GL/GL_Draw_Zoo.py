import rdtest
import renderdoc as rd


class GL_Draw_Zoo(rdtest.Draw_Zoo):
    demos_test_name = 'GL_Draw_Zoo'
    internal = False

    def check_capture(self):
        rdtest.Draw_Zoo.check_capture(self)

        action = self.find_action("GL_PRIMITIVE_RESTART")

        self.check(action is not None)

        action = action.next

        self.controller.SetFrameEvent(action.eventId, True)

        pipe = self.controller.GetPipelineState()

        ib = pipe.GetIBuffer()


        self.check(pipe.IsRestartEnabled())
        self.check((pipe.GetRestartIndex() & ((1 << (ib.byteStride*8)) - 1)) == 0xffff)

        action = self.find_action("GL_PRIMITIVE_RESTART_FIXED_INDEX")

        self.check(action is not None)

        action = action.next

        self.controller.SetFrameEvent(action.eventId, True)

        pipe = self.controller.GetPipelineState()

        ib = pipe.GetIBuffer()

        self.check(pipe.IsRestartEnabled())
        self.check((pipe.GetRestartIndex() & ((1 << (ib.byteStride*8)) - 1)) == 0xffff)
        
        action = self.find_action("GL_ClearDepth")
        self.check(action is not None)
        
        action = action.next
        
        self.check(action.flags & (rd.ActionFlags.Clear|rd.ActionFlags.ClearDepthStencil) == (rd.ActionFlags.Clear|rd.ActionFlags.ClearDepthStencil))
        
