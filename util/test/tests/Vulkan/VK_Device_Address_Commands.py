import renderdoc as rd
import rdtest


class VK_Device_Address_Commands(rdtest.TestCase):
    demos_test_name = 'VK_Device_Address_Commands'

    def check_capture(self):
        action = self.find_action("Draw")

        assert action is not None

        self.set_event(action.eventId, True)

        pipe = self.controller.GetPipelineState()
        assert pipe.GetVBuffers()[0].resourceId != rd.ResourceId()
        assert pipe.GetIBuffer().resourceId != rd.ResourceId()

        rdtest.log.success('Vertex and Index buffers are bound')

        postvs_data = self.get_postvs(action, rd.MeshDataStage.VSOut, 0, action.numIndices)

        postvs_ref: rdtest.MeshReference = {
            0: {
                'vtx': 0,
                'idx': 0,
                'gl_Position': [-1.0, 1.0, 0.0, 1.0],
                'vertOut.pos': [-1.0, 1.0, 0.0, 1.0],
                'vertOut.col': [0.0, 1.0, 0.0, 1.0],
                'vertOut.uv': [0.0, 0.5, 0.0, 1.0],
            },
            1: {
                'vtx': 1,
                'idx': 1,
                'gl_Position': [-1.0, -1.0, 0.0, 1.0],
                'vertOut.pos': [-1.0, -1.0, 0.0, 1.0],
                'vertOut.col': [0.0, 0.0, 0.0, 1.0],
                'vertOut.uv': [0.0, 0.0, 0.0, 1.0],
            },
            2: {
                'vtx': 2,
                'idx': 2,
                'gl_Position': [1.0, 1.0, 0.0, 1.0],
                'vertOut.pos': [1.0, 1.0, 0.0, 1.0],
                'vertOut.col': [1.0, 1.0, 0.0, 1.0],
                'vertOut.uv': [0.5, 0.5, 0.0, 1.0],
            },
            3: {
                'vtx': 3,
                'idx': 3,
                'gl_Position': [1.0, -1.0, 0.0, 1.0],
                'vertOut.pos': [1.0, -1.0, 0.0, 1.0],
                'vertOut.col': [1.0, 0.0, 0.0, 1.0],
                'vertOut.uv': [0.5, 0.0, 0.0, 1.0],
            },
            4: {
                'vtx': 4,
                'idx': 1,
                'gl_Position': [-1.0, -1.0, 0.0, 1.0],
                'vertOut.pos': [-1.0, -1.0, 0.0, 1.0],
                'vertOut.col': [0.0, 0.0, 0.0, 1.0],
                'vertOut.uv': [0.0, 0.0, 0.0, 1.0],
            },
            5: {
                'vtx': 5,
                'idx': 2,
                'gl_Position': [1.0, 1.0, 0.0, 1.0],
                'vertOut.pos': [1.0, 1.0, 0.0, 1.0],
                'vertOut.col': [1.0, 1.0, 0.0, 1.0],
                'vertOut.uv': [0.5, 0.5, 0.0, 1.0],
            },
        }

        self.check_mesh_data(postvs_ref, postvs_data)

        self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 155, 195, [0.816, 0.723, 0.361, 1.0], eps=1.0/255.0)
        self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 190, 195, [0.723, 0.816, 0.361, 1.0], eps=1.0/255.0)
        self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 255, 195, [0.637, 0.906, 0.27, 1.0], eps=1.0/255.0)
        self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 230, 150, [0.723, 0.816, 0.361, 1.0], eps=1.0/255.0)

        self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 190, 80, [0.906, 0.637, 0.361, 1.0], eps=1.0/255.0)
        self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 200, 80, [0.906, 0.637, 0.361, 1.0], eps=1.0/255.0)
        self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 210, 80, [0.906, 0.637, 0.361, 1.0], eps=1.0/255.0)


        action = self.find_action("After CopyImageToMemory")

        assert action is not None

        self.set_event(action.eventId, True)

        buffer = self.get_resource_by_name("Address Buffer").resourceId

        if int.from_bytes(self.controller.GetBufferData(buffer, 9404, 4), byteorder='big') == 0:
            raise rdtest.TestFailureException("ImageToMemory copy failed")
        else:
            rdtest.log.success('ImageToMemory copy succeeded')
