import renderdoc as rd
import rdtest


class GL_Empty_Capture(rdtest.TestCase):
    demos_test_name = 'GL_Empty_Capture'
    demos_frame_cap = 100

    def check_capture(self):
        actions = self.controller.GetRootActions()

        assert len(actions) == 1
        assert 'End' in actions[0].customName
        # EID 1 is the implicit context activation
        assert actions[0].eventId == 2
