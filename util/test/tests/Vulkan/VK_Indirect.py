import rdtest
import struct
import renderdoc as rd


def real_action_children(action):
    return [c for c in action.children if not c.flags & rd.ActionFlags.PopMarker]


class VK_Indirect(rdtest.TestCase):
    demos_test_name = 'VK_Indirect'

    def check_overlay(self, pass_samples, *, no_overlay = False):
        pipe: rd.PipeState = self.controller.GetPipelineState()

        tex = rd.TextureDisplay()
        tex.overlay = rd.DebugOverlay.Drawcall
        tex.resourceId = pipe.GetOutputTargets()[0].resource

        self.out.SetTextureDisplay(tex)

        self.out.Display()

        overlay_id = self.out.GetDebugOverlayTexID()

        samples = [
            (50, 40),
            (60, 40),
            (70, 40),

            (90, 40),
            (100, 40),
            (110, 40),

            (130, 40),
            (140, 40),
            (160, 40),

            (190, 40),
            (200, 40),
            (220, 40),



            (50, 190),
            (60, 190),
            (70, 190),

            (90, 190),
            (100, 190),
            (110, 190),

            (130, 190),
            (140, 190),
            (160, 190),

            (190, 190),
            (200, 190),
            (220, 190),



            (330, 40),
            (340, 40),
            (350, 40),

            (330, 115),
            (340, 115),
            (350, 115),

            (330, 190),
            (340, 190),
            (350, 190),
        ]

        # Every sample that isn't passing should be off
        off_alpha = 0.5
        # If the overlay isn't even for a action, it will be cleared to black
        if no_overlay:
            off_alpha = 0.0
            self.check(len(pass_samples) == 0)
        for s in [s for s in samples if s not in pass_samples]:
            self.check_pixel_value(overlay_id, s[0], s[1], [0.0, 0.0, 0.0, off_alpha], eps=1.0/256.0)

        # And the passing samples should be on
        for s in pass_samples:
            self.check_pixel_value(overlay_id, s[0], s[1], [0.8, 0.1, 0.8, 1.0], eps=1.0/256.0)

    def check_capture(self):
        fill = self.find_action("vkCmdFillBuffer")

        self.check(fill is not None)

        buffer_usage = {}

        for usage in self.controller.GetUsage(fill.copyDestination):
            usage: rd.EventUsage
            if usage.eventId not in buffer_usage:
                buffer_usage[usage.eventId] = []
            buffer_usage[usage.eventId].append(usage.usage)

        # The texture is the backbuffer
        tex = self.get_last_action().copyDestination

        for level in ["Primary", "Secondary"]:
            rdtest.log.print("Checking {} indirect calls".format(level))

            final = self.find_action("{}: Final".format(level))

            indirect_count_root = self.find_action("{}: KHR_action_indirect_count".format(level))

            self.controller.SetFrameEvent(final.eventId, False)

            # Check the top row, non indirect count and always present
            self.check_pixel_value(tex, 60, 60, [1.0, 0.0, 0.0, 1.0])
            self.check_pixel_value(tex, 100, 60, [0.0, 0.0, 1.0, 1.0])
            self.check_pixel_value(tex, 145, 35, [1.0, 1.0, 0.0, 1.0])
            self.check_pixel_value(tex, 205, 35, [0.0, 1.0, 1.0, 1.0])

            # if present, check bottom row of indirect count as well as post-count calls
            if indirect_count_root is not None:
                self.check_pixel_value(tex, 60, 220, [0.0, 1.0, 0.0, 1.0])
                self.check_pixel_value(tex, 100, 220, [1.0, 0.0, 1.0, 1.0])
                self.check_pixel_value(tex, 145, 185, [0.5, 1.0, 0.0, 1.0])
                self.check_pixel_value(tex, 205, 185, [0.5, 0.0, 1.0, 1.0])

                self.check_pixel_value(tex, 340, 40, [1.0, 0.5, 0.0, 1.0])
                self.check_pixel_value(tex, 340, 115, [1.0, 0.5, 0.5, 1.0])
                self.check_pixel_value(tex, 340, 190, [1.0, 0.0, 0.5, 1.0])

            dispatches = self.find_action("{}: Dispatches".format(level))

            # Set up a ReplayOutput and TextureSave for quickly testing the action highlight overlay
            self.out: rd.ReplayOutput = self.controller.CreateOutput(rd.CreateHeadlessWindowingData(100, 100),
                                                                     rd.ReplayOutputType.Texture)

            self.check(self.out is not None)

            # Rewind to the start of the capture
            action: rd.ActionDescription = dispatches.children[0]
            while action.previous is not None:
                action = action.previous

            # Ensure we can select all actions
            while action is not None:
                self.controller.SetFrameEvent(action.eventId, False)
                action = action.next

            rdtest.log.success("Selected all {} actions".format(level))

            self.check(dispatches and len(real_action_children(dispatches)) == 3)

            self.check(dispatches.children[0].dispatchDimension == (0, 0, 0))
            self.check(dispatches.children[1].dispatchDimension == (1, 1, 1))
            self.check(dispatches.children[2].dispatchDimension == (3, 4, 5))

            rdtest.log.success("{} Indirect dispatches are the correct dimensions".format(level))

            self.controller.SetFrameEvent(dispatches.children[2].eventId, False)

            pipe: rd.PipeState = self.controller.GetPipelineState()

            ssbo = pipe.GetReadWriteResources(rd.ShaderStage.Compute)[0].descriptor
            data: bytes = self.controller.GetBufferData(ssbo.resource, 0, 0)

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

            empties = self.find_action("{}: Empty draws".format(level))

            self.check(empties and len(real_action_children(empties)) == 2)

            action: rd.ActionDescription
            for action in real_action_children(empties):
                self.check(action.numIndices == 0)
                self.check(action.numInstances == 0)

                self.controller.SetFrameEvent(action.eventId, False)

                # Check that we have empty PostVS
                postvs_data = self.get_postvs(action, rd.MeshDataStage.VSOut, 0, 1)
                self.check(len(postvs_data) == 0)

                # No samples should be passing in the empties
                self.check_overlay([])

            rdtest.log.success("{} empty actions are empty".format(level))

            indirects = self.find_action("{}: Indirect draws".format(level))

            self.check('vkCmdDrawIndirect' in indirects.children[0].customName)
            self.check('vkCmdDrawIndexedIndirect' in indirects.children[1].customName)
            self.check(len(real_action_children(indirects.children[1])) == 2)

            rdtest.log.success("Correct number of {} indirect draws".format(level))

            # vkCmdDrawIndirect(...)
            action = indirects.children[0]
            self.check(action.numIndices == 3)
            self.check(action.numInstances == 2)

            self.controller.SetFrameEvent(action.eventId, False)

            self.check(rd.ResourceUsage.Indirect in buffer_usage[action.eventId])

            # Check that we have PostVS as expected
            postvs_data = self.get_postvs(action, rd.MeshDataStage.VSOut)

            postvs_ref = {
                0: {'vtx': 0, 'idx': 0, 'gl_Position': [-0.8, -0.5, 0.0, 1.0]},
                1: {'vtx': 1, 'idx': 1, 'gl_Position': [-0.7, -0.8, 0.0, 1.0]},
                2: {'vtx': 2, 'idx': 2, 'gl_Position': [-0.6, -0.5, 0.0, 1.0]},
            }

            self.check_mesh_data(postvs_ref, postvs_data)
            self.check(len(postvs_data) == len(postvs_ref))  # We shouldn't have any extra vertices

            self.check_overlay([(60, 40)])

            rdtest.log.success("{} {} is as expected".format(level, action.customName))

            self.check(rd.ResourceUsage.Indirect in buffer_usage[indirects.children[1].eventId])

            # vkCmdDrawIndexedIndirect[0](...)
            action = indirects.children[1].children[0]
            self.check(action.numIndices == 3)
            self.check(action.numInstances == 3)

            self.controller.SetFrameEvent(action.eventId, False)

            # Check that we have PostVS as expected
            postvs_data = self.get_postvs(action, rd.MeshDataStage.VSOut)

            # These indices are the *output* indices, which have been rebased/remapped, so are not the same as the input
            # indices
            postvs_ref = {
                0: {'vtx': 0, 'idx': 6, 'gl_Position': [-0.6, -0.5, 0.0, 1.0]},
                1: {'vtx': 1, 'idx': 7, 'gl_Position': [-0.5, -0.8, 0.0, 1.0]},
                2: {'vtx': 2, 'idx': 8, 'gl_Position': [-0.4, -0.5, 0.0, 1.0]},
            }

            self.check_mesh_data(postvs_ref, postvs_data)
            self.check(len(postvs_data) == len(postvs_ref))  # We shouldn't have any extra vertices

            self.check_overlay([(100, 40)])

            rdtest.log.success("{} {} is as expected".format(level, action.customName))

            # vkCmdDrawIndexedIndirect[1](...)
            action = indirects.children[1].children[1]
            self.check(action.numIndices == 6)
            self.check(action.numInstances == 2)

            self.controller.SetFrameEvent(action.eventId, False)

            # Check that we have PostVS as expected
            postvs_data = self.get_postvs(action, rd.MeshDataStage.VSOut)

            postvs_ref = {
                0: {'vtx': 0, 'idx': 9, 'gl_Position': [-0.4, -0.5, 0.0, 1.0]},
                1: {'vtx': 1, 'idx': 10, 'gl_Position': [-0.3, -0.8, 0.0, 1.0]},
                2: {'vtx': 2, 'idx': 11, 'gl_Position': [-0.2, -0.8, 0.0, 1.0]},

                3: {'vtx': 3, 'idx': 12, 'gl_Position': [-0.1, -0.5, 0.0, 1.0]},
                4: {'vtx': 4, 'idx': 13, 'gl_Position': [ 0.0, -0.8, 0.0, 1.0]},
                5: {'vtx': 5, 'idx': 14, 'gl_Position': [ 0.1, -0.8, 0.0, 1.0]},
            }

            self.check_mesh_data(postvs_ref, postvs_data)
            self.check(len(postvs_data) == len(postvs_ref))  # We shouldn't have any extra vertices

            self.check_overlay([(140, 40), (200, 40)])

            rdtest.log.success("{} {} is as expected".format(level, action.customName))

            if indirect_count_root is not None:
                self.check(indirect_count_root.children[0].customName == '{}: Empty count draws'.format(level))
                self.check(indirect_count_root.children[1].customName == '{}: Indirect count draws'.format(level))

                empties = indirect_count_root.children[0]

                self.check(empties and len(real_action_children(empties)) == 3)

                action: rd.ActionDescription
                for action in real_action_children(empties.children):
                    self.check(action.numIndices == 0)
                    self.check(action.numInstances == 0)

                    self.controller.SetFrameEvent(action.eventId, False)

                    # Check that we have empty PostVS
                    postvs_data = self.get_postvs(action, rd.MeshDataStage.VSOut, 0, 1)
                    self.check(len(postvs_data) == 0)

                    self.check_overlay([], no_overlay=True)

                # vkCmdDrawIndirectCountKHR
                action_indirect = indirect_count_root.children[1].children[0]

                self.check(rd.ResourceUsage.Indirect in buffer_usage[action_indirect.eventId])

                self.check(action_indirect and len(real_action_children(action_indirect)) == 1)

                # vkCmdDrawIndirectCountKHR[0]
                action = action_indirect.children[0]

                self.check(action.numIndices == 3)
                self.check(action.numInstances == 4)

                self.controller.SetFrameEvent(action.eventId, False)

                # Check that we have PostVS as expected
                postvs_data = self.get_postvs(action, rd.MeshDataStage.VSOut)

                # These indices are the *output* indices, which have been rebased/remapped, so are not the same as the input
                # indices
                postvs_ref = {
                    0: {'vtx': 0, 'idx': 0, 'gl_Position': [-0.8, 0.5, 0.0, 1.0]},
                    1: {'vtx': 1, 'idx': 1, 'gl_Position': [-0.7, 0.2, 0.0, 1.0]},
                    2: {'vtx': 2, 'idx': 2, 'gl_Position': [-0.6, 0.5, 0.0, 1.0]},
                }

                self.check_mesh_data(postvs_ref, postvs_data)
                self.check(len(postvs_data) == len(postvs_ref))  # We shouldn't have any extra vertices

                self.check_overlay([(60, 190)])

                rdtest.log.success("{} {} is as expected".format(level, action.customName))

                # vkCmdDrawIndexedIndirectCountKHR
                action_indirect = indirect_count_root.children[1].children[1]

                self.check(action_indirect and len(real_action_children(action_indirect)) == 3)

                # vkCmdDrawIndirectCountKHR[0]
                action = action_indirect.children[0]
                self.check(action.numIndices == 3)
                self.check(action.numInstances == 1)

                self.controller.SetFrameEvent(action.eventId, False)

                # Check that we have PostVS as expected
                postvs_data = self.get_postvs(action, rd.MeshDataStage.VSOut)

                # These indices are the *output* indices, which have been rebased/remapped, so are not the same as the input
                # indices
                postvs_ref = {
                    0: {'vtx': 0, 'idx': 15, 'gl_Position': [-0.6, 0.5, 0.0, 1.0]},
                    1: {'vtx': 1, 'idx': 16, 'gl_Position': [-0.5, 0.2, 0.0, 1.0]},
                    2: {'vtx': 2, 'idx': 17, 'gl_Position': [-0.4, 0.5, 0.0, 1.0]},
                }

                self.check_mesh_data(postvs_ref, postvs_data)
                self.check(len(postvs_data) == len(postvs_ref))  # We shouldn't have any extra vertices

                self.check_overlay([(100, 190)])

                rdtest.log.success("{} {} is as expected".format(level, action.customName))

                # vkCmdDrawIndirectCountKHR[1]
                action = action_indirect.children[1]
                self.check(action.numIndices == 0)
                self.check(action.numInstances == 0)

                self.controller.SetFrameEvent(action.eventId, False)

                postvs_data = self.get_postvs(action, rd.MeshDataStage.VSOut)

                self.check(len(postvs_data) == 0)

                self.check_overlay([])

                rdtest.log.success("{} {} is as expected".format(level, action.customName))

                # vkCmdDrawIndirectCountKHR[2]
                action = action_indirect.children[2]
                self.check(action.numIndices == 6)
                self.check(action.numInstances == 2)

                self.controller.SetFrameEvent(action.eventId, False)

                # Check that we have PostVS as expected
                postvs_data = self.get_postvs(action, rd.MeshDataStage.VSOut)

                # These indices are the *output* indices, which have been rebased/remapped, so are not the same as the input
                # indices
                postvs_ref = {
                    0: {'vtx': 0, 'idx': 18, 'gl_Position': [-0.4, 0.5, 0.0, 1.0]},
                    1: {'vtx': 1, 'idx': 19, 'gl_Position': [-0.3, 0.2, 0.0, 1.0]},
                    2: {'vtx': 2, 'idx': 20, 'gl_Position': [-0.2, 0.2, 0.0, 1.0]},

                    3: {'vtx': 3, 'idx': 21, 'gl_Position': [-0.1, 0.5, 0.0, 1.0]},
                    4: {'vtx': 4, 'idx': 22, 'gl_Position': [ 0.0, 0.2, 0.0, 1.0]},
                    5: {'vtx': 5, 'idx': 23, 'gl_Position': [ 0.1, 0.2, 0.0, 1.0]},
                }

                self.check_mesh_data(postvs_ref, postvs_data)
                self.check(len(postvs_data) == len(postvs_ref))  # We shouldn't have any extra vertices

                self.check_overlay([(140, 190), (200, 190)])

                rdtest.log.success("{} {} is as expected".format(level, action.customName))

                # Now check that the draws post-count are correctly highlighted
                self.controller.SetFrameEvent(self.find_action("{}: Post-count 1".format(level)).children[0].eventId, False)
                self.check_overlay([(340, 40)])
                self.controller.SetFrameEvent(self.find_action("{}: Post-count 2".format(level)).children[0].eventId, False)
                self.check_overlay([(340, 190)])
                self.controller.SetFrameEvent(self.find_action("{}: Post-count 3".format(level)).children[0].eventId, False)
                self.check_overlay([(340, 115)])
            else:
                rdtest.log.print("KHR_action_indirect_count not tested")
