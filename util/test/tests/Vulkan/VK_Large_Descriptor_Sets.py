import renderdoc as rd
import rdtest


class VK_Large_Descriptor_Sets(rdtest.TestCase):
    demos_test_name = 'VK_Large_Descriptor_Sets'

    def run(self):
        self.capture_filename = self.get_capture()

        assert rdtest.util.target_path_exists(self.capture_filename), "Didn't generate capture in make_capture"

        rdtest.log.print("Loading capture")

        memory_before = rd.GetCurrentProcessMemoryUsage()
        start_time = self.get_time()

        self.controller = rdtest.open_capture(self.capture_filename, opts=self.get_replay_options())

        duration = self.get_time() - start_time
        memory_after = rd.GetCurrentProcessMemoryUsage()

        memory_increase = memory_after - memory_before

        rdtest.log.print(f"Loaded capture in {duration} seconds, consuming {memory_increase} bytes of memory")

        if memory_increase > 2000*1000*1000:
            raise rdtest.TestFailureException(f"Memory increase {memory_increase} is too high")
        else:
            rdtest.log.success("Memory usage is OK")

        if rd.IsReleaseBuild():
            if duration.total_seconds() >= 2.5:
                raise rdtest.TestFailureException("Time to load is too high")
            rdtest.log.success("Time to load is OK")
        else:
            rdtest.log.print("Not checking time to load in non-release build")

        self.controller.Shutdown()
