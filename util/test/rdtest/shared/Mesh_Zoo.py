import renderdoc as rd
import rdtest

# Not a real test, re-used by API-specific tests
class Mesh_Zoo():
    def __init__(self):
        self.out = None
        self.cfg = rd.MeshDisplay()

    def cache_output(self):
        self.out.SetMeshDisplay(self.cfg)

        self.out.Display()

        pixels: bytes = self.out.ReadbackOutputTexture()
        dim = self.out.GetDimensions()

        pitch = dim[0]*3
        self.rows = [pixels[row_start:row_start + pitch] for row_start in range(0, dim[1] * pitch, pitch)]

        rdtest.png_save(rdtest.get_tmp_path('output.png'), self.rows, dim, False)

    def find_action(self, name):
        action = None

        for d in self.controller.GetRootActions():
            if name in d.customName:
                action = d
                break

        if action is None:
            raise rdtest.TestFailureException("Couldn't find '{}' action".format(name))

        return action

    # To avoid needing to do image comparisons, we instead do quad region probes to see which colours are present. That
    # way we can programmatically check that the wireframe we expect to be there, is there
    def get_region_cols(self, region):
        x0, y0, x1, y1 = region
        cols = []
        for y in range(y0, y1+1):
            for x in range(x0, x1+1):
                col = tuple(self.rows[y][x*3:x*3+3])

                # skip pure gray, this comes from the checkerboard or frustum, all our lines and data are coloured
                if col[0] == col[1] and col[1] == col[2]:
                    continue

                if col not in cols:
                    cols.append(col)
        return cols

    def check_region(self, region, test):
        colors = self.get_region_cols(region)

        if not test(colors):
            tmp_path = rdtest.get_tmp_path('output.png')
            rdtest.png_save(tmp_path, self.rows, self.out.GetDimensions(), False)
            raise rdtest.TestFailureException("Expected line segment wrong, colors: {}".format(colors), tmp_path)

    def check_vertex(self, x, y, result):
        pick = self.out.PickVertex(x, y)

        if not rdtest.value_compare(result, pick):
            raise rdtest.TestFailureException("When picking ({},{}) expected vertex {} in instance {}, but found {} in {}".format(x, y, result[0], result[1], pick[0], pick[1]))

        rdtest.log.success("Picking {},{} returns vertex {} in instance {} as expected".format(x, y, result[0], result[1]))

    def check_capture(self, capture_filename: str, controller: rd.ReplayController):
        self.controller = controller

        self.controller.SetFrameEvent(self.find_action("Quad").next.eventId, False)

        self.out: rd.ReplayOutput = self.controller.CreateOutput(rd.CreateHeadlessWindowingData(200, 200),
                                                            rd.ReplayOutputType.Mesh)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        self.cfg = rd.MeshDisplay()

        cam: rd.Camera = rd.InitCamera(rd.CameraType.FPSLook)

        cam.SetPosition(0, 0, 0)
        cam.SetFPSRotation(0, 0, 0)

        self.cfg.type = rd.MeshDataStage.VSOut
        self.cfg.cam = cam

        # Position is always first, so getting the postvs data will give us
        inst0: rd.MeshFormat = self.controller.GetPostVSData(0, 0, self.cfg.type)
        self.cfg.position = inst0

        # after position we have float2 Color2 then float4 Color4
        self.cfg.second = self.cfg.position
        self.cfg.second.vertexByteOffset += 16
        self.cfg.second.vertexByteOffset += 8
        if pipe.HasAlignedPostVSData(self.cfg.type):
            self.cfg.second.vertexByteOffset += 8

        # Configure an ortho camera, even though we don't really have a camera
        self.cfg.ortho = True
        self.cfg.position.nearPlane = 1.0
        self.cfg.position.farPlane = 100.0
        self.cfg.aspect = 1.0
        self.cfg.wireframeDraw = True
        self.cfg.position.meshColor = rd.FloatVector(1.0, 0.0, 1.0, 1.0)

        self.cache_output()

        # We should have a single quad, check each outside edge and the inside diagonal.
        # All these line segments should have some colors (not including the background checkerboard or the frustum)
        self.check_region((55, 95, 65, 95), lambda x: x != [])  # Left edge
        self.check_region((85, 60, 85, 70), lambda x: x != [])  # Top edge
        self.check_region((105, 100, 115, 100), lambda x: x != [])  # Right edge
        self.check_region((90, 130, 90, 140), lambda x: x != [])  # Bottom edge
        self.check_region((65, 120, 75, 120), lambda x: x != [])  # Bottom-Left of diagonal
        self.check_region((105, 70, 110, 70), lambda x: x != [])  # Top-right of diagonal

        rdtest.log.success("Base rendering is as expected")

        self.cfg.visualisationMode = rd.Visualisation.Secondary
        self.cfg.wireframeDraw = False

        # allow for blending with white for the frustum
        isred = lambda col: col[0] > col[1] and col[1] == col[2]
        isgreen = lambda col: col[1] > col[0] and col[0] == col[2]
        isblue = lambda col: col[2] > col[0] and col[0] == col[1]

        isredgreen = lambda col: isred(col) or isgreen(col) or col[2] == 0

        isyellow = lambda col: col[0] == col[1] and col[2] < col[1]

        self.cache_output()

        # The secondary color should be completely green
        self.check_region((85, 70, 85, 125), lambda x: all([isgreen(i) for i in x]))
        self.check_region((65, 100, 105, 100), lambda x: all([isgreen(i) for i in x]))
        # this line segment isn't in the first instance
        self.check_region((65, 55, 105, 55), lambda x: x == [])
        # this line segment isn't in the second instance
        self.check_region((65, 125, 105, 125), lambda x: all([isgreen(i) for i in x]))

        rdtest.log.success("Secondary rendering of instance 0 is as expected")

        # Out of bounds should look the same as without highlighting at all, check the corners are all still green
        self.cfg.highlightVert = 9

        self.cache_output()

        self.check_region((55, 60, 65, 70), lambda x: all([isgreen(i) for i in x]))
        self.check_region((105, 60, 115, 70), lambda x: all([isgreen(i) for i in x]))
        self.check_region((55, 130, 65, 140), lambda x: all([isgreen(i) for i in x]))
        self.check_region((105, 130, 115, 140), lambda x: all([isgreen(i) for i in x]))

        vert_regions = [
            (55, 60, 65, 70),
            (110, 60, 120, 70),
            (55, 130, 65, 140),

            (110, 60, 120, 70),
            (110, 130, 120, 140),
            (55, 130, 65, 140),
        ]

        for vert in range(6):
            self.cfg.highlightVert = vert

            self.cache_output()

            tri = int(vert / 3)

            # Check that the triangle we're highlighting is red and the other is green
            if tri == 0:
                self.check_region((65, 75, 75, 85), lambda x: all([isred(i) for i in x]))
                self.check_region((100, 115, 110, 125), lambda x: all([isgreen(i) for i in x]))
            else:
                self.check_region((65, 75, 75, 85), lambda x: all([isgreen(i) for i in x]))
                self.check_region((100, 115, 110, 125), lambda x: all([isred(i) for i in x]))

            # The corners that touch should be red and green - that is no other colours but red and green, but at least
            # some red and some green
            self.check_region((65, 115, 75, 125),
                              lambda x: all([isredgreen(i) for i in x]) and
                                        any([isred(i) for i in x]) and
                                        any([isgreen(i) for i in x]))

            # check that there's blue in this vertex's region
            self.check_region(vert_regions[vert], lambda x: any([isblue(i) for i in x]))

        rdtest.log.success("Rendering of highlighted vertices is as expected")

        self.cfg.highlightVert = rd.MeshDisplay.NoHighlight

        # If we render from the float2 color we shouldn't get any blue
        self.cfg.second.vertexByteOffset = self.cfg.position.vertexByteOffset = inst0.vertexByteOffset
        self.cfg.second.vertexByteOffset += 16
        self.cfg.second.format.compCount = 2

        self.cache_output()

        # If we render from the float2 color we shouldn't get any blue since it's only a two-component value
        self.check_region((85, 70, 85, 125), lambda x: all([isredgreen(i) for i in x]))
        self.check_region((65, 100, 105, 100), lambda x: all([isredgreen(i) for i in x]))
        self.check_region((65, 55, 105, 55), lambda x: x == [])
        self.check_region((65, 125, 105, 125), lambda x: all([isredgreen(i) for i in x]))

        rdtest.log.success("Rendering of float2 color secondary in instance 0 is as expected")

        self.cfg.highlightVert = rd.MeshDisplay.NoHighlight
        inst1: rd.MeshFormat = self.controller.GetPostVSData(1, 0, self.cfg.type)

        self.cfg.curInstance = 1
        self.cfg.second.vertexResourceId = self.cfg.position.vertexResourceId = inst1.vertexResourceId
        self.cfg.second.vertexByteOffset = self.cfg.position.vertexByteOffset = inst1.vertexByteOffset
        self.cfg.second.vertexByteOffset += 16
        self.cfg.second.vertexByteOffset += 8
        if pipe.HasAlignedPostVSData(self.cfg.type):
            self.cfg.second.vertexByteOffset += 8

        self.cache_output()

        # The secondary color should be completely yellow
        self.check_region((85, 70, 85, 125), lambda x: all([isyellow(i) for i in x]))
        self.check_region((65, 100, 105, 100), lambda x: all([isyellow(i) for i in x]))
        # this line segment isn't in the first instance
        self.check_region((65, 55, 105, 55), lambda x: all([isyellow(i) for i in x]))
        # this line segment isn't in the second instance
        self.check_region((65, 125, 105, 125), lambda x: x == [])

        rdtest.log.success("Secondary rendering of instance 1 is as expected")

        # If we render from the float2 color we shouldn't get any blue
        self.cfg.second.vertexByteOffset = self.cfg.position.vertexByteOffset = inst1.vertexByteOffset
        self.cfg.second.vertexByteOffset += 16
        self.cfg.second.format.compCount = 2

        self.cache_output()

        # If we render from the float2 color we shouldn't get any blue since it's only a two-component value
        self.check_region((85, 70, 85, 125), lambda x: all([isredgreen(i) for i in x]))
        self.check_region((65, 100, 105, 100), lambda x: all([isredgreen(i) for i in x]))
        self.check_region((65, 55, 105, 55), lambda x: all([isredgreen(i) for i in x]))
        self.check_region((65, 125, 105, 125), lambda x: x == [])

        rdtest.log.success("Rendering of float2 color secondary in instance 1 is as expected")

        self.cfg.visualisationMode = rd.Visualisation.NoSolid
        self.cfg.showAllInstances = True

        self.cache_output()

        # wireframe for original quad should still be present
        self.check_region((55, 95, 65, 95), lambda x: x != [])
        self.check_region((85, 60, 85, 70), lambda x: x != [])
        self.check_region((105, 100, 115, 100), lambda x: x != [])
        self.check_region((90, 130, 90, 140), lambda x: x != [])
        self.check_region((65, 120, 75, 120), lambda x: x != [])
        self.check_region((105, 70, 110, 70), lambda x: x != [])

        # But now we'll have an additional instance
        self.check_region((75, 55, 85, 55), lambda x: x != [])
        self.check_region((125, 85, 135, 85), lambda x: x != [])
        self.check_region((105, 110, 105, 120), lambda x: x != [])

        self.cfg.showWholePass = True

        self.cache_output()

        # same again
        self.check_region((55, 95, 65, 95), lambda x: x != [])
        self.check_region((85, 60, 85, 70), lambda x: x != [])
        self.check_region((105, 100, 115, 100), lambda x: x != [])
        self.check_region((90, 130, 90, 140), lambda x: x != [])
        self.check_region((65, 120, 75, 120), lambda x: x != [])
        self.check_region((105, 70, 110, 70), lambda x: x != [])
        self.check_region((75, 55, 85, 55), lambda x: x != [])
        self.check_region((125, 85, 135, 85), lambda x: x != [])
        self.check_region((105, 110, 105, 120), lambda x: x != [])

        # But now an extra previous action
        self.check_region((30, 105, 40, 105), lambda x: x != [])
        self.check_region((50, 80, 50, 90), lambda x: x != [])
        self.check_region((45, 130, 55, 130), lambda x: x != [])
        self.check_region((30, 150, 40, 150), lambda x: x != [])

        rdtest.log.success("Mesh rendering is as expected")

        self.cfg.showWholePass = False
        self.cfg.showAllInstances = False

        # Go back to instance 0. We can ignore cfg.second now
        self.cfg.curInstance = 0
        self.cfg.position.vertexResourceId = inst0.vertexResourceId
        self.cfg.position.vertexByteOffset = inst0.vertexByteOffset

        self.cache_output()

        # Just above top-left, no result
        self.check_vertex(55, 60, (rd.ReplayOutput.NoResult, rd.ReplayOutput.NoResult))
        # Just inside top-left, first vertex
        self.check_vertex(65, 70, (0, 0))
        # Outside top-right, inside the second instance, but because we only have one instance showing should return
        # no result
        self.check_vertex(115, 60, (rd.ReplayOutput.NoResult, rd.ReplayOutput.NoResult))
        self.check_vertex(80, 60, (rd.ReplayOutput.NoResult, rd.ReplayOutput.NoResult))
        # In the first triangle near the top right
        self.check_vertex(105, 70, (1, 0))
        # In the second triangle near the top right
        self.check_vertex(110, 70, (3, 0))
        # In the second triangle near the middle, would be in the second instance
        self.check_vertex(95, 110, (4, 0))
        # In the second triangle near the bottom right
        self.check_vertex(110, 130, (4, 0))

        rdtest.log.success("Instance 0 picking is as expected")

        # if we look at only instance 1, the results should change
        self.cfg.curInstance = 1
        self.cfg.position.vertexResourceId = inst1.vertexResourceId
        self.cfg.position.vertexByteOffset = inst1.vertexByteOffset

        self.cache_output()

        self.check_vertex(55, 60, (rd.ReplayOutput.NoResult, rd.ReplayOutput.NoResult))
        self.check_vertex(65, 70, (rd.ReplayOutput.NoResult, rd.ReplayOutput.NoResult))
        self.check_vertex(115, 60, (1, 1))
        self.check_vertex(80, 60, (0, 1))
        self.check_vertex(105, 70, (1, 1))
        self.check_vertex(110, 70, (1, 1))
        self.check_vertex(95, 110, (5, 1))
        self.check_vertex(110, 130, (rd.ReplayOutput.NoResult, rd.ReplayOutput.NoResult))

        rdtest.log.success("Instance 1 picking is as expected")

        # Now look at both instances together, this goes 'in order' so if there is overlap the first instance wins
        self.cfg.showAllInstances = True

        self.cache_output()

        self.check_vertex(55, 60, (rd.ReplayOutput.NoResult, rd.ReplayOutput.NoResult))
        self.check_vertex(65, 70, (0, 0))
        self.check_vertex(115, 60, (1, 1))
        self.check_vertex(80, 60, (0, 1))
        self.check_vertex(105, 70, (1, 0))
        self.check_vertex(110, 70, (3, 0))
        self.check_vertex(95, 110, (4, 0))
        self.check_vertex(110, 130, (4, 0))

        rdtest.log.success("Both instance picking is as expected")

        self.controller.SetFrameEvent(self.find_action("Points").next.eventId, False)

        # Only one instance, just check we can see the points
        self.cfg.curInstance = 0
        self.cfg.position = self.controller.GetPostVSData(0, 0, self.cfg.type)
        self.cfg.position.nearPlane = 1.0
        self.cfg.position.farPlane = 100.0

        self.cache_output()

        # Picking points doesn't have any primitive, it should pick as long as it's close to the point
        self.check_vertex(55, 60, (0, 0))
        self.check_vertex(65, 70, (0, 0))

        self.check_vertex(105, 65, (1, 0))
        self.check_vertex(115, 135, (2, 0))
        self.check_vertex(65, 130, (3, 0))
        self.check_vertex(60, 125, (3, 0))

        rdtest.log.success("Point picking is as expected")

        self.cfg.highlightVert = rd.MeshDisplay.NoHighlight
        self.cfg.visualisationMode = rd.Visualisation.Solid

        self.cache_output()
        self.cfg.visualisationMode = rd.Visualisation.Lit
        self.cache_output()

        rdtest.log.success("Point solid and lit rendering works as expected")

        self.controller.SetFrameEvent(self.find_action("Lines").next.eventId, False)

        self.cache_output()
        self.cfg.visualisationMode = rd.Visualisation.Lit
        self.cache_output()

        rdtest.log.success("Lines solid and lit rendering works as expected")

        self.controller.SetFrameEvent(self.find_action("Stride 0").next.eventId, False)

        self.cfg.position = self.controller.GetPostVSData(0, 0, self.cfg.type)
        self.cfg.position.nearPlane = 1.0
        self.cfg.position.farPlane = 100.0

        self.cache_output()

        # Stride of 0 is unusual but valid, ensure vertex picking still works
        self.check_vertex(55, 60, (0, 0))
        self.check_vertex(65, 70, (0, 0))

        self.check_vertex(105, 65, (rd.ReplayOutput.NoResult, rd.ReplayOutput.NoResult))
        self.check_vertex(115, 135, (rd.ReplayOutput.NoResult, rd.ReplayOutput.NoResult))

        self.controller.SetFrameEvent(self.find_action("Empty").next.eventId, False)

        self.cfg.position = self.controller.GetPostVSData(0, 0, self.cfg.type)
        self.cfg.position.nearPlane = 1.0
        self.cfg.position.farPlane = 100.0

        self.cache_output()

        self.check_vertex(105, 65, (rd.ReplayOutput.NoResult, rd.ReplayOutput.NoResult))
        self.check_vertex(115, 135, (rd.ReplayOutput.NoResult, rd.ReplayOutput.NoResult))

        rdtest.log.success("Picking in empty draw is as expected")

