import renderdoc as rd
import rdtest
import struct


class D3D12_Execute_Indirect(rdtest.TestCase):
    demos_test_name = 'D3D12_Execute_Indirect'

    def check_capture(self):
        from_eid = self.find_action("Multiple draws").eventId
        for i in range(8):
            action = self.find_action("IndirectDraw", from_eid)
            self.controller.SetFrameEvent(action.eventId, False)

            self.check_triangle(back=[0.0, 0.0, 0.0, 1.0])

            # Should be a green triangle in the centre of the screen on a black background
            self.check_triangle(back=[0.0, 0.0, 0.0, 1.0])
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

            from_eid = action.eventId + 1

            pipe = self.controller.GetPipelineState()

            vbs = pipe.GetVBuffers()
            ro = pipe.GetReadOnlyResources(rd.ShaderStage.Vertex)
            rw = pipe.GetReadWriteResources(rd.ShaderStage.Vertex)
            self.check(vbs[0].resourceId != rd.ResourceId())
            self.check(ro[0].descriptor.resource != rd.ResourceId())
            self.check(rw[0].descriptor.resource != rd.ResourceId())
            self.check(pipe.GetConstantBlock(rd.ShaderStage.Vertex, 0, 0).descriptor.resource != rd.ResourceId())

        action = self.find_action("Post draw")
        self.controller.SetFrameEvent(action.eventId, False)

        # triangle should still be visible
        self.check_triangle(back=[0.0, 0.0, 0.0, 1.0])

        # but state should be reset
        pipe = self.controller.GetPipelineState()

        vbs = pipe.GetVBuffers()
        ro = pipe.GetReadOnlyResources(rd.ShaderStage.Vertex)
        rw = pipe.GetReadWriteResources(rd.ShaderStage.Vertex)
        self.check(len(vbs) == 0 or vbs[0].resourceId == rd.ResourceId())
        self.check(len(ro) == 0 or ro[0].descriptor.resource == rd.ResourceId())
        self.check(len(rw) == 0 or rw[0].descriptor.resource == rd.ResourceId())
        self.check(pipe.GetConstantBlock(rd.ShaderStage.Vertex, 0, 0).descriptor.resource == rd.ResourceId())

        rdtest.log.success("State is reset after execute")

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
                            "buffer at {},{},{}: {} doesn't match expected {}".format(x, y, z, value, expect))

        rdtest.log.success("Dispatch buffer output is correct")

        # The final draw is in indeterminate order because the parameters are defined by a compute shader in
        # indeterminate order
        # If the vertex buffer is referenced in the wrong order (e.g. by cached draw parameters) it will show exploding
        # polygons. To check the behaviour we can't require any given draw appear in any given place. However each
        # time we replay it should be self-consistent - after selecting the 4th draw then eactly 4 draws should appear.
        # And of course no exploding polys!

        action = self.find_action("Custom order draw")
        action = self.find_action("IndirectDraw", action.eventId)

        drawPoints = [
            (60, 20),
            (210, 20),
            (360, 20),

            (60, 130),
            (360, 130),

            (60, 240),
            (210, 240),
            (360, 240),
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
                        "With {} selected we should have {} draws, but counted {} draws".format(action.GetName(sdfile),
                                                                                                drawNum + 1, count))

                rdtest.log.print("With draw #{} selected we saw draws {} active".format(drawNum, str(draws)))

                # the exploded verts are calibrated to render as purple. We don't handle the case where exploding polys
                # reference vertices from other draws, but this _should_ not happen as we leave a large margin between
                # each draw's segments

                data = self.controller.GetTextureData(out, rd.Subresource(0, 0, 0))
                tex = self.get_texture(out)
                rdtest.log.print("{} - {} {} ".format(len(data), tex.width, tex.height))
                pixels = [struct.unpack_from("4B", data, 4 * p) for p in range(int(tex.width * tex.height))]
                unique_pixels = list(set(pixels))

                if (255, 0, 255, 255) in unique_pixels:
                    raise rdtest.TestFailureException(
                        "Detected an exploded polygon with {} selected".format(action.GetName(sdfile)))

            rdtest.log.success(f"Pass {passNum} of unordered draw was correct")

        # This does not draw anything but its argument buffer is fully used with no spare bytes
        # Iterate over every draw and check the replay has valid output target
        action = self.find_action("Full Arg Buffer")
        action = self.find_action("IndirectDraw", action.eventId)
        for drawNum in range(3):
            self.controller.SetFrameEvent(action.eventId + drawNum, False)
            pipe = self.controller.GetPipelineState()
            if len(pipe.GetOutputTargets()) != 1:
                raise rdtest.TestFailureException(
                    f"With event {action.eventId + drawNum} selected we should have one output target but there is {len(pipe.GetOutputTargets())}")
        rdtest.log.success("Fully used argument buffer with multiple draws replayed")

        # This does not draw anything but its argument buffer is fully used with no spare bytes
        # Iterate over every draw and check the replay has valid output target
        action = self.find_action("Full Arg Buffer: State + Draw")
        action = self.find_action("IndirectDraw", action.eventId)
        for drawNum in range(3):
            self.controller.SetFrameEvent(action.eventId + drawNum, False)
            pipe = self.controller.GetPipelineState()
            if len(pipe.GetOutputTargets()) != 1:
                raise rdtest.TestFailureException(
                    f"With event {action.eventId + drawNum} selected we should have one output target but there is {len(pipe.GetOutputTargets())}")
        rdtest.log.success("Fully used argument buffer with multiple states + draws replayed")
