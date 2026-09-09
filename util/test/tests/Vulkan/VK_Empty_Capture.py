import rdtest


class VK_Empty_Capture(rdtest.TestCase):
    demos_test_name = 'VK_Empty_Capture'
    demos_frame_cap = 100

    def check_capture(self):
        actions = self.controller.GetRootActions()

        assert len(actions) == 1
        assert 'End' in actions[0].customName
        assert actions[0].eventId == 1
