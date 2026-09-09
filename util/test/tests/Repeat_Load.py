import rdtest
import os
import renderdoc as rd


class Repeat_Load(rdtest.TestCase):
    slow_test = True

    def repeat_load(self, path):
        memory_usage = memory_baseline = 0

        for i in range(20):
            rdtest.log.print(f"Loading for iteration {i + 1}")

            try:
                controller = rdtest.open_capture(path)
            except RuntimeError as err:
                rdtest.log.print(f"Skipping. Can't open {path}: {err}")
                return

            rdtest.log.print("Loaded capture.")
            if not self.validate_eventids(controller):
                raise rdtest.TestFailureException("ERROR: capture doesn't have valid event IDs.")

            # Do nothing, just ensure it's loaded
            memory_usage = rd.GetCurrentProcessMemoryUsage()

            # We measure the baseline memory usage during the second peak to avoid any persistent caches etc that might
            # not be full
            if i == 2:
                memory_baseline = memory_usage

            controller.Shutdown()

            pct_over = 'N/A'

            if memory_baseline > 0:
                pct_over = f'{memory_usage / memory_baseline * 100:.2f}%'

            rdtest.log.success(f"Succeeded iteration {i + 1}, memory usage was {memory_usage} ({pct_over} of baseline)")

        pct_over = f'{memory_usage / memory_baseline * 100:.2f}%'
        msg = f'final memory usage was {memory_usage}, {pct_over} compared to baseline {memory_baseline}'

        if memory_baseline * 1.25 < memory_usage:
            raise rdtest.TestFailureException(msg)
        else:
            rdtest.log.success(msg)

    def run(self):
        dir_path = self.get_ref_path('', extra=True)

        for file in os.scandir(dir_path):
            section_name = f"Repeat loading {file.name}"

            rdtest.log.begin_section(section_name)
            self.repeat_load(file.path)
            rdtest.log.end_section(section_name)

        rdtest.log.success("Repeat loaded all files")
