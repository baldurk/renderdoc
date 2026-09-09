import renderdoc as rd
import rdtest


class D3D11_Counters(rdtest.TestCase):
    demos_test_name = 'D3D11_Simple_Triangle'

    def check_capture(self):
        avail = self.controller.EnumerateCounters()
        wanted = [rd.GPUCounter.EventGPUDuration, rd.GPUCounter.VSInvocations, rd.GPUCounter.PSInvocations,
                  rd.GPUCounter.IAPrimitives, rd.GPUCounter.RasterizedPrimitives, rd.GPUCounter.RasterizerInvocations,
                  rd.GPUCounter.SamplesPassed]

        counters = list(set(avail).intersection(set(wanted)))

        results = self.controller.FetchCounters(counters)
        descs = {}

        for c in counters:
            descs[c] = self.controller.DescribeCounter(c)

        action = self.find_action("Draw")

        # filter to only results from the draw
        results = [r for r in results if r.eventId == action.eventId]

        ps = samp = None

        for r in results:
            desc = descs[r.counter]
            if r.counter == rd.GPUCounter.EventGPUDuration:
                val = 0.0
                if desc.resultByteWidth == 8:
                    val = r.value.d
                elif desc.resultByteWidth == 4:
                    val = r.value.f

                # should not be smaller than 0.1 microseconds, and should not be more than 10 milliseconds
                if val < 1.0e-7 or val > 0.01:
                    raise rdtest.TestFailureException(f"{desc.name} of draw {val}s is unexpected")
                else:
                    rdtest.log.success(f"{desc.name} of draw {val}s is expected")
            elif (r.counter == rd.GPUCounter.IAPrimitives or r.counter == rd.GPUCounter.RasterizedPrimitives or
                  r.counter == rd.GPUCounter.RasterizerInvocations):
                val = 0
                if desc.resultByteWidth == 8:
                    val = r.value.u64
                elif desc.resultByteWidth == 4:
                    val = r.value.u32

                if val != 1:
                    raise rdtest.TestFailureException(f"{desc.name} of draw {val} is unexpected")
                else:
                    rdtest.log.success(f"{desc.name} of draw {val} is expected")
            elif r.counter == rd.GPUCounter.VSInvocations:
                val = 0
                if desc.resultByteWidth == 8:
                    val = r.value.u64
                elif desc.resultByteWidth == 4:
                    val = r.value.u32

                if val != 3:
                    raise rdtest.TestFailureException(f"{desc.name} of draw {val} is unexpected")
                else:
                    rdtest.log.success(f"{desc.name} of draw {val} is expected")
            elif r.counter == rd.GPUCounter.PSInvocations or r.counter == rd.GPUCounter.SamplesPassed:
                val = 0
                if desc.resultByteWidth == 8:
                    val = r.value.u64
                elif desc.resultByteWidth == 4:
                    val = r.value.u32

                if r.counter == rd.GPUCounter.PSInvocations:
                    ps = val
                else:
                    samp = val

                # should be around 15000 pixels, but allow for slight rasterization differences
                if val < 14500 or val > 15500:
                    raise rdtest.TestFailureException(f"{desc.name} of draw {val} is unexpected")
                else:
                    rdtest.log.success(f"{desc.name} of draw {val} is expected")

        if ps is not None and samp is not None:
            # allow 500 difference for overshading counting
            if abs(ps - samp) > 500:
                raise rdtest.TestFailureException(f"Samples passed {samp} and PS invocations {ps} don't match")
            else:
                rdtest.log.success(f"Samples passed {samp} and PS invocations {ps} match")

        rdtest.log.success("All counters have expected values")
