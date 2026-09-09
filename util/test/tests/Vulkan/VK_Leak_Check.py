import renderdoc as rd
import rdtest


class VK_Leak_Check(rdtest.TestCase):
    demos_test_name = 'VK_Leak_Check'
    demos_frame_cap = 50000
    demos_frame_count = 10
    demos_timeout = 120

    def check_capture(self):
        memory = rd.GetCurrentProcessMemoryUsage()

        if memory > 500*1000*1000:
            raise rdtest.TestFailureException(f"Memory usage of {memory} is too high")

        rdtest.log.success(f"Capture {self.demos_frame_cap} opened with reasonable memory ({memory})")
