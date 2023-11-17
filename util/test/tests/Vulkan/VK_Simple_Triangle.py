import renderdoc as rd
import rdtest


class VK_Simple_Triangle(rdtest.TestCase):
    demos_test_name = 'VK_Simple_Triangle'

    def check_capture(self):
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

        save_data = rd.TextureSave()
        save_data.destType = rd.FileType.DDS
        path = rdtest.get_tmp_path('temp.dds')

        # Check that nothing breaks if we call typical enumeration functions on resources
        for res in self.controller.GetResources():
            res: rd.ResourceDescription

            save_data.resourceId = res.resourceId

            self.controller.GetShaderEntryPoints(res.resourceId)
            self.controller.GetUsage(res.resourceId)
            self.controller.GetBufferData(res.resourceId, 0, 0)
            self.controller.GetTextureData(res.resourceId, rd.Subresource())
            self.controller.SaveTexture(save_data, path)
