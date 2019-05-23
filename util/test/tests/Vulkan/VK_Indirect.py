import rdtest
import struct
import renderdoc as rd


class VK_Indirect(rdtest.TestCase):
    demos_test_name = 'VK_Indirect'

    def check_overlay(self, eventId: int, out: rd.ReplayOutput, tex: rd.TextureDisplay, save_data: rd.TextureSave):
        pipe: rd.PipeState = self.controller.GetPipelineState()

        # Check that the highlight draw overlay is empty
        tex.resourceId = pipe.GetOutputTargets()[0].resourceId

        out.SetTextureDisplay(tex)

        overlay_path = rdtest.get_tmp_path(str(eventId) + '_draw.png')
        ref_path = self.get_ref_path(str(eventId) + '_draw.png')

        save_data.resourceId = out.GetDebugOverlayTexID()

        self.controller.SaveTexture(save_data, overlay_path)

        if not rdtest.png_compare(overlay_path, ref_path):
            raise rdtest.TestFailureException("Reference and output image differ @ EID {}".format(str(eventId)),
                                              ref_path, overlay_path)

    def check_capture(self):
        self.check_final_backbuffer()

        for level in ["Primary", "Secondary"]:
            rdtest.log.print("Checking {} indirect calls".format(level))

            dispatches = self.find_draw("{}: Dispatches".format(level))

            # Set up a ReplayOutput and TextureSave for quickly testing the drawcall highlight overlay
            out: rd.ReplayOutput = self.controller.CreateOutput(rd.CreateHeadlessWindowingData(100, 100),
                                                                rd.ReplayOutputType.Texture)

            self.check(out is not None)

            tex = rd.TextureDisplay()
            tex.overlay = rd.DebugOverlay.Drawcall

            save_data = rd.TextureSave()
            save_data.destType = rd.FileType.PNG

            # Rewind to the start of the capture
            draw: rd.DrawcallDescription = dispatches.children[0]
            while draw.previous is not None:
                draw = draw.previous

            # Ensure we can select all draws
            while draw is not None:
                self.controller.SetFrameEvent(draw.eventId, False)
                draw = draw.next

            rdtest.log.success("Selected all {} draws".format(level))

            self.check(dispatches and len(dispatches.children) == 3)

            self.check(dispatches.children[0].dispatchDimension == [0,0,0])
            self.check(dispatches.children[1].dispatchDimension == [1,1,1])
            self.check(dispatches.children[2].dispatchDimension == [3,4,5])

            rdtest.log.success("{} Indirect dispatches are the correct dimensions".format(level))

            self.controller.SetFrameEvent(dispatches.children[2].eventId, False)

            pipe: rd.PipeState = self.controller.GetPipelineState()

            ssbo: rd.BoundResource = pipe.GetReadWriteResources(rd.ShaderStage.Compute)[0].resources[0]
            data: bytes = self.controller.GetBufferData(ssbo.resourceId, 0, 0)

            rdtest.log.print("Got {} bytes of uints".format(len(data)))

            uints = [struct.unpack_from('=4L', data, offs) for offs in range(0, len(data), 16)]

            for x in range(0, 6):  # 3 groups of 2 threads each
                for y in range(0, 8):  # 3 groups of 2 threads each
                    for z in range(0, 5):  # 5 groups of 1 thread each
                        idx = 100 + z*8*6 + y*6 + x
                        if not rdtest.value_compare(uints[idx], [x, y, z, 12345]):
                            raise rdtest.TestFailureException(
                                'expected thread index data @ {},{},{}: {} is not as expected: {}'
                                    .format(x, y, z, uints[idx], [x, y, z, 12345]))

            rdtest.log.success("Dispatched buffer contents are as expected for {}".format(level))

            empties = self.find_draw("{}: Empty draws".format(level))

            self.check(empties and len(empties.children) == 2)

            draw: rd.DrawcallDescription
            for draw in empties.children:
                self.check(draw.numIndices == 0)
                self.check(draw.numInstances == 0)

                self.controller.SetFrameEvent(draw.eventId, False)

                # Check that we have empty PostVS
                postvs_data = self.get_postvs(rd.MeshDataStage.VSOut, 0, 1)
                self.check(len(postvs_data) == 0)

                self.check_overlay(draw.eventId, out, tex, save_data)

            rdtest.log.success("{} empty draws are empty".format(level))

            indirects = self.find_draw("{}: Indirect draws".format(level))

            self.check('vkCmdDrawIndirect' in indirects.children[0].name)
            self.check('vkCmdDrawIndexedIndirect' in indirects.children[1].name)
            self.check(len(indirects.children[1].children) == 2)

            rdtest.log.success("Correct number of {} indirect draws".format(level))

            # vkCmdDrawIndirect(...)
            draw = indirects.children[0]
            self.check(draw.numIndices == 3)
            self.check(draw.numInstances == 2)

            self.controller.SetFrameEvent(draw.eventId, False)

            # Check that we have PostVS as expected
            postvs_data = self.get_postvs(rd.MeshDataStage.VSOut)

            postvs_ref = {
                0: {'vtx': 0, 'idx': 0, 'gl_PerVertex.gl_Position': [-0.8, -0.5, 0.0, 1.0]},
                1: {'vtx': 1, 'idx': 1, 'gl_PerVertex.gl_Position': [-0.7, -0.8, 0.0, 1.0]},
                2: {'vtx': 2, 'idx': 2, 'gl_PerVertex.gl_Position': [-0.6, -0.5, 0.0, 1.0]},
            }

            self.check_mesh_data(postvs_ref, postvs_data)
            self.check(len(postvs_data) == len(postvs_ref))  # We shouldn't have any extra vertices

            self.check_overlay(draw.eventId, out, tex, save_data)

            rdtest.log.success("{} {} is as expected".format(level, draw.name))

            # vkCmdDrawIndexedIndirect[0](...)
            draw = indirects.children[1].children[0]
            self.check(draw.numIndices == 3)
            self.check(draw.numInstances == 3)

            self.controller.SetFrameEvent(draw.eventId, False)

            # Check that we have PostVS as expected
            postvs_data = self.get_postvs(rd.MeshDataStage.VSOut)

            # These indices are the *output* indices, which have been rebased/remapped, so are not the same as the input
            # indices
            postvs_ref = {
                0: {'vtx': 0, 'idx': 0, 'gl_PerVertex.gl_Position': [-0.6, -0.5, 0.0, 1.0]},
                1: {'vtx': 1, 'idx': 1, 'gl_PerVertex.gl_Position': [-0.5, -0.8, 0.0, 1.0]},
                2: {'vtx': 2, 'idx': 2, 'gl_PerVertex.gl_Position': [-0.4, -0.5, 0.0, 1.0]},
            }

            self.check_mesh_data(postvs_ref, postvs_data)
            self.check(len(postvs_data) == len(postvs_ref))  # We shouldn't have any extra vertices

            self.check_overlay(draw.eventId, out, tex, save_data)

            rdtest.log.success("{} {} is as expected".format(level, draw.name))

            # vkCmdDrawIndexedIndirect[1](...)
            draw = indirects.children[1].children[1]
            self.check(draw.numIndices == 6)
            self.check(draw.numInstances == 2)

            self.controller.SetFrameEvent(draw.eventId, False)

            # Check that we have PostVS as expected
            postvs_data = self.get_postvs(rd.MeshDataStage.VSOut)

            postvs_ref = {
                0: {'vtx': 0, 'idx': 0, 'gl_PerVertex.gl_Position': [-0.4, -0.5, 0.0, 1.0]},
                1: {'vtx': 1, 'idx': 1, 'gl_PerVertex.gl_Position': [-0.3, -0.8, 0.0, 1.0]},
                2: {'vtx': 2, 'idx': 2, 'gl_PerVertex.gl_Position': [-0.2, -0.8, 0.0, 1.0]},

                3: {'vtx': 3, 'idx': 3, 'gl_PerVertex.gl_Position': [-0.1, -0.5, 0.0, 1.0]},
                4: {'vtx': 4, 'idx': 4, 'gl_PerVertex.gl_Position': [ 0.0, -0.8, 0.0, 1.0]},
                5: {'vtx': 5, 'idx': 5, 'gl_PerVertex.gl_Position': [ 0.1, -0.8, 0.0, 1.0]},
            }

            self.check_mesh_data(postvs_ref, postvs_data)
            self.check(len(postvs_data) == len(postvs_ref))  # We shouldn't have any extra vertices

            self.check_overlay(draw.eventId, out, tex, save_data)

            rdtest.log.success("{} {} is as expected".format(level, draw.name))

            indirect_count_root = self.find_draw("{}: KHR_draw_indirect_count".format(level))

            if indirect_count_root is not None:
                self.check(indirect_count_root.children[0].name == '{}: Empty count draws'.format(level))
                self.check(indirect_count_root.children[1].name == '{}: Indirect count draws'.format(level))

                empties = indirect_count_root.children[0]

                self.check(empties and len(empties.children) == 2)

                draw: rd.DrawcallDescription
                for draw in empties.children:
                    self.check(draw.numIndices == 0)
                    self.check(draw.numInstances == 0)

                    self.controller.SetFrameEvent(draw.eventId, False)

                    # Check that we have empty PostVS
                    postvs_data = self.get_postvs(rd.MeshDataStage.VSOut, 0, 1)
                    self.check(len(postvs_data) == 0)

                    self.check_overlay(draw.eventId, out, tex, save_data)

                # vkCmdDrawIndirectCountKHR
                draw_indirect = indirect_count_root.children[1].children[0]

                self.check(draw_indirect and len(draw_indirect.children) == 1)

                # vkCmdDrawIndirectCountKHR[0]
                draw = draw_indirect.children[0]

                self.check(draw.numIndices == 3)
                self.check(draw.numInstances == 4)

                self.controller.SetFrameEvent(draw.eventId, False)

                # Check that we have PostVS as expected
                postvs_data = self.get_postvs(rd.MeshDataStage.VSOut)

                # These indices are the *output* indices, which have been rebased/remapped, so are not the same as the input
                # indices
                postvs_ref = {
                    0: {'vtx': 0, 'idx': 0, 'gl_PerVertex.gl_Position': [-0.8, 0.5, 0.0, 1.0]},
                    1: {'vtx': 1, 'idx': 1, 'gl_PerVertex.gl_Position': [-0.7, 0.2, 0.0, 1.0]},
                    2: {'vtx': 2, 'idx': 2, 'gl_PerVertex.gl_Position': [-0.6, 0.5, 0.0, 1.0]},
                }

                self.check_mesh_data(postvs_ref, postvs_data)
                self.check(len(postvs_data) == len(postvs_ref))  # We shouldn't have any extra vertices

                self.check_overlay(draw.eventId, out, tex, save_data)

                rdtest.log.success("{} {} is as expected".format(level, draw.name))

                # vkCmdDrawIndexedIndirectCountKHR
                draw_indirect = indirect_count_root.children[1].children[1]

                self.check(draw_indirect and len(draw_indirect.children) == 3)

                # vkCmdDrawIndirectCountKHR[0]
                draw = draw_indirect.children[0]
                self.check(draw.numIndices == 3)
                self.check(draw.numInstances == 1)

                self.controller.SetFrameEvent(draw.eventId, False)

                # Check that we have PostVS as expected
                postvs_data = self.get_postvs(rd.MeshDataStage.VSOut)

                # These indices are the *output* indices, which have been rebased/remapped, so are not the same as the input
                # indices
                postvs_ref = {
                    0: {'vtx': 0, 'idx': 0, 'gl_PerVertex.gl_Position': [-0.6, 0.5, 0.0, 1.0]},
                    1: {'vtx': 1, 'idx': 1, 'gl_PerVertex.gl_Position': [-0.5, 0.2, 0.0, 1.0]},
                    2: {'vtx': 2, 'idx': 2, 'gl_PerVertex.gl_Position': [-0.4, 0.5, 0.0, 1.0]},
                }

                self.check_mesh_data(postvs_ref, postvs_data)
                self.check(len(postvs_data) == len(postvs_ref))  # We shouldn't have any extra vertices

                self.check_overlay(draw.eventId, out, tex, save_data)

                rdtest.log.success("{} {} is as expected".format(level, draw.name))

                # vkCmdDrawIndirectCountKHR[1]
                draw = draw_indirect.children[1]
                self.check(draw.numIndices == 0)
                self.check(draw.numInstances == 0)

                self.controller.SetFrameEvent(draw.eventId, False)

                postvs_data = self.get_postvs(rd.MeshDataStage.VSOut)

                self.check(len(postvs_data) == 0)

                self.check_overlay(draw.eventId, out, tex, save_data)

                rdtest.log.success("{} {} is as expected".format(level, draw.name))

                # vkCmdDrawIndirectCountKHR[2]
                draw = draw_indirect.children[2]
                self.check(draw.numIndices == 6)
                self.check(draw.numInstances == 2)

                self.controller.SetFrameEvent(draw.eventId, False)

                # Check that we have PostVS as expected
                postvs_data = self.get_postvs(rd.MeshDataStage.VSOut)

                # These indices are the *output* indices, which have been rebased/remapped, so are not the same as the input
                # indices
                postvs_ref = {
                    0: {'vtx': 0, 'idx': 0, 'gl_PerVertex.gl_Position': [-0.4, 0.5, 0.0, 1.0]},
                    1: {'vtx': 1, 'idx': 1, 'gl_PerVertex.gl_Position': [-0.3, 0.2, 0.0, 1.0]},
                    2: {'vtx': 2, 'idx': 2, 'gl_PerVertex.gl_Position': [-0.2, 0.2, 0.0, 1.0]},

                    3: {'vtx': 3, 'idx': 3, 'gl_PerVertex.gl_Position': [-0.1, 0.5, 0.0, 1.0]},
                    4: {'vtx': 4, 'idx': 4, 'gl_PerVertex.gl_Position': [ 0.0, 0.2, 0.0, 1.0]},
                    5: {'vtx': 5, 'idx': 5, 'gl_PerVertex.gl_Position': [ 0.1, 0.2, 0.0, 1.0]},
                }

                self.check_mesh_data(postvs_ref, postvs_data)
                self.check(len(postvs_data) == len(postvs_ref))  # We shouldn't have any extra vertices

                self.check_overlay(draw.eventId, out, tex, save_data)

                rdtest.log.success("{} {} is as expected".format(level, draw.name))
            else:
                rdtest.log.print("KHR_draw_indirect_count not tested")
