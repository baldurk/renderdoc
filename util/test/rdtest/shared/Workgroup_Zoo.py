import rdtest

# Not a real test, re-used by API-specific tests
class Workgroup_Zoo(rdtest.Subgroup_Zoo):
    internal = True
    slow_test = True
    demos_test_name = None

    def check_capture(self):
        compute_dims = [a for a in self.find_action("Compute Tests").children if 'x' in a.customName]

        # threads to check. largest dimension only (all small dim checked)
        thread_checks = [
            # first few
            0, 1, 2,
            # near end of 16-subgroup and boundary
            15, 16, 17,
            # near end of 32-subgroup and boundary
            31, 32, 33,
            # near end of 64-subgroup and boundary
            63, 64, 65,
            # near end of 128-subgroup and boundary
            127, 128, 129,
            # large values 
            150
        ]

        self.workgroup = (1, 0, 0)
        if self.check_compute_tests(compute_dims, thread_checks):
            raise rdtest.TestFailureException("Some tests were not as expected")

        rdtest.log.success("All tests matched")