import renderdoc as rd
import rdtest


class GL_Empty_Capture(rdtest.TestCase):
    demos_test_name = 'GL_Empty_Capture'
    demos_frame_cap = 100

    def check_capture(self):
        actions = self.controller.GetRootActions()

        self.check(len(actions) == 1)
        self.check('End' in actions[0].customName)
        # EID 1 is the implicit context activation
        self.check(actions[0].eventId == 2)