from __future__ import annotations
from typing import Dict, List, Tuple

import rdtest
import struct
import renderdoc as rd

def real_action_children(action: rd.ActionDescription):
    return [c for c in action.children if not c.flags & rd.ActionFlags.PopMarker]


class VK_Indirect(rdtest.TestCase):
    demos_test_name = 'VK_Indirect'

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

    def check_pixel_history_succeeds(self, eid: int, x: int, y: int):
        pipe = self.controller.GetPipelineState()
        rt = pipe.GetOutputTargets()[0]
        tex = rt.resource
        sub = rd.Subresource()
        modifs = self.controller.PixelHistory(tex, x, y, sub, rt.format.compType)
        if len(modifs) < 2:
            raise rdtest.TestFailureException(f"EID: {eid} No pixel history found at ({x}, {y})")
        rdtest.log.success(f"EID: {eid} Pixel History {x}, {y} Worked")

    def check_overlay(self, pass_samples: List[Tuple[int,int]], *, no_overlay = False):
        pipe = self.controller.GetPipelineState()

        tex = rd.TextureDisplay()
        tex.overlay = rd.DebugOverlay.Drawcall
        tex.resourceId = pipe.GetOutputTargets()[0].resource

        self.out.SetTextureDisplay(tex)

        self.out.Display()

        overlay_id = self.out.GetDebugOverlayTexID()

        # Every sample that isn't passing should be off
        off_alpha = 0.5
        # If the overlay isn't even for a action, it will be cleared to black
        if no_overlay:
            assert len(pass_samples) == 0
        for s in [s for s in self.samples if s not in pass_samples]:
            self.check_pixel_value(overlay_id, s[0], s[1], [0.0, 0.0, 0.0, off_alpha], eps=1.0/256.0)

        # And the passing samples should be on
        for s in pass_samples:
            self.check_pixel_value(overlay_id, s[0], s[1], [0.8, 0.1, 0.8, 1.0], eps=1.0/256.0)

    def get_overlay_pixel(self, overlay: rd.DebugOverlay, col_tex: rd.ResourceId, x: int, y: int):
        tex = rd.TextureDisplay()
        tex.resourceId = col_tex
        tex.overlay = overlay
        tex.subresource.sample = 0

        out = self.controller.CreateOutput(rd.CreateHeadlessWindowingData(100, 100), rd.ReplayOutputType.Texture)
        out.SetTextureDisplay(tex)
        out.Display()
        overlayTex = out.GetDebugOverlayTexID()
        if overlay == rd.DebugOverlay.ClearBeforeDraw:
            overlayTex = col_tex
        if overlay == rd.DebugOverlay.ClearBeforePass:
            overlayTex = col_tex

        picked = self.controller.PickPixel(overlayTex, x, y, rd.Subresource(), rd.CompType.Typeless)
        out.Shutdown()
        return picked

    def check_overlays(self, eid: int, x: int, y: int):
        with rdtest.log.auto_section(f'EID {eid} Checking Overlays at {x}, {y}'):
            pipe = self.controller.GetPipelineState()
            if len(pipe.GetOutputTargets()) == 0:
                raise rdtest.TestFailureException("No output targets found")

            col_tex = pipe.GetOutputTargets()[0].resource

            for overlay in rd.DebugOverlay:
                if overlay == rd.DebugOverlay.NoOverlay:
                    continue
                if overlay == rd.DebugOverlay.NaN or overlay == rd.DebugOverlay.Clipping:
                    continue
                if overlay == rd.DebugOverlay.ViewportScissor:
                    continue

                emptyPixel = (0.0, 0.0, 0.0, 0.0)
                picked = self.get_overlay_pixel(overlay, col_tex, x, y)
                if picked.floatValue == emptyPixel:
                    raise rdtest.TestFailureException(f"{overlay.name} overlay is empty")

            # Check "Quad Overdraw (Draw)/(Pass)" match
            pickedDraw = self.get_overlay_pixel(rd.DebugOverlay.QuadOverdrawDraw, col_tex, x, y)
            pickedPass = self.get_overlay_pixel(rd.DebugOverlay.QuadOverdrawPass, col_tex, x, y)
            if pickedDraw.floatValue != pickedPass.floatValue:
                raise rdtest.TestFailureException(f"Quad Overdraw Draw and Pass do not match: {pickedDraw.floatValue} vs {pickedPass.floatValue}")
            pickedDraw = self.get_overlay_pixel(rd.DebugOverlay.TriangleSizeDraw, col_tex, x, y)
            pickedPass = self.get_overlay_pixel(rd.DebugOverlay.TriangleSizePass, col_tex, x, y)
            if pickedDraw.floatValue != pickedPass.floatValue:
                raise rdtest.TestFailureException(f"Triangle Size Draw and Pass do not match: {pickedDraw.floatValue} vs {pickedPass.floatValue}")

    def check_overlay_and_pixel_history(self, eid: int, coords: List[Tuple[int,int]]):
        self.set_event(eid, False)
        for c in coords:
            x = c[0]
            y = c[1]
            self.check_overlays(eid, x, y)
            self.check_pixel_history_succeeds(eid, x, y)

    def check_overlays_and_pixel_history(self):
        for level in ["Primary", "Secondary"]:
            action = self.find_action(f"{level}: Indirect draws")

            # vkCmdDrawIndirect : 1 
            action = self.find_action("vkCmdDrawIndirect", action.eventId)
            eid = action.eventId
            coords = [(60, 60)]
            self.check_overlay_and_pixel_history(eid, coords)

            # vkCmdDrawIndexedIndirect : 2 
            action = self.find_action("vkCmdDrawIndexedIndirect", action.eventId)
            eid = action.eventId
            # Draw Action 1
            eid += 1
            coords = [(100, 60)]
            self.check_overlay_and_pixel_history(eid, coords)
            # Draw Action 2
            eid += 1
            coords = [(140, 40), (200, 40)]
            self.check_overlay_and_pixel_history(eid, coords)

            action = self.find_action(f"{level}: Indirect count draws Three & Three")
            if action:
                # vkCmdDrawIndirectCount : 3 
                action = self.find_action("vkCmdDrawIndirectCount", action.eventId)
                eid = action.eventId
                # Draw Action 1
                eid += 1
                coords = [(230, 250)]
                self.check_overlay_and_pixel_history(eid, coords)
                # Draw Action 2
                eid += 1
                coords = [(250, 180)]
                self.check_overlay_and_pixel_history(eid, coords)
                # Draw Action 3
                eid += 1
                coords = [(270, 170)]
                self.check_overlay_and_pixel_history(eid, coords)

                # vkCmdDrawIndexedIndirectCount : 3 
                action = self.find_action("vkCmdDrawIndexedIndirectCount", action.eventId)
                eid = action.eventId
                # Draw Action 1
                eid += 1
                coords = [(250, 250)]
                self.check_overlay_and_pixel_history(eid, coords)
                # Draw Action 2 : no draw
                eid += 1
                # Draw Action 3
                eid += 1
                coords = [(270, 245),(300,245)]
                self.check_overlay_and_pixel_history(eid, coords)

            action = self.find_action(f"{level}: KHR_draw_indirect_count")
            if action:
                # Primary: Indirect count draws
                action = self.find_action(f"{level}: Indirect count draws", action.eventId)
                # vkCmdDrawIndirectCount : 1 
                action = self.find_action("vkCmdDrawIndirectCount", action.eventId)
                eid = action.eventId
                # Draw Action 1
                eid += 1
                coords = [(60, 200)]
                self.check_overlay_and_pixel_history(eid, coords)
                # vkCmdDrawIndexedIndirectCount : 3 
                action = self.find_action("vkCmdDrawIndexedIndirectCount", action.eventId)
                eid = action.eventId
                # Draw Action 1
                eid += 1
                coords = [(100, 200)]
                self.check_overlay_and_pixel_history(eid, coords)
                # Draw Action 2 : no draw
                eid += 1
                # Draw Action 3
                eid += 1
                coords = [(140, 190),(200,190)]
                self.check_overlay_and_pixel_history(eid, coords)

            # Primary : Post-count 1
            action = self.find_action(f"{level}: Post-count 1")
            # vkCmdDraw : 1
            action = self.find_action("vkCmdDraw", action.eventId)
            eid = action.eventId
            coords = [(340, 60)]
            self.check_overlay_and_pixel_history(eid, coords)

            # Primary : Post-count 2
            action = self.find_action(f"{level}: Post-count 2")
            # vkCmdDraw : 1
            action = self.find_action("vkCmdDraw", action.eventId)
            eid = action.eventId
            coords = [(340, 200)]
            self.check_overlay_and_pixel_history(eid, coords)

            # Primary : Post-count 3
            action = self.find_action(f"{level}: Post-count 3")
            # vkCmdDraw : 1
            action = self.find_action("vkCmdDraw", action.eventId)
            eid = action.eventId
            coords = [(340, 140)]
            self.check_overlay_and_pixel_history(eid, coords)

    def check_empty_draw_overlays(self):
        with rdtest.log.auto_section('Checking Empty Draws'):
            for level in ["Primary", "Secondary"]:
                empties = self.find_action(f"{level}: Empty count draws")
                assert empties is not None
                for action in real_action_children(empties):
                    eid = action.eventId
                    self.set_event(eid, False)
                    pipe = self.controller.GetPipelineState()
                    for overlay in rd.DebugOverlay:
                        if overlay == rd.DebugOverlay.NoOverlay:
                            continue
                        if overlay == rd.DebugOverlay.NaN or overlay == rd.DebugOverlay.Clipping:
                            continue
                        if overlay == rd.DebugOverlay.ViewportScissor:
                            continue
                        if overlay == rd.DebugOverlay.Wireframe:
                            continue
                        tex = rd.TextureDisplay()
                        col_tex = pipe.GetOutputTargets()[0].resource
                        tex.resourceId = col_tex
                        tex.overlay = overlay
                        tex.subresource.sample = 0
                        out = self.controller.CreateOutput(rd.CreateHeadlessWindowingData(100, 100), rd.ReplayOutputType.Texture)
                        out.SetTextureDisplay(tex)
                        out.Display()
                        overlayTex = out.GetDebugOverlayTexID()
                        expectEmpty = True
                        if overlay == rd.DebugOverlay.ClearBeforeDraw:
                            overlayTex = col_tex
                        if overlay == rd.DebugOverlay.ClearBeforePass:
                            overlayTex = col_tex
                        if overlay == rd.DebugOverlay.ClearBeforePass:
                            expectEmpty = False
                        if overlay == rd.DebugOverlay.QuadOverdrawPass:
                            expectEmpty = False
                        if overlay == rd.DebugOverlay.TriangleSizePass:
                            expectEmpty = False

                        empty = True
                        emptyPixel = (0.0, 0.0, 0.0, 0.0)
                        if overlay == rd.DebugOverlay.Drawcall:
                            emptyPixel = (0.0, 0.0, 0.0, 0.5)

                        for s in self.samples:
                            x = s[0]
                            y = s[1]
                            picked = self.controller.PickPixel(overlayTex, x, y, rd.Subresource(), rd.CompType.Typeless)
                            if picked.floatValue != emptyPixel:
                                empty = False
                            if expectEmpty and not empty:
                                raise rdtest.TestFailureException(f"EID {eid} {overlay.name} {x}, {y} {picked.floatValue} is not as expected {emptyPixel}")
                        if expectEmpty != empty:
                            raise rdtest.TestFailureException(f"EID {eid} {overlay.name} is not as expected")

                        out.Shutdown()

    def check_capture(self):
        assert self.controller is not None

        with rdtest.log.auto_section("Checking Indirect Action Names"):
            if not self.check_indirect_action_name_consistency(self.controller):
                raise rdtest.TestFailureException("Indirect action parameters do not match its event parameters")

        fill = self.find_action("vkCmdFillBuffer")

        assert fill is not None

        buffer_usage: Dict[int, List[rd.ResourceUsage]] = {}

        for usage in self.controller.GetUsage(fill.copyDestination):
            if usage.eventId not in buffer_usage:
                buffer_usage[usage.eventId] = []
            buffer_usage[usage.eventId].append(usage.usage)

        # The texture is the backbuffer
        tex = self.get_last_action().copyDestination

        for level in ["Primary", "Secondary"]:
            rdtest.log.print(f"Checking {level} indirect calls")

            final = self.find_action(f"{level}: Final")

            indirect_count_root = self.find_action(f"{level}: KHR_draw_indirect_count")

            self.set_event(final.eventId, False)

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

            dispatches = self.find_action(f"{level}: Dispatches")

            # Set up a ReplayOutput and TextureSave for quickly testing the action highlight overlay
            self.out = self.controller.CreateOutput(
                rd.CreateHeadlessWindowingData(100, 100), rd.ReplayOutputType.Texture
            )

            assert self.out is not None

            # Rewind to the start of the capture
            action = dispatches.children[0]
            while action.previousAction is not None:
                action = action.previousAction

            # Ensure we can select all actions
            while action is not None:
                self.set_event(action.eventId, False)
                action = action.nextAction

            rdtest.log.success(f"Selected all {level} actions")

            assert dispatches and len(real_action_children(dispatches)) == 3

            assert dispatches.children[0].dispatchDimension == (0, 0, 0)
            assert dispatches.children[1].dispatchDimension == (1, 1, 1)
            assert dispatches.children[2].dispatchDimension == (3, 4, 5)

            rdtest.log.success(f"{level} Indirect dispatches are the correct dimensions")

            self.set_event(dispatches.children[2].eventId, False)

            pipe = self.controller.GetPipelineState()

            ssbo = pipe.GetReadWriteResources(rd.ShaderStage.Compute)[0].descriptor
            data = self.controller.GetBufferData(ssbo.resource, 0, 0)

            rdtest.log.print(f"Got {len(data)} bytes of uints")

            uints = [struct.unpack_from('=4L', data, offs) for offs in range(0, len(data), 16)]

            for x in range(0, 6):  # 3 groups of 2 threads each
                for y in range(0, 8):  # 3 groups of 2 threads each
                    for z in range(0, 5):  # 5 groups of 1 thread each
                        idx = 100 + z*8*6 + y*6 + x
                        if not rdtest.value_compare(uints[idx], [x, y, z, 12345]):
                            raise rdtest.TestFailureException(
                                f'expected thread index data @ {x},{y},{z}: {uints[idx]} is not as expected: {[x, y, z, 12345]}')

            rdtest.log.success(f"Dispatched buffer contents are as expected for {level}")

            empties = self.find_action(f"{level}: Empty draws")

            assert empties and len(real_action_children(empties)) == 2

            for action in real_action_children(empties):
                assert action.numIndices == 0
                assert action.numInstances == 0

                self.set_event(action.eventId, False)

                # Check that we have empty PostVS
                postvs_data = self.get_postvs(action, rd.MeshDataStage.VSOut, 0, 1)
                assert len(postvs_data) == 0

                # No samples should be passing in the empties
                self.check_overlay([])

            rdtest.log.success(f"{level} empty actions are empty")

            indirects = self.find_action(f"{level}: Indirect draws")

            assert 'vkCmdDrawIndirect' in indirects.children[0].customName
            assert 'vkCmdDrawIndexedIndirect' in indirects.children[1].customName
            assert len(real_action_children(indirects.children[1])) == 2

            rdtest.log.success(f"Correct number of {level} indirect draws")

            # vkCmdDrawIndirect(...)
            action = indirects.children[0]
            assert action.numIndices == 3
            assert action.numInstances == 2

            self.set_event(action.eventId, False)

            assert rd.ResourceUsage.Indirect in buffer_usage[action.eventId]

            # Check that we have PostVS as expected
            postvs_data = self.get_postvs(action, rd.MeshDataStage.VSOut)

            postvs_ref: rdtest.MeshReference = {
                0: {'vtx': 0, 'idx': 0, 'gl_Position': [-0.8, -0.5, 0.0, 1.0]},
                1: {'vtx': 1, 'idx': 1, 'gl_Position': [-0.7, -0.8, 0.0, 1.0]},
                2: {'vtx': 2, 'idx': 2, 'gl_Position': [-0.6, -0.5, 0.0, 1.0]},
            }

            self.check_mesh_data(postvs_ref, postvs_data)
            assert len(postvs_data) == len(postvs_ref)  # We shouldn't have any extra vertices

            self.check_overlay([(60, 40)])

            rdtest.log.success(f"{level} {action.customName} is as expected")

            assert rd.ResourceUsage.Indirect in buffer_usage[indirects.children[1].eventId]

            # vkCmdDrawIndexedIndirect[0](...)
            action = indirects.children[1].children[0]
            assert action.numIndices == 3
            assert action.numInstances == 3

            self.set_event(action.eventId, False)

            # Check that we have PostVS as expected
            postvs_data = self.get_postvs(action, rd.MeshDataStage.VSOut)

            # These indices are the *output* indices, which have been rebased/remapped, so are not the same as the input
            # indices
            postvs_ref: rdtest.MeshReference = {
                0: {'vtx': 0, 'idx': 6, 'gl_Position': [-0.6, -0.5, 0.0, 1.0]},
                1: {'vtx': 1, 'idx': 7, 'gl_Position': [-0.5, -0.8, 0.0, 1.0]},
                2: {'vtx': 2, 'idx': 8, 'gl_Position': [-0.4, -0.5, 0.0, 1.0]},
            }

            self.check_mesh_data(postvs_ref, postvs_data)
            assert len(postvs_data) == len(postvs_ref)  # We shouldn't have any extra vertices

            self.check_overlay([(100, 40)])

            rdtest.log.success(f"{level} {action.customName} is as expected")

            # vkCmdDrawIndexedIndirect[1](...)
            action = indirects.children[1].children[1]
            assert action.numIndices == 6
            assert action.numInstances == 2

            self.set_event(action.eventId, False)

            # Check that we have PostVS as expected
            postvs_data = self.get_postvs(action, rd.MeshDataStage.VSOut)

            postvs_ref: rdtest.MeshReference = {
                0: {'vtx': 0, 'idx': 9, 'gl_Position': [-0.4, -0.5, 0.0, 1.0]},
                1: {'vtx': 1, 'idx': 10, 'gl_Position': [-0.3, -0.8, 0.0, 1.0]},
                2: {'vtx': 2, 'idx': 11, 'gl_Position': [-0.2, -0.8, 0.0, 1.0]},

                3: {'vtx': 3, 'idx': 12, 'gl_Position': [-0.1, -0.5, 0.0, 1.0]},
                4: {'vtx': 4, 'idx': 13, 'gl_Position': [ 0.0, -0.8, 0.0, 1.0]},
                5: {'vtx': 5, 'idx': 14, 'gl_Position': [ 0.1, -0.8, 0.0, 1.0]},
            }

            self.check_mesh_data(postvs_ref, postvs_data)
            assert len(postvs_data) == len(postvs_ref)  # We shouldn't have any extra vertices

            self.check_overlay([(140, 40), (200, 40)])

            rdtest.log.success(f"{level} {action.customName} is as expected")

            if indirect_count_root is not None:
                rdtest.log.print(f"Testing {indirect_count_root.customName}")
                assert indirect_count_root.children[0].customName == f'{level}: Empty count draws'
                assert indirect_count_root.children[1].customName == f'{level}: Indirect count draws'

                empties = indirect_count_root.children[0]

                assert empties and len(real_action_children(empties)) == 3

                for action in real_action_children(empties):
                    assert action.numIndices == 0
                    assert action.numInstances == 0

                    self.set_event(action.eventId, False)

                    # Check that we have empty PostVS
                    postvs_data = self.get_postvs(action, rd.MeshDataStage.VSOut, 0, 1)
                    assert len(postvs_data) == 0

                    self.check_overlay([], no_overlay=True)

                # vkCmdDrawIndirectCountKHR
                action_indirect = indirect_count_root.children[1].children[0]

                assert rd.ResourceUsage.Indirect in buffer_usage[action_indirect.eventId]

                assert action_indirect and len(real_action_children(action_indirect)) == 1

                # vkCmdDrawIndirectCountKHR[0]
                action = action_indirect.children[0]

                assert action.numIndices == 3
                assert action.numInstances == 4

                self.set_event(action.eventId, False)

                # Check that we have PostVS as expected
                postvs_data = self.get_postvs(action, rd.MeshDataStage.VSOut)

                # These indices are the *output* indices, which have been rebased/remapped, so are not the same as the input
                # indices
                postvs_ref: rdtest.MeshReference = {
                    0: {'vtx': 0, 'idx': 0, 'gl_Position': [-0.8, 0.5, 0.0, 1.0]},
                    1: {'vtx': 1, 'idx': 1, 'gl_Position': [-0.7, 0.2, 0.0, 1.0]},
                    2: {'vtx': 2, 'idx': 2, 'gl_Position': [-0.6, 0.5, 0.0, 1.0]},
                }

                self.check_mesh_data(postvs_ref, postvs_data)
                assert len(postvs_data) == len(postvs_ref)  # We shouldn't have any extra vertices

                self.check_overlay([(60, 190)])

                rdtest.log.success(f"{level} {action.customName} is as expected")

                # vkCmdDrawIndexedIndirectCountKHR
                action_indirect = indirect_count_root.children[1].children[1]

                assert action_indirect and len(real_action_children(action_indirect)) == 3

                # vkCmdDrawIndirectCountKHR[0]
                action = action_indirect.children[0]
                assert action.numIndices == 3
                assert action.numInstances == 1

                self.set_event(action.eventId, False)

                # Check that we have PostVS as expected
                postvs_data = self.get_postvs(action, rd.MeshDataStage.VSOut)

                # These indices are the *output* indices, which have been rebased/remapped, so are not the same as the input
                # indices
                postvs_ref: rdtest.MeshReference = {
                    0: {'vtx': 0, 'idx': 15, 'gl_Position': [-0.6, 0.5, 0.0, 1.0]},
                    1: {'vtx': 1, 'idx': 16, 'gl_Position': [-0.5, 0.2, 0.0, 1.0]},
                    2: {'vtx': 2, 'idx': 17, 'gl_Position': [-0.4, 0.5, 0.0, 1.0]},
                }

                self.check_mesh_data(postvs_ref, postvs_data)
                assert len(postvs_data) == len(postvs_ref)  # We shouldn't have any extra vertices

                self.check_overlay([(100, 190)])

                rdtest.log.success(f"{level} {action.customName} is as expected")

                # vkCmdDrawIndirectCountKHR[1]
                action = action_indirect.children[1]
                assert action.numIndices == 0
                assert action.numInstances == 0

                self.set_event(action.eventId, False)

                postvs_data = self.get_postvs(action, rd.MeshDataStage.VSOut)

                assert len(postvs_data) == 0

                self.check_overlay([])

                rdtest.log.success(f"{level} {action.customName} is as expected")

                # vkCmdDrawIndirectCountKHR[2]
                action = action_indirect.children[2]
                assert action.numIndices == 6
                assert action.numInstances == 2

                self.set_event(action.eventId, False)

                # Check that we have PostVS as expected
                postvs_data = self.get_postvs(action, rd.MeshDataStage.VSOut)

                # These indices are the *output* indices, which have been rebased/remapped, so are not the same as the input
                # indices
                postvs_ref: rdtest.MeshReference = {
                    0: {'vtx': 0, 'idx': 18, 'gl_Position': [-0.4, 0.5, 0.0, 1.0]},
                    1: {'vtx': 1, 'idx': 19, 'gl_Position': [-0.3, 0.2, 0.0, 1.0]},
                    2: {'vtx': 2, 'idx': 20, 'gl_Position': [-0.2, 0.2, 0.0, 1.0]},

                    3: {'vtx': 3, 'idx': 21, 'gl_Position': [-0.1, 0.5, 0.0, 1.0]},
                    4: {'vtx': 4, 'idx': 22, 'gl_Position': [ 0.0, 0.2, 0.0, 1.0]},
                    5: {'vtx': 5, 'idx': 23, 'gl_Position': [ 0.1, 0.2, 0.0, 1.0]},
                }

                self.check_mesh_data(postvs_ref, postvs_data)
                assert len(postvs_data) == len(postvs_ref)  # We shouldn't have any extra vertices

                self.check_overlay([(140, 190), (200, 190)])

                rdtest.log.success(f"{level} {action.customName} is as expected")

                # Now check that the draws post-count are correctly highlighted
                self.set_event(self.find_action(f"{level}: Post-count 1").children[0].eventId, False)
                self.check_overlay([(340, 40)])
                self.set_event(self.find_action(f"{level}: Post-count 2").children[0].eventId, False)
                self.check_overlay([(340, 190)])
                self.set_event(self.find_action(f"{level}: Post-count 3").children[0].eventId, False)
                self.check_overlay([(340, 115)])
            else:
                rdtest.log.print("KHR_draw_indirect_count not tested")

        with rdtest.log.auto_section('Checking Overlays And Pixel History'):
            self.check_overlays_and_pixel_history()

        with rdtest.log.auto_section('Checking All Overlays'):
            for eid in range(self.get_first_action().eventId, self.get_last_action().eventId + 1):
                self.set_event(eid, False)
                pipe = self.controller.GetPipelineState()
                if len(pipe.GetOutputTargets()) == 0:
                    continue
                rdtest.log.print(f"EID: {eid}")
                for overlay in rd.DebugOverlay:
                    tex = rd.TextureDisplay()
                    col_tex = pipe.GetOutputTargets()[0].resource
                    tex.resourceId = col_tex
                    tex.overlay = overlay
                    tex.subresource.sample = 0

                    out = self.controller.CreateOutput(rd.CreateHeadlessWindowingData(100, 100), rd.ReplayOutputType.Texture)
                    out.SetTextureDisplay(tex)
                    out.Display()
                    out.Shutdown()

        self.check_empty_draw_overlays()
