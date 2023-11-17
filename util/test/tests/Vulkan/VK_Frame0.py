import renderdoc as rd
import rdtest


class VK_Frame0(rdtest.TestCase):
    demos_test_name = 'VK_Simple_Triangle'
    demos_frame_cap = 0

    def check_capture(self):
        first_action = self.get_first_action()

        found = False
        sdfile = self.controller.GetStructuredFile()
        for e in first_action.events:
            c: rd.SDChunk = sdfile.chunks[e.chunkIndex]
            if 'vkUnmapMemory' in c.name:
                found = True

        if not found:
            raise rdtest.TestFailureException("Expected an vkUnmapMemory() chunk in frame 0, but couldn't find it!")

        last_action: rd.ActionDescription = self.get_last_action()

        self.controller.SetFrameEvent(last_action.eventId, True)

        self.check_triangle(out=last_action.copyDestination)

        self.check_export(self.capture_filename)

        action = self.find_action("Draw")

        self.controller.SetFrameEvent(action.eventId, False)

        postvs_data = self.get_postvs(action, rd.MeshDataStage.VSOut, 0, action.numIndices)

        postvs_ref = {
            0: {
                'vtx': 0,
                'idx': 0,
                'gl_Position': [-0.5, 0.5, 0.0, 1.0],
                'vertOut.pos': [-0.5, 0.5, 0.0, 1.0],
                'vertOut.col': [0.0, 1.0, 0.0, 1.0],
                'vertOut.uv': [0.0, 0.0, 0.0, 1.0],
            },
            1: {
                'vtx': 1,
                'idx': 1,
                'gl_Position': [0.0, -0.5, 0.0, 1.0],
                'vertOut.pos': [0.0, -0.5, 0.0, 1.0],
                'vertOut.col': [0.0, 1.0, 0.0, 1.0],
                'vertOut.uv': [0.0, 1.0, 0.0, 1.0],
            },
            2: {
                'vtx': 2,
                'idx': 2,
                'gl_Position': [0.5, 0.5, 0.0, 1.0],
                'vertOut.pos': [0.5, 0.5, 0.0, 1.0],
                'vertOut.col': [0.0, 1.0, 0.0, 1.0],
                'vertOut.uv': [1.0, 0.0, 0.0, 1.0],
            },
        }

        self.check_mesh_data(postvs_ref, postvs_data)

        # Check that nothing breaks if we call typical enumeration functions on resources
        for res in self.controller.GetResources():
            res: rd.ResourceDescription

            self.controller.GetShaderEntryPoints(res.resourceId)
            self.controller.GetUsage(res.resourceId)
            self.controller.GetBufferData(res.resourceId, 0, 0)
            self.controller.GetTextureData(res.resourceId, rd.Subresource())
