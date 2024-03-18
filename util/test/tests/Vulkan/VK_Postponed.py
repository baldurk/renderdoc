import rdtest
import renderdoc as rd


class VK_Postponed(rdtest.TestCase):
    demos_test_name = 'VK_Postponed'
    demos_frame_cap = 505
    demos_frame_count = 1

    def check_capture(self):
        tex = self.get_resource_by_name("offimg").resourceId

        action: rd.ActionDescription = self.find_action("Pre-Copy")
        self.controller.SetFrameEvent(action.eventId, True)

        # starts off cleared
        self.check_pixel_value(tex, 1, 1, [0.2, 0.2, 0.2, 1.0])

        action: rd.ActionDescription = self.find_action("Post-Copy", action.eventId)
        self.controller.SetFrameEvent(action.eventId, True)

        # Gets green from first postponed image copy
        self.check_pixel_value(tex, 1, 1, [0.2, 1.0, 0.2, 1.0])

        action: rd.ActionDescription = self.find_action("Pre-Copy", action.eventId)
        self.controller.SetFrameEvent(action.eventId, True)

        # Cleared to black before second copy
        self.check_pixel_value(tex, 1, 1, [0.0, 0.0, 0.0, 1.0])

        action: rd.ActionDescription = self.find_action("Post-Copy", action.eventId)
        self.controller.SetFrameEvent(action.eventId, True)

        self.check_pixel_value(tex, 1, 1, [0.2, 1.0, 0.2, 1.0])

        rdtest.log.success("Image copies are all correct")

        action: rd.ActionDescription = self.find_action("Post-Draw", action.eventId)
        self.controller.SetFrameEvent(action.eventId, True)

        pipe = self.controller.GetPipelineState()
        tex_details = self.get_texture(pipe.GetOutputTargets()[0].resource)

        # Green triangles in each quadrant
        w = int(tex_details.width / 2)
        h = int(tex_details.height / 2)
        self.check_triangle(fore=[0.0, 1.0, 0.0, 1.0],
                            vp=(0, 0, w, h))
        self.check_triangle(fore=[0.0, 1.0, 0.0, 1.0],
                            vp=(w, 0, w, h))
        self.check_triangle(fore=[0.0, 1.0, 0.0, 1.0],
                            vp=(0, h, w, h))
        self.check_triangle(fore=[0.0, 1.0, 0.0, 1.0],
                            vp=(w, h, w, h))

        rdtest.log.success("Draws are all correct")
