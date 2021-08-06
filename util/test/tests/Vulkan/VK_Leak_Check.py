import renderdoc as rd
import rdtest


class VK_Leak_Check(rdtest.TestCase):
    demos_test_name = 'VK_Leak_Check'
    demos_frame_cap = 50000
    demos_frame_count = 10
    demos_timeout = 120

    def check_capture(self):
        memory: int = rd.GetCurrentProcessMemoryUsage()

        if memory > 500*1000*1000:
            raise rdtest.TestFailureException("Memory usage of {} is too high".format(memory))

        rdtest.log.success("Capture {} opened with reasonable memory ({})".format(self.demos_frame_cap, memory))
