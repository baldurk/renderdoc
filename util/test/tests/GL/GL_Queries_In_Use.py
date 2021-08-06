import renderdoc as rd
import rdtest


class GL_Queries_In_Use(rdtest.TestCase):
    demos_test_name = 'GL_Queries_In_Use'

    def check_capture(self):
        last_action: rd.ActionDescription = self.get_last_action()

        self.controller.SetFrameEvent(last_action.eventId, True)

        tex_details = self.get_texture(last_action.copyDestination)

        action = self.find_action("XFB Draw").next

        self.controller.SetFrameEvent(action.eventId, False)

        postvs_data = self.get_postvs(action, rd.MeshDataStage.VSOut, 0, action.numIndices)

        postvs_ref = {
            0: {
                'vtx': 0,
                'idx': 0,
                'gl_Position': [-0.5, -0.5, 0.0, 1.0],
                'v2f_block.pos': [-0.5, -0.5, 0.0, 1.0],
                'v2f_block.col': [0.0, 1.0, 0.0, 1.0],
                'v2f_block.uv': [0.0, 0.0, 0.0, 1.0],
            },
            1: {
                'vtx': 1,
                'idx': 1,
                'gl_Position': [0.0, 0.5, 0.0, 1.0],
                'v2f_block.pos': [0.0, 0.5, 0.0, 1.0],
                'v2f_block.col': [0.0, 1.0, 0.0, 1.0],
                'v2f_block.uv': [0.0, 1.0, 0.0, 1.0],
            },
            2: {
                'vtx': 2,
                'idx': 2,
                'gl_Position': [0.5, -0.5, 0.0, 1.0],
                'v2f_block.pos': [0.5, -0.5, 0.0, 1.0],
                'v2f_block.col': [0.0, 1.0, 0.0, 1.0],
                'v2f_block.uv': [1.0, 0.0, 0.0, 1.0],
            },
        }

        self.check_mesh_data(postvs_ref, postvs_data)

        results = self.controller.FetchCounters([rd.GPUCounter.RasterizedPrimitives, rd.GPUCounter.VSInvocations, rd.GPUCounter.FSInvocations])

        action = self.find_action("Counters Draw").next

        results = [r for r in results if r.eventId == action.eventId]

        if len(results) != 3:
            raise rdtest.TestFailureException("Expected 3 results, got {} results".format(len(results)))

        for r in results:
            r: rd.CounterResult
            val = r.value.u32
            if r.counter == rd.GPUCounter.RasterizedPrimitives:
                if not rdtest.value_compare(val, 1):
                    raise rdtest.TestFailureException("RasterizedPrimitives result {} is not as expected".format(val))
                else:
                    rdtest.log.success("RasterizedPrimitives result is as expected")
            elif r.counter == rd.GPUCounter.VSInvocations:
                if not rdtest.value_compare(val, 3):
                    raise rdtest.TestFailureException("VSInvocations result {} is not as expected".format(val))
                else:
                    rdtest.log.success("VSInvocations result is as expected")
            elif r.counter == rd.GPUCounter.FSInvocations:
                if val < int(0.1 * tex_details.width * tex_details.height):
                    raise rdtest.TestFailureException("FSInvocations result {} is not as expected".format(val))
                else:
                    rdtest.log.success("FSInvocations result is as expected")
            else:
                raise rdtest.TestFailureException("Unexpected counter result {}".format(r.counter))

        rdtest.log.success("Counter data retrieved successfully")
