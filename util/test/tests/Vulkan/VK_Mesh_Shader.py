from typing import Tuple

import renderdoc as rd
import rdtest

class VK_Mesh_Shader(rdtest.TestCase):
    demos_test_name = 'VK_Mesh_Shader'
    demos_frame_cap = 5

    def build_local_taskout_reference(self):
        reference: rdtest.MeshReference = {
            0: {
                "padArr": [1000, 1001, 1002, 1003],
                "pad": [123],
                "tri": [0, 1, 2, 3],
            }
        }
        return reference

    def build_meshout_reference(self, orgY: float, color: Tuple[float,float,float,float]):
        countTris = 4 
        triSize = 0.2
        deltX = 0.42
        orgX = -0.65
        i = 0
        reference: rdtest.MeshReference = {}
        for tri in range(countTris):
            for vert in range(3):
                posX = orgX + tri * deltX
                posY = orgY

                if vert == 0:
                    posX += -triSize
                    posY += -triSize
                elif vert == 1:
                    posX += 0.0
                    posY += triSize
                elif vert == 2:
                    posX += triSize
                    posY += -triSize

                reference[i] = {
                    'vtx': i,
                    'idx': i,
                    'gl_Position': [posX, posY, 0.0, 1.0],
                    'outColor': color,
                }
                i += 1
        return reference

    def check_capture(self):
        assert self.controller is not None
        last_action = self.get_last_action()

        self.controller.SetFrameEvent(last_action.eventId, True)

        action = self.find_action("Mesh Shaders")

        action = action.nextAction
        assert action is not None
        name = f"Pure Mesh Shader Test EID:{action.eventId}"
        with rdtest.log.auto_section(name):
            self.controller.SetFrameEvent(action.eventId, False)

            x = 70
            y = 240

            orgY = 0.65
            color = (1.0, 0.0, 0.0, 1.0)
            postms_ref = self.build_meshout_reference(orgY, color)
            postms_data = self.get_postvs(action, rd.MeshDataStage.MeshOut, 0, action.numIndices)
            self.check_mesh_data(postms_ref, postms_data)
            self.check_debug_pixel(x, y)

        y -= 100
        action = action.nextAction
        assert action is not None
        name = f"Task Shader with Local Payload EID:{action.eventId}"
        with rdtest.log.auto_section(name):
            self.controller.SetFrameEvent(action.eventId, False)

            postts_ref = self.build_local_taskout_reference()
            postts_data = self.get_task_data(action)
            self.check_task_data(postts_ref, postts_data)

            orgY = 0.0
            color = (0.0, 0.0, 1.0, 1.0)
            postms_ref = self.build_meshout_reference(orgY, color)
            postms_data = self.get_postvs(action, rd.MeshDataStage.MeshOut, 0, action.numIndices)
            self.check_mesh_data(postms_ref, postms_data)
            self.check_debug_pixel(x, y)

        name = f"Mesh Shader with Points output"
        with rdtest.log.auto_section(name):
            action = action.nextAction
            assert action is not None
            self.controller.SetFrameEvent(action.eventId, False)
            x = 290
            y = 90

            color = [0.0, 1.0, 0.0, 1.0]
            postms_ref: rdtest.MeshReference = {}
            for i in range(6):
                postms_ref[i] = {
                    'vtx': i,
                    'idx': i,
                    'gl_Position': [-0.4 + 0.21 * i, -0.4, 0.0, 1.0],
                    'gl_PointSize': 20.0,
                    'outColor': color,
                }
            postms_data = self.get_postvs(action, rd.MeshDataStage.MeshOut, 0, action.numIndices)
            self.check_mesh_data(postms_ref, postms_data)
            self.check_debug_pixel(x, y)

        with rdtest.log.auto_section("Checking Indirect Action Names"):
            if not self.check_indirect_action_name_consistency(self.controller):
                raise rdtest.TestFailureException("Indirect action parameters do not match its event parameters")
