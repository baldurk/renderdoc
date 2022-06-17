import renderdoc as rd
import rdtest


class D3D12_Simple_Triangle(rdtest.TestCase):
    demos_test_name = 'D3D12_Simple_Triangle'

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
                'SV_POSITION': [-0.5, -0.5, 0.0, 1.0],
                'COLOR': [0.0, 1.0, 0.0, 1.0],
                'TEXCOORD': [0.0, 0.0],
            },
            1: {
                'vtx': 1,
                'idx': 1,
                'SV_POSITION': [0.0, 0.5, 0.0, 1.0],
                'COLOR': [0.0, 1.0, 0.0, 1.0],
                'TEXCOORD': [0.0, 1.0],
            },
            2: {
                'vtx': 2,
                'idx': 2,
                'SV_POSITION': [0.5, -0.5, 0.0, 1.0],
                'COLOR': [0.0, 1.0, 0.0, 1.0],
                'TEXCOORD': [1.0, 0.0],
            },
        }

        self.check_mesh_data(postvs_ref, postvs_data)

        tex = self.get_resource_by_name("rtvtex").resourceId
        self.check_pixel_value(tex, 1, 1, [0.2, 0.2, 0.2, 1.0])

        tex = self.get_resource_by_name("rtvMStex").resourceId
        self.check_pixel_value(tex, 1, 1, [0.2, 0.2, 0.2, 1.0])

        tex = self.get_resource_by_name("dsvMStex").resourceId
        self.check_pixel_value(tex, 1, 1, [0.2, (0x55)/255.0, 0.0, 1.0])

        # Check that nothing breaks if we call typical enumeration functions on resources
        for res in self.controller.GetResources():
            res: rd.ResourceDescription

            self.controller.GetShaderEntryPoints(res.resourceId)
            self.controller.GetUsage(res.resourceId)
            self.controller.GetBufferData(res.resourceId, 0, 0)
            self.controller.GetTextureData(res.resourceId, rd.Subresource())

