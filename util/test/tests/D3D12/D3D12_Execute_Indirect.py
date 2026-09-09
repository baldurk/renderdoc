import renderdoc as rd
import rdtest
import struct
from typing import List

class D3D12_Execute_Indirect(rdtest.TestCase):
    demos_test_name = 'D3D12_Execute_Indirect'

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

                picked = self.controller.PickPixel(overlayTex, x, y, rd.Subresource(), rd.CompType.UNorm)
                emptyPixel = (0.0, 0.0, 0.0, 0.0)
                if picked.floatValue == emptyPixel:
                    raise rdtest.TestFailureException(f"{overlay.name} overlay is empty")
                out.Shutdown()

    def check_pixel_history_succeeds(self, x: int, y: int):
        pipe = self.controller.GetPipelineState()
        rt = pipe.GetOutputTargets()[0]
        tex = rt.resource
        sub = rd.Subresource()
        modifs = self.controller.PixelHistory(tex, x, y, sub, rt.format.compType)
        if len(modifs) < 2:
            raise rdtest.TestFailureException(f"No pixel history found at ({x}, {y})")
        rdtest.log.success(f"Pixel History {x}, {y} Worked")

    def check_root_consts(self, expected: List[float]):
        pipe = self.controller.GetPipelineState()
        root_consts = pipe.GetConstantBlock(rd.ShaderStage.Vertex, 1, 0)
        if root_consts.descriptor.resource == rd.ResourceId():
            raise rdtest.TestFailureException('rootConsts not found in pipeline state')
        bytes = self.controller.GetBufferData(root_consts.descriptor.resource, 0, root_consts.descriptor.byteSize)   
        assert len(bytes) == 16  # 4 floats
        data = struct.unpack("ffff", bytes)
        for i in range(len(expected)):
            if not rdtest.value_compare(expected[i], data[i]):
                raise rdtest.TestFailureException(f'rootConsts[i] does not match expected:{expected[i]} got:{data[i]}')
        rdtest.log.success("rootConsts is as expected")

    def check_capture(self):

        with rdtest.log.auto_section("Checking Indirect Action Names"):
            if not self.check_indirect_action_name_consistency(self.controller):
                raise rdtest.TestFailureException("Indirect action parameters do not match its event parameters")

        with rdtest.log.auto_section('EI without Root Signature'):
            action = self.find_action("EI without Root Signature");
            self.controller.SetFrameEvent(action.eventId, False)
            action = self.find_action("IndirectDraw", action.eventId)
            for drawNum in range(3):
                self.controller.SetFrameEvent(action.eventId + drawNum, False)
                pipe = self.controller.GetPipelineState()
                if len(pipe.GetOutputTargets()) != 1:
                    raise rdtest.TestFailureException(
                        f"With event {action.eventId + drawNum} selected we should have one output target but there is {len(pipe.GetOutputTargets())}")
                self.check_pixel_history_succeeds(285, 110)
                if drawNum == 0:
                    self.check_overlays(action.eventId, 285, 110)
        
        with rdtest.log.auto_section('Multiple draws'):
            from_eid = self.find_action("Multiple draws").eventId
            ei_eid = self.find_action("ExecuteIndirect", from_eid).eventId
            self.controller.SetFrameEvent(ei_eid - 1, False)
            self.check_root_consts([10.0, 9.0, 8.0, 7.0])
            viewX = 0
            viewY = 0
            sqSize = 300 / 4
            viewW = sqSize
            viewH = sqSize
            for i in range(8):
                action = self.find_action("IndirectDraw", from_eid)
                eid = action.eventId
                self.controller.SetFrameEvent(eid, False)
                self.check_root_consts([123.0, 9.0, 8.0, 7.0])

                # Should be a green triangle in the centre of the screen on a black background
                self.check_triangle(back=[0.0, 0.0, 0.0, 1.0], vp=[viewX, viewY, viewW, viewH])

                vsin_ref = {
                    0: {
                        'vtx': 0,
                        'idx': 0,
                        'POSITION': [-0.5, -0.5, 0.0, 0.0],
                        'COLOR': [0.0, 1.0, 0.0, 1.0],
                    },
                    1: {
                        'vtx': 1,
                        'idx': 1,
                        'POSITION': [0.0, 0.5, 0.0, 0.0],
                        'COLOR': [0.0, 1.0, 0.0, 1.0],
                    },
                    2: {
                        'vtx': 2,
                        'idx': 2,
                        'POSITION': [0.5, -0.5, 0.0, 0.0],
                        'COLOR': [0.0, 1.0, 0.0, 1.0],
                    },
                }
                self.check_mesh_data(vsin_ref, self.get_vsin(action))
                postvs_data = self.get_postvs(action, rd.MeshDataStage.VSOut, 0, action.numIndices)
                postvs_ref = {
                    0: {
                        'vtx': 0,
                        'idx': 0,
                        'SV_POSITION': [-0.5, -0.5, 0.0, 1.0],
                        'COLOR': [0.0, 1.0, 0.0, 1.0],
                    },
                    1: {
                        'vtx': 1,
                        'idx': 1,
                        'SV_POSITION': [0.0, 0.5, 0.0, 1.0],
                        'COLOR': [0.0, 1.0, 0.0, 1.0],
                    },
                    2: {
                        'vtx': 2,
                        'idx': 2,
                        'SV_POSITION': [0.5, -0.5, 0.0, 1.0],
                        'COLOR': [0.0, 1.0, 0.0, 1.0],
                    },
                }
                self.check_mesh_data(postvs_ref, postvs_data)

                x = int(viewX + viewW/2)
                y = int(viewY + viewH/2)
                self.check_pixel_history_succeeds(x,y)
                self.check_overlays(eid, x,y)

                from_eid = eid + 1

                pipe = self.controller.GetPipelineState()

                vbs = pipe.GetVBuffers()
                ro = pipe.GetReadOnlyResources(rd.ShaderStage.Vertex)
                rw = pipe.GetReadWriteResources(rd.ShaderStage.Vertex)
                assert vbs[0].resourceId != rd.ResourceId()
                assert ro[0].descriptor.resource != rd.ResourceId()
                assert rw[0].descriptor.resource != rd.ResourceId()
                assert pipe.GetConstantBlock(rd.ShaderStage.Vertex, 0, 0).descriptor.resource != rd.ResourceId()

                viewX += sqSize
                if viewX + sqSize >= 400:
                    viewX = 0
                    viewY += sqSize

        with rdtest.log.auto_section('State is reset after execute'):
            viewX = 0
            viewY = 0
            action = self.find_action("Post draw")
            self.controller.SetFrameEvent(action.eventId, False)

            # triangle should still be visible
            self.check_triangle(back=[0.0, 0.0, 0.0, 1.0], vp=[viewX, viewY, viewW, viewH])

            # but state should be reset
            pipe = self.controller.GetPipelineState()

            vbs = pipe.GetVBuffers()
            ro = pipe.GetReadOnlyResources(rd.ShaderStage.Vertex)
            rw = pipe.GetReadWriteResources(rd.ShaderStage.Vertex)
            assert len(vbs) == 0 or vbs[0].resourceId == rd.ResourceId()
            assert len(ro) == 0 or ro[0].descriptor.resource == rd.ResourceId()
            assert len(rw) == 0 or rw[0].descriptor.resource == rd.ResourceId()
            assert pipe.GetConstantBlock(rd.ShaderStage.Vertex, 0, 0).descriptor.resource == rd.ResourceId()

        self.check_pixel_history_succeeds(185, 50)

        with rdtest.log.auto_section('Dispatch buffer output is correct'):
            action = self.find_action("Post Single dispatch")
            self.controller.SetFrameEvent(action.eventId, False)

            pipe = self.controller.GetPipelineState()
            rw = pipe.GetReadWriteResources(rd.ShaderStage.Compute)

            for z in range(10):
                for y in range(30):
                    for x in range(12):
                        idx = z*30*12+y*12+x
                        value = struct.unpack_from('4f', self.controller.GetBufferData(rw[0].descriptor.resource, 16*idx, 16))
                        expect = [float(x), float(y), float(z), float(idx)]

                        if not rdtest.value_compare(expect, value):
                            raise rdtest.TestFailureException(
                                f"buffer at {x},{y},{z}: {value} doesn't match expected {expect}")

        self.check_pixel_history_succeeds(185, 50)

        # The final draw is in indeterminate order because the parameters are defined by a compute shader in
        # indeterminate order
        # If the vertex buffer is referenced in the wrong order (e.g. by cached draw parameters) it will show exploding
        # polygons. To check the behaviour we can't require any given draw appear in any given place. However each
        # time we replay it should be self-consistent - after selecting the 4th draw then eactly 4 draws should appear.
        # And of course no exploding polys!

        with rdtest.log.auto_section('Custom order draw'):
            action = self.find_action("Custom order draw")
            action = self.find_action("IndirectDraw", action.eventId)

            drawPoints = [
                (310, 78),
                (338, 78),
                (367, 78),

                (310, 107),
                (367, 107),

                (310, 135),
                (338, 135),
                (367, 135),
            ]

            sdfile = self.controller.GetStructuredFile()

            # do N passes since it will be unpredictable
            for passNum in range(50):
                for drawNum in range(8):
                    self.controller.SetFrameEvent(action.eventId + drawNum, False)

                    pipe = self.controller.GetPipelineState()
                    out = pipe.GetOutputTargets()[0].resource

                    count = 0
                    draws = []
                    for i, p in enumerate(drawPoints):
                        picked = self.controller.PickPixel(out, p[0], p[1], rd.Subresource(), rd.CompType.UNorm)
                        if rdtest.value_compare(picked.floatValue, [0.0, 1.0, 0.0, 1.0]):
                            count += 1
                            draws += [i]

                    if not rdtest.value_compare(drawNum + 1, count):
                        raise rdtest.TestFailureException(
                            f"With {action.GetName(sdfile)} selected we should have {drawNum + 1} draws, but counted {count} draws")

                    rdtest.log.print(f"With draw #{drawNum} selected we saw draws {draws!s} active")

                    # the exploded verts are calibrated to render as purple. We don't handle the case where exploding polys
                    # reference vertices from other draws, but this _should_ not happen as we leave a large margin between
                    # each draw's segments

                    data = self.controller.GetTextureData(out, rd.Subresource(0, 0, 0))
                    tex = self.get_texture(out)
                    rdtest.log.print(f"{len(data)} - {tex.width} {tex.height} ")
                    pixels = [struct.unpack_from("4B", data, 4 * p) for p in range(int(tex.width * tex.height))]
                    unique_pixels = list(set(pixels))

                    if (255, 0, 255, 255) in unique_pixels:
                        raise rdtest.TestFailureException(
                            f"Detected an exploded polygon with {action.GetName(sdfile)} selected")

                    self.check_pixel_history_succeeds(185, 50)

                rdtest.log.success(f"Pass {passNum} of unordered draw was correct")

        # This does not draw anything but its argument buffer is fully used with no spare bytes
        # Iterate over every draw and check the replay has valid output target
        with rdtest.log.auto_section('Fully used argument buffer with multiple draws replayed'):
            action = self.find_action("Full Arg Buffer")
            action = self.find_action("IndirectDraw", action.eventId)
            for drawNum in range(3):
                self.controller.SetFrameEvent(action.eventId + drawNum, False)
                pipe = self.controller.GetPipelineState()
                if len(pipe.GetOutputTargets()) != 1:
                    raise rdtest.TestFailureException(
                        f"With event {action.eventId + drawNum} selected we should have one output target but there is {len(pipe.GetOutputTargets())}")
                self.check_pixel_history_succeeds(185, 50)

        # This does not draw anything but its argument buffer is fully used with no spare bytes
        # Iterate over every draw and check the replay has valid output target
        with rdtest.log.auto_section('Fully used argument buffer with multiple states + draws replayed'):
            action = self.find_action("Full Arg Buffer: State + Draw")
            action = self.find_action("IndirectDraw", action.eventId)
            for drawNum in range(3):
                eid = action.eventId
                self.controller.SetFrameEvent(eid, False)
                pipe = self.controller.GetPipelineState()
                if len(pipe.GetOutputTargets()) != 1:
                    raise rdtest.TestFailureException(
                        f"With event {eid} selected we should have one output target but there is {len(pipe.GetOutputTargets())}")
                x = 100
                y = 210 - drawNum * 20
                self.check_overlays(eid, x, y)
                self.check_pixel_history_succeeds(x, 210)
                if drawNum > 0:
                    self.check_pixel_history_succeeds(x, 190)
                if drawNum > 1:
                    self.check_pixel_history_succeeds(x, 170)
                action = action.nextAction

        with rdtest.log.auto_section('Checking Count Buffer Draws'):
            base = self.find_action("Count Buffer Draws")
            tests = [
                ("MaxCount: 1024 CountBuf: 256", 256, 170),
                ("MaxCount: 1 CountBuf: 7", 1, 250),
                ("MaxCount: 0 CountBuf: 11", 0, 0)
            ]
            for test in tests:
                marker = test[0]
                expectedDraws = test[1] 
                xpos = test[2]
                with rdtest.log.auto_section(f'Checking "{marker}"'):
                    markerAction = self.find_action(marker, base.eventId);
                    executeAction = self.find_action("ExecuteIndirect", markerAction.eventId);
                    # Exclude the ExecuteIndirect end marker
                    countDraws = len(executeAction.children) - 1
                    self.check_eq(countDraws, expectedDraws)
                    action = self.find_action("IndirectDraw", executeAction.eventId)
                    for drawNum in range(min(countDraws, 12)):
                        eid = action.eventId
                        self.controller.SetFrameEvent(eid, False)
                        pipe = self.controller.GetPipelineState()
                        if len(pipe.GetOutputTargets()) != 1:
                            raise rdtest.TestFailureException(
                                f"With event {eid} selected we should have one output target but there is {len(pipe.GetOutputTargets())}")
                        drawNum = drawNum % 6
                        x = xpos
                        y = 210 - drawNum * 20
                        if drawNum > 2:
                            x = xpos + 30
                            y = 165 + (drawNum - 3) * 20

                        self.check_overlays(eid, x, y)
                        self.check_pixel_history_succeeds(xpos, 210)
                        if drawNum > 0:
                            self.check_pixel_history_succeeds(xpos, 190)
                        if drawNum > 1:
                            self.check_pixel_history_succeeds(xpos, 170)
                        if drawNum > 2:
                            self.check_pixel_history_succeeds(x, 165)
                        if drawNum > 3:
                            self.check_pixel_history_succeeds(x, 185)
                        if drawNum > 4:
                            self.check_pixel_history_succeeds(x, 205)
                        action = action.nextAction

        with rdtest.log.auto_section('Two Single Draws QuadOverdraw (Pass) replayed correctly'):
            for drawNum in range(2):
                action = self.find_action("Two Single Draws")
                action = self.find_action("IndirectDraw", action.eventId)
                if drawNum == 1:
                    action = self.find_action("IndirectDraw", action.eventId+1)

                eid = action.eventId
                self.controller.SetFrameEvent(action.eventId, False)
                pipe = self.controller.GetPipelineState()
                if len(pipe.GetOutputTargets()) != 1:
                    raise rdtest.TestFailureException(
                        f"With event {action.eventId} selected we should have one output target but there is {len(pipe.GetOutputTargets())}")
                x = 50 + 80 * drawNum
                y = 275
                self.check_pixel_history_succeeds(x, y)
                self.check_overlays(eid, x, y)

                overlay = rd.DebugOverlay.QuadOverdrawPass
                tex = rd.TextureDisplay()
                col_tex = pipe.GetOutputTargets()[0].resource
                tex.resourceId = col_tex
                tex.overlay = overlay
                tex.subresource.sample = 0

                outw = self.controller.CreateOutput(rd.CreateHeadlessWindowingData(100, 100), rd.ReplayOutputType.Texture)
                outw.SetTextureDisplay(tex)
                outw.Display()
                outw.Shutdown()

        largeEIMarker = self.find_action("MaxCount: 1024 CountBuf: 256")
        skipEIDmin = largeEIMarker.eventId + 10
        largeEIAction = self.find_action("ExecuteIndirect", largeEIMarker.eventId);
        skipEIDmax = largeEIAction.eventId + len(largeEIAction.children) - 10

        with rdtest.log.auto_section('Checking All Overlays'):
            for eid in range(self.get_first_action().eventId, self.get_last_action().eventId + 1):
                if eid >= skipEIDmin and eid <= skipEIDmax:
                    continue
                self.controller.SetFrameEvent(eid, False)
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

                    outw = self.controller.CreateOutput(rd.CreateHeadlessWindowingData(100, 100), rd.ReplayOutputType.Texture)
                    outw.SetTextureDisplay(tex)
                    outw.Display()
                    outw.Shutdown()
