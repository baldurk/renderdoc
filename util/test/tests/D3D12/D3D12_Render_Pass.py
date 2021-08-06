import renderdoc as rd
import rdtest


class D3D12_Render_Pass(rdtest.TestCase):
    demos_test_name = 'D3D12_Render_Pass'

    def check_capture(self):
        rp1 = self.find_action("RP 1")
        rp2 = self.find_action("RP 2")

        action = next(d for d in rp1.children if d.flags & rd.ActionFlags.Drawcall)

        self.controller.SetFrameEvent(action.eventId, False)

        self.check_triangle(back=[0.0, 0.0, 1.0, 1.0])

        action = next(d for d in rp2.children if d.flags & rd.ActionFlags.Drawcall)

        self.controller.SetFrameEvent(action.eventId, False)

        self.check_triangle(back=[1.0, 0.0, 1.0, 1.0])

        action = self.get_last_action()

        self.controller.SetFrameEvent(action.eventId, False)

        self.check_pixel_value(action.copyDestination, 0.45, 0.45, [0.0, 0.0, 1.0, 1.0])
        self.check_pixel_value(action.copyDestination, 0.55, 0.55, [1.0, 0.0, 1.0, 1.0])
        self.check_pixel_value(action.copyDestination, 0.25, 0.25, [0.0, 1.0, 0.0, 1.0])
        self.check_pixel_value(action.copyDestination, 0.75, 0.75, [0.0, 1.0, 0.0, 1.0])

        self.check_pixel_value(action.copyDestination, 0.75, 0.25, [0.0, 0.0, 0.0, 1.0])
        self.check_pixel_value(action.copyDestination, 0.25, 0.75, [0.0, 0.0, 0.0, 1.0])
