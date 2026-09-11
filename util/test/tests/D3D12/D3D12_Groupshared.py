import rdtest

class D3D12_Groupshared(rdtest.Groupshared):
    internal = False
    demos_test_name = 'D3D12_Groupshared'

    def check_capture(self):
        overallFailed = False
        action = self.find_action("SM5")
        assert action is not None
        self.check_compute_section_tests(action)

        action = self.find_action("SM6")
        assert action is not None
        self.check_compute_section_tests(action)
        if overallFailed:
            raise rdtest.TestFailureException("Some tests were not as expected")

        rdtest.log.success("All tests matched")
