import rdtest


class D3D12_Buffer_Truncation(rdtest.Buffer_Truncation):
    demos_test_name = 'D3D12_Buffer_Truncation'
    internal = False

    def check_capture(self):
        rdtest.log.begin_section("SM5")
        self.draw_action = self.find_action("SM5")
        self.draw_action = self.draw_action.nextAction
        super().check_capture()
        rdtest.log.end_section("SM5")

        self.draw_action = self.find_action("SM6.0")
        if self.draw_action is None:
            rdtest.log.print("No SM6.0 action to test")
            return
        rdtest.log.begin_section("SM6.0")
        self.draw_action = self.draw_action.nextAction
        super().check_capture()
        rdtest.log.end_section("SM6.0")

        self.draw_action = self.find_action("SM6.6")
        if self.draw_action is None:
            rdtest.log.print("No SM6.6 action to test")
            return
        rdtest.log.begin_section("SM6.6")
        self.draw_action = self.draw_action.nextAction
        super().check_capture()
        rdtest.log.end_section("SM6.6")
