import renderdoc as rd
import rdtest


class VK_Load_Store_None(rdtest.TestCase):
    demos_test_name = 'VK_Load_Store_None'

    def check_capture(self):
        res = self.get_resource_by_name('PreserveImg').resourceId

        for action in [self.find_action("BeginRender"), self.find_action("Draw"), self.find_action("EndRender"),
                       self.find_action("Blit")]:
            self.controller.SetFrameEvent(action.eventId, True)

            self.check_pixel_value(res, 200, 125, [0.0, 1.0, 0.0, 1.0])
            self.check_pixel_value(res, 200, 90, [0.2, 0.2, 0.2, 1.0])
            self.check_pixel_value(res, 200, 160, [0.2, 0.2, 0.2, 1.0])
            self.check_pixel_value(res, 250, 125, [0.2, 0.2, 0.2, 1.0])
            self.check_pixel_value(res, 100, 125, [0.2, 0.2, 0.2, 1.0])

            rdtest.log.success("Preserved image is as expected at {}".format(self.action_name(action)))
